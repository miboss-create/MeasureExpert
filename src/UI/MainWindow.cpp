#include "MainWindow.h"

#include "CustomImageView.h"
#include "LogWidget.h"
#include "ThumbnailStrip.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setMinimumSize(1200, 720);
    setWindowTitle(tr("MeasureExpert - 工业通用测量专家"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.png")));

    createMenus();

    if (!initWidget()) {
        auto *fallback = new QTextEdit(this);
        setCentralWidget(fallback);
        fallback->setPlainText(tr("界面初始化失败。"));
        return;
    }

    createToolBar();
    createStatusBar();
}

MainWindow::~MainWindow() = default;

bool MainWindow::initWidget()
{
    auto *configPanel = new QWidget(this);
    auto *configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(4, 4, 4, 4);

    configLayout->addWidget(new QLabel(tr("配方"), configPanel));
    m_recipeList = new QListWidget(configPanel);
    m_recipeList->addItem(tr("(暂无配方)"));
    configLayout->addWidget(m_recipeList, 1);

    configLayout->addWidget(new QLabel(tr("测量项"), configPanel));
    m_measureList = new QListWidget(configPanel);
    m_measureList->addItem(tr("(暂无测量项)"));
    configLayout->addWidget(m_measureList, 2);

    m_logWidget = new LogWidget(this);

    auto *leftSplitter = new QSplitter(Qt::Vertical, this);
    leftSplitter->addWidget(configPanel);
    leftSplitter->addWidget(m_logWidget);
    leftSplitter->setStretchFactor(0, 3);
    leftSplitter->setStretchFactor(1, 2);

    m_sourceThumbs = new ThumbnailStrip(ThumbnailStrip::Orientation::Vertical, this);
    connect(m_sourceThumbs, &ThumbnailStrip::currentImageChanged,
            this, &MainWindow::onThumbImageChanged);
    connect(m_sourceThumbs, &ThumbnailStrip::currentPathChanged,
            this, &MainWindow::onThumbPathChanged);
    connect(m_sourceThumbs, &ThumbnailStrip::currentIndexChanged,
            this, &MainWindow::onSourceIndexChanged);

    m_imageView = new CustomImageView(this);
    m_imageView->setMinimumSize(400, 300);

    // 结果横条暂用原图缩略图代替，与源图条同步
    m_resultThumbs = new ThumbnailStrip(ThumbnailStrip::Orientation::Horizontal, this);
    connect(m_resultThumbs, &ThumbnailStrip::currentImageChanged,
            this, &MainWindow::onThumbImageChanged);
    connect(m_resultThumbs, &ThumbnailStrip::currentPathChanged,
            this, &MainWindow::onThumbPathChanged);
    connect(m_resultThumbs, &ThumbnailStrip::currentIndexChanged,
            this, &MainWindow::onResultIndexChanged);

    auto *imageColumn = new QWidget(this);
    auto *imageColLayout = new QVBoxLayout(imageColumn);
    imageColLayout->setContentsMargins(0, 0, 0, 0);
    imageColLayout->setSpacing(4);
    imageColLayout->addWidget(m_imageView, 1);
    imageColLayout->addWidget(m_resultThumbs);

    auto *centerWidget = new QWidget(this);
    auto *centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(4);
    centerLayout->addWidget(m_sourceThumbs);
    centerLayout->addWidget(imageColumn, 1);

    m_okNgLabel = new QLabel(tr("---"), this);
    m_okNgLabel->setAlignment(Qt::AlignCenter);
    m_okNgLabel->setMinimumHeight(80);
    m_okNgLabel->setStyleSheet(
        QStringLiteral("QLabel { font-size: 36px; font-weight: bold; background: #3a3a3a; color: #dddddd; }"));

    m_resultTable = new QTableWidget(0, 4, this);
    m_resultTable->setHorizontalHeaderLabels(
        {tr("名称"), tr("测量值"), tr("公差"), tr("判定")});

    m_propertyPanel = new QTextEdit(this);
    m_propertyPanel->setReadOnly(true);
    m_propertyPanel->setPlaceholderText(tr("测量项属性（名义/公差）"));
    m_propertyPanel->setMaximumHeight(160);

    auto *judgePanel = new QWidget(this);
    auto *judgeLayout = new QVBoxLayout(judgePanel);
    judgeLayout->setContentsMargins(4, 4, 4, 4);
    judgeLayout->addWidget(m_okNgLabel);
    judgeLayout->addWidget(m_resultTable, 1);

    auto *rightSplitter = new QSplitter(Qt::Vertical, this);
    rightSplitter->addWidget(judgePanel);
    rightSplitter->addWidget(m_propertyPanel);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 1);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(leftSplitter);
    m_mainSplitter->addWidget(centerWidget);
    m_mainSplitter->addWidget(rightSplitter);
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 5);
    m_mainSplitter->setStretchFactor(2, 2);
    m_mainSplitter->setSizes({280, 700, 280});

    setCentralWidget(m_mainSplitter);
    return true;
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("新建工程"), this, [] {});
    fileMenu->addAction(tr("打开工程"), this, [] {});
    fileMenu->addAction(tr("保存工程"), this, [] {});
    fileMenu->addSeparator();
    fileMenu->addAction(tr("加载图像..."), this, &MainWindow::onLoadImage);
    fileMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), this, &QWidget::close);

    auto *acqMenu = menuBar()->addMenu(tr("采集(&A)"));
    acqMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);

    menuBar()->addMenu(tr("标定(&C)"));
    menuBar()->addMenu(tr("运行(&R)"));
    menuBar()->addMenu(tr("工具(&T)"));
    menuBar()->addMenu(tr("帮助(&H)"));
}

void MainWindow::createToolBar()
{
    auto *tb = addToolBar(tr("主工具栏"));
    tb->setMovable(false);
    tb->addAction(tr("新建"));
    tb->addAction(tr("打开"));
    tb->addAction(tr("保存"));
    tb->addSeparator();
    tb->addAction(tr("加载图"), this, &MainWindow::onLoadImage);
    tb->addAction(tr("加载文件夹"), this, &MainWindow::onLoadFolder);
    tb->addAction(tr("上一张"), m_sourceThumbs, &ThumbnailStrip::selectPrevious);
    tb->addAction(tr("下一张"), m_sourceThumbs, &ThumbnailStrip::selectNext);
    tb->addSeparator();
    tb->addAction(tr("单次试跑"));
    tb->addAction(tr("连续运行"));
    tb->addAction(tr("停止"));
    tb->addSeparator();
    tb->addAction(tr("编辑⇄运行"));
}

void MainWindow::createStatusBar()
{
    updateImageSourceStatus(tr("未设置"));
}

void MainWindow::updateImageSourceStatus(const QString &source)
{
    statusBar()->showMessage(
        tr("图像源: %1 | 标定: 未标定(px) | 节拍: - | 模式: 编辑 | 通信: 预留").arg(source));
}

void MainWindow::syncThumbSelection(ThumbnailStrip *from, ThumbnailStrip *to, int index)
{
    Q_UNUSED(from);
    if (!to || m_syncingThumbs) {
        return;
    }
    m_syncingThumbs = true;
    to->setCurrentIndex(index);
    m_syncingThumbs = false;
}

void MainWindow::onLoadImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("加载图像"),
        QString(),
        tr("图像文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    m_imageView->setImagePath(path);
    qInfo().noquote() << tr("已加载图像: %1").arg(path);
    updateImageSourceStatus(path);
}

void MainWindow::onLoadFolder()
{
    if (!m_sourceThumbs->openFolderDialog()) {
        if (!m_sourceThumbs->folderPath().isEmpty() && m_sourceThumbs->count() == 0) {
            qWarning().noquote() << tr("文件夹内未找到可识别图像");
        }
        return;
    }

    // 结果横条暂复用同一批原图
    m_resultThumbs->loadFolder(m_sourceThumbs->folderPath());
    syncThumbSelection(m_sourceThumbs, m_resultThumbs, m_sourceThumbs->currentIndex());

    qInfo().noquote() << tr("已加载文件夹: %1（%2 张）")
                             .arg(m_sourceThumbs->folderPath())
                             .arg(m_sourceThumbs->count());
}

void MainWindow::onThumbImageChanged(const QImage &image)
{
    m_imageView->setImage(image);
}

void MainWindow::onThumbPathChanged(const QString &path)
{
    updateImageSourceStatus(path);
    qInfo().noquote() << tr("当前图像: %1").arg(QFileInfo(path).fileName());
}

void MainWindow::onSourceIndexChanged(int index)
{
    syncThumbSelection(m_sourceThumbs, m_resultThumbs, index);
}

void MainWindow::onResultIndexChanged(int index)
{
    syncThumbSelection(m_resultThumbs, m_sourceThumbs, index);
}
