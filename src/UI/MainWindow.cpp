#include "MainWindow.h"

#include "Alg/AlignmentService.h"
#include "Alg/RunEngine.h"
#include "Core/ProjectService.h"
#include "CustomImageView.h"
#include "LogWidget.h"
#include "ThumbnailStrip.h"
#include "UI/CalibrationDialog.h"
#include "UI/EditMeasureDialog.h"
#include "UI/ResultOverlay.h"
#include "UI/TrainAlignmentDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

using Me::CalibrationDialog;
using Me::EditMeasureDialog;
using Me::MeasureType;
using Me::ProjectService;
using Me::RunEngine;
using Me::TrainAlignmentDialog;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_projectService(new ProjectService(this))
{
    qRegisterMetaType<Me::CycleResult>("Me::CycleResult");
    qRegisterMetaType<Me::MeasureResult>("Me::MeasureResult");
    qRegisterMetaType<QVector<Me::MeasureResult>>("QVector<Me::MeasureResult>");
    qRegisterMetaType<Me::Recipe>("Me::Recipe");
    qRegisterMetaType<Me::CalibrationConfig>("Me::CalibrationConfig");

    setMinimumSize(1200, 720);
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
    setupRunEngine();

    connect(m_projectService, &ProjectService::structureChanged,
            this, &MainWindow::onProjectStructureChanged);
    connect(m_projectService, &ProjectService::dirtyChanged,
            this, &MainWindow::onDirtyChanged);
    connect(m_projectService, &ProjectService::projectLoaded,
            this, [this](const QString &path) {
                qInfo().noquote() << tr("已打开工程: %1").arg(path);
                maybeLoadImageFolderFromProject();
            });
    connect(m_projectService, &ProjectService::projectSaved,
            this, [this](const QString &path) {
                qInfo().noquote() << tr("已保存工程: %1").arg(path);
            });

    onProjectStructureChanged();
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    if (m_runThread) {
        m_runThread->quit();
        m_runThread->wait(3000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        QMessageBox::information(this, tr("提示"), tr("正在试跑，请稍候再关闭。"));
        event->ignore();
        return;
    }
    if (!maybeSaveBeforeContinue()) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_imageView && m_imageView->isRoiEditEnabled()) {
        onCancelRoiEdit();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::updateRoiActionTexts()
{
    if (m_trainAlignAction) {
        if (m_roiEditMode == RoiEditMode::AlignTemplate) {
            m_trainAlignAction->setText(tr("确认训练模板"));
        } else {
            m_trainAlignAction->setText(tr("创建定位模板"));
        }
    }
    const QString measureText = (m_roiEditMode == RoiEditMode::MeasureCaliper)
        ? tr("确认卡尺ROI")
        : tr("画卡尺ROI");
    if (m_drawMeasureRoiAction) {
        m_drawMeasureRoiAction->setText(measureText);
    }
    if (m_drawMeasureRoiBtn) {
        m_drawMeasureRoiBtn->setText(measureText);
    }
}

void MainWindow::enterRoiEditMode(bool forMeasure)
{
    m_roiEditMode = forMeasure ? RoiEditMode::MeasureCaliper : RoiEditMode::AlignTemplate;
    if (forMeasure) {
        m_imageView->setRoiStyle(QColor(255, 200, 0), tr("卡尺"));
    } else {
        m_imageView->setRoiStyle(QColor(0, 220, 120), tr("模板"));
    }

    if (!m_imageView->hasRoi()) {
        const int w = m_currentImage.width();
        const int h = m_currentImage.height();
        m_imageView->setRoiImageRect(QRectF(w * 0.25, h * 0.25, w * 0.5, h * 0.5));
    }
    m_imageView->setRoiEditEnabled(true);
    if (m_cancelRoiAction) {
        m_cancelRoiAction->setEnabled(true);
    }
    updateRoiActionTexts();
    if (forMeasure) {
        statusBar()->showMessage(
            tr("卡尺 ROI：左键拖拽绘制，右键确认后可拖图并调整黄框；选中右键可删除。调好后点「确认卡尺ROI」。Esc 取消。"),
            0);
        qInfo().noquote() << tr("已进入卡尺 ROI 编辑：请在主图上调整黄色矩形");
    } else {
        statusBar()->showMessage(
            tr("模板 ROI：左键拖拽绘制，右键确认后可拖图并调整绿框；选中右键可删除。调好后点「确认训练模板」。Esc 取消。"),
            0);
        qInfo().noquote() << tr("已进入定位 ROI 编辑：请在主图上调整绿色矩形");
    }
}

void MainWindow::leaveRoiEditMode(bool clearRoi)
{
    if (!m_imageView) {
        return;
    }
    m_roiEditMode = RoiEditMode::None;
    m_imageView->setRoiEditEnabled(false);
    if (clearRoi) {
        m_imageView->clearRoi();
    }
    m_imageView->setRoiStyle(QColor(0, 220, 120), tr("模板"));
    if (m_cancelRoiAction) {
        m_cancelRoiAction->setEnabled(false);
    }
    updateRoiActionTexts();
    updateCalibrationStatus();
}

void MainWindow::onCancelRoiEdit()
{
    leaveRoiEditMode(true);
    qInfo().noquote() << tr("已取消 ROI 编辑");
}

QRectF MainWindow::measureGeometryToImageRect(const Me::CaliperGeometry &geo,
                                             const Me::AlignmentResult &align) const
{
    const double cA = qCos(align.angleRad);
    const double sA = qSin(align.angleRad);
    const double rowImg = cA * geo.row - sA * geo.column + align.row;
    const double colImg = sA * geo.row + cA * geo.column + align.column;
    const double w = qMax(8.0, geo.length1 * 2.0);
    const double h = qMax(8.0, geo.length2 * 2.0);
    return QRectF(colImg - w * 0.5, rowImg - h * 0.5, w, h);
}

bool MainWindow::imageRectToMeasureGeometry(const QRectF &imageRect,
                                           const Me::AlignmentResult &align,
                                           Me::CaliperGeometry *out) const
{
    if (!out || !align.ok || !imageRect.isValid()) {
        return false;
    }
    const QRectF r = imageRect.normalized();
    const double rowImg = r.center().y();
    const double colImg = r.center().x();
    const double cA = qCos(align.angleRad);
    const double sA = qSin(align.angleRad);
    const double dRow = rowImg - align.row;
    const double dCol = colImg - align.column;

    // 画出来的是图像轴对齐框 → 相对模板原点；phi 使运行时卡尺仍轴对齐于当前图
    out->row = cA * dRow + sA * dCol;
    out->column = -sA * dRow + cA * dCol;
    out->phi = -align.angleRad;
    out->length1 = qMax(2.0, r.width() * 0.5);
    out->length2 = qMax(2.0, r.height() * 0.5);
    return true;
}

void MainWindow::setupRunEngine()
{
    m_runThread = new QThread(this);
    m_runEngine = new RunEngine();
    m_runEngine->moveToThread(m_runThread);

    connect(m_runThread, &QThread::finished, m_runEngine, &QObject::deleteLater);
    connect(m_runEngine, &RunEngine::cycleFinished, this, &MainWindow::onCycleFinished);
    connect(m_runEngine, &RunEngine::errorOccurred, this, &MainWindow::onRunError);

    m_runThread->start();
}

bool MainWindow::initWidget()
{
    auto *configPanel = new QWidget(this);
    auto *configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(4, 4, 4, 4);

    configLayout->addWidget(new QLabel(tr("配方"), configPanel));
    m_recipeList = new QListWidget(configPanel);
    configLayout->addWidget(m_recipeList, 1);

    auto *recipeBtnRow = new QHBoxLayout();
    m_addRecipeBtn = new QPushButton(tr("添加"), configPanel);
    m_removeRecipeBtn = new QPushButton(tr("删除"), configPanel);
    recipeBtnRow->addWidget(m_addRecipeBtn);
    recipeBtnRow->addWidget(m_removeRecipeBtn);
    configLayout->addLayout(recipeBtnRow);

    configLayout->addWidget(new QLabel(tr("测量项"), configPanel));
    m_measureList = new QListWidget(configPanel);
    configLayout->addWidget(m_measureList, 2);

    auto *measureBtnRow = new QHBoxLayout();
    m_addMeasureBtn = new QPushButton(tr("添加"), configPanel);
    m_editMeasureBtn = new QPushButton(tr("编辑"), configPanel);
    m_drawMeasureRoiBtn = new QPushButton(tr("画卡尺ROI"), configPanel);
    m_removeMeasureBtn = new QPushButton(tr("删除"), configPanel);
    measureBtnRow->addWidget(m_addMeasureBtn);
    measureBtnRow->addWidget(m_editMeasureBtn);
    measureBtnRow->addWidget(m_drawMeasureRoiBtn);
    measureBtnRow->addWidget(m_removeMeasureBtn);
    configLayout->addLayout(measureBtnRow);

    connect(m_recipeList, &QListWidget::currentRowChanged,
            this, &MainWindow::onRecipeSelectionChanged);
    connect(m_measureList, &QListWidget::currentRowChanged,
            this, &MainWindow::onMeasureSelectionChanged);
    connect(m_measureList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onMeasureDoubleClicked);
    connect(m_addRecipeBtn, &QPushButton::clicked, this, &MainWindow::onAddRecipe);
    connect(m_removeRecipeBtn, &QPushButton::clicked, this, &MainWindow::onRemoveRecipe);
    connect(m_addMeasureBtn, &QPushButton::clicked, this, &MainWindow::onAddMeasure);
    connect(m_editMeasureBtn, &QPushButton::clicked, this, &MainWindow::onEditMeasure);
    connect(m_drawMeasureRoiBtn, &QPushButton::clicked, this, &MainWindow::onDrawMeasureRoi);
    connect(m_removeMeasureBtn, &QPushButton::clicked, this, &MainWindow::onRemoveMeasure);

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

    m_imageView = new CustomImageView(this);
    m_imageView->setMinimumSize(400, 300);
    connect(m_imageView, &CustomImageView::roiDrawConfirmed, this, [this]() {
        statusBar()->showMessage(
            tr("ROI 已确认：可拖动画布；点击 ROI 可移动/改大小；选中后右键可删除。"),
            0);
        qInfo().noquote() << tr("ROI 绘制已确认（右键），可拖图并继续调整 ROI");
    });
    connect(m_imageView, &CustomImageView::roiDeleted, this, [this]() {
        statusBar()->showMessage(
            tr("ROI 已删除。左键拖拽重新绘制，右键确认。"),
            0);
        qInfo().noquote() << tr("ROI 已删除");
    });

    auto *centerWidget = new QWidget(this);
    auto *centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(4);
    centerLayout->addWidget(m_sourceThumbs);
    centerLayout->addWidget(m_imageView, 1);

    m_okNgLabel = new QLabel(tr("---"), this);
    m_okNgLabel->setAlignment(Qt::AlignCenter);
    m_okNgLabel->setMinimumHeight(80);
    m_okNgLabel->setStyleSheet(
        QStringLiteral("QLabel { font-size: 36px; font-weight: bold; background: #3a3a3a; color: #dddddd; }"));

    m_resultTable = new QTableWidget(0, 4, this);
    m_resultTable->setHorizontalHeaderLabels(
        {tr("名称"), tr("测量值"), tr("公差"), tr("判定")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);

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
    fileMenu->addAction(tr("新建工程"), this, &MainWindow::onNewProject);
    fileMenu->addAction(tr("打开工程..."), this, &MainWindow::onOpenProject);
    fileMenu->addAction(tr("保存工程"), this, &MainWindow::onSaveProject);
    fileMenu->addAction(tr("工程另存为..."), this, &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("加载图像..."), this, &MainWindow::onLoadImage);
    fileMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), this, &QWidget::close);

    auto *acqMenu = menuBar()->addMenu(tr("采集(&A)"));
    acqMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);

    auto *calMenu = menuBar()->addMenu(tr("标定(&C)"));
    calMenu->addAction(tr("像素当量..."), this, &MainWindow::onCalibration);

    auto *runMenu = menuBar()->addMenu(tr("运行(&R)"));
    m_trainAlignAction = runMenu->addAction(tr("创建定位模板..."), this, &MainWindow::onTrainAlignment);
    m_drawMeasureRoiAction = runMenu->addAction(tr("画卡尺ROI..."), this, &MainWindow::onDrawMeasureRoi);
    m_cancelRoiAction = runMenu->addAction(tr("取消 ROI 编辑"), this, &MainWindow::onCancelRoiEdit);
    m_cancelRoiAction->setEnabled(false);
    runMenu->addAction(tr("编辑测量项..."), this, &MainWindow::onEditMeasure);
    runMenu->addSeparator();
    runMenu->addAction(tr("单次试跑"), this, &MainWindow::onRunOnce);

    menuBar()->addMenu(tr("工具(&T)"));
    menuBar()->addMenu(tr("帮助(&H)"));
}

void MainWindow::createToolBar()
{
    auto *tb = addToolBar(tr("主工具栏"));
    tb->setMovable(false);
    tb->addAction(tr("新建"), this, &MainWindow::onNewProject);
    tb->addAction(tr("打开"), this, &MainWindow::onOpenProject);
    tb->addAction(tr("保存"), this, &MainWindow::onSaveProject);
    tb->addSeparator();
    tb->addAction(tr("加载图"), this, &MainWindow::onLoadImage);
    tb->addAction(tr("加载文件夹"), this, &MainWindow::onLoadFolder);
    tb->addAction(tr("上一张"), m_sourceThumbs, &ThumbnailStrip::selectPrevious);
    tb->addAction(tr("下一张"), m_sourceThumbs, &ThumbnailStrip::selectNext);
    tb->addSeparator();
    tb->addAction(tr("标定"), this, &MainWindow::onCalibration);
    if (!m_trainAlignAction) {
        m_trainAlignAction = new QAction(tr("定位模板"), this);
        connect(m_trainAlignAction, &QAction::triggered, this, &MainWindow::onTrainAlignment);
    }
    tb->addAction(m_trainAlignAction);
    if (!m_drawMeasureRoiAction) {
        m_drawMeasureRoiAction = new QAction(tr("画卡尺ROI"), this);
        connect(m_drawMeasureRoiAction, &QAction::triggered, this, &MainWindow::onDrawMeasureRoi);
    }
    tb->addAction(m_drawMeasureRoiAction);
    tb->addAction(tr("单次试跑"), this, &MainWindow::onRunOnce);
    tb->addAction(tr("连续运行"));
    tb->addAction(tr("停止"));
    tb->addSeparator();
    tb->addAction(tr("编辑⇄运行"));
}

void MainWindow::createStatusBar()
{
    m_currentImageStatus = tr("未设置");
    updateCalibrationStatus();
}

void MainWindow::updateWindowTitle()
{
    QString title = tr("MeasureExpert - %1").arg(m_projectService->project().name);
    if (m_projectService->hasFilePath()) {
        title += QStringLiteral(" [%1]").arg(QFileInfo(m_projectService->filePath()).fileName());
    }
    if (m_projectService->isDirty()) {
        title += QStringLiteral(" *");
    }
    setWindowTitle(title);
}

void MainWindow::updateImageSourceStatus(const QString &source)
{
    m_currentImageStatus = source;
    updateCalibrationStatus();
}

void MainWindow::updateCalibrationStatus()
{
    const auto &cal = m_projectService->project().calibration;
    const QString calText = cal.calibrated
        ? tr("已标定 %1 mm/px (%2)").arg(cal.pixelSizeMm, 0, 'g', 6).arg(cal.unit)
        : tr("未标定(px)");
    statusBar()->showMessage(
        tr("图像源: %1 | 标定: %2 | 节拍: - | 模式: 编辑 | 通信: 预留")
            .arg(m_currentImageStatus, calText));
}

QString MainWindow::selectedRecipeId() const
{
    auto *item = m_recipeList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString MainWindow::selectedMeasureId() const
{
    auto *item = m_measureList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QString MainWindow::projectDir() const
{
    if (m_projectService->hasFilePath()) {
        return QFileInfo(m_projectService->filePath()).absolutePath();
    }
    return QCoreApplication::applicationDirPath();
}

void MainWindow::setRunningUi(bool running)
{
    m_running = running;
    m_addRecipeBtn->setEnabled(!running);
    m_removeRecipeBtn->setEnabled(!running && m_projectService->project().recipes.size() > 1);
    m_addMeasureBtn->setEnabled(!running);
    m_editMeasureBtn->setEnabled(!running && m_measureList->count() > 0);
    m_drawMeasureRoiBtn->setEnabled(!running && m_measureList->count() > 0);
    m_removeMeasureBtn->setEnabled(!running && m_measureList->count() > 0);
}

void MainWindow::refreshRecipeList()
{
    m_updatingLists = true;
    const QString keepId = selectedRecipeId().isEmpty()
        ? m_projectService->project().activeRecipeId
        : selectedRecipeId();

    m_recipeList->clear();
    for (const auto &recipe : m_projectService->project().recipes) {
        auto *item = new QListWidgetItem(recipe.name, m_recipeList);
        item->setData(Qt::UserRole, recipe.id);
        if (!recipe.enabled) {
            item->setForeground(Qt::gray);
        }
        if (!recipe.alignment.modelFile.isEmpty()) {
            item->setToolTip(tr("模型: %1").arg(recipe.alignment.modelFile));
        }
    }

    int row = 0;
    for (int i = 0; i < m_recipeList->count(); ++i) {
        if (m_recipeList->item(i)->data(Qt::UserRole).toString() == keepId) {
            row = i;
            break;
        }
    }
    if (m_recipeList->count() > 0) {
        m_recipeList->setCurrentRow(row);
        m_projectService->setActiveRecipeId(
            m_recipeList->item(row)->data(Qt::UserRole).toString(), false);
    }
    m_updatingLists = false;
    m_removeRecipeBtn->setEnabled(!m_running && m_projectService->project().recipes.size() > 1);
}

void MainWindow::refreshMeasureList()
{
    m_updatingLists = true;
    const QString keepMeasureId = selectedMeasureId();
    m_measureList->clear();

    const auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        m_updatingLists = false;
        m_removeMeasureBtn->setEnabled(false);
        m_editMeasureBtn->setEnabled(false);
        m_drawMeasureRoiBtn->setEnabled(false);
        refreshMeasureProperties();
        return;
    }

    int selectRow = 0;
    for (int i = 0; i < recipe->measures.size(); ++i) {
        const auto &m = recipe->measures.at(i);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 (%2)").arg(m.name, Me::measureTypeToString(m.type)),
            m_measureList);
        item->setData(Qt::UserRole, m.id);
        if (m.id == keepMeasureId) {
            selectRow = i;
        }
    }

    if (m_measureList->count() > 0) {
        m_measureList->setCurrentRow(selectRow);
    }
    m_updatingLists = false;
    m_removeMeasureBtn->setEnabled(!m_running && m_measureList->count() > 0);
    m_editMeasureBtn->setEnabled(!m_running && m_measureList->count() > 0);
    m_drawMeasureRoiBtn->setEnabled(!m_running && m_measureList->count() > 0);
    refreshMeasureProperties();
}

void MainWindow::refreshMeasureProperties()
{
    const auto *recipe = m_projectService->activeRecipe();
    const QString measureId = selectedMeasureId();
    if (!recipe || measureId.isEmpty()) {
        m_propertyPanel->clear();
        m_propertyPanel->setPlaceholderText(tr("测量项属性（名义/公差）"));
        return;
    }

    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        m_propertyPanel->clear();
        return;
    }

    const auto &m = recipe->measures.at(idx);
    const QString geometryText =
        QStringLiteral("  row=%1  col=%2  phi=%3\n  length1=%4  length2=%5")
            .arg(m.geometry.row)
            .arg(m.geometry.column)
            .arg(m.geometry.phi)
            .arg(m.geometry.length1)
            .arg(m.geometry.length2);
    m_propertyPanel->setPlainText(
        tr("名称: %1\n"
           "类型: %2\n"
           "名义值: %3\n"
           "上公差: %4\n"
           "下公差: %5\n"
           "小数位: %6\n"
           "几何(相对模板):\n"
           "%7")
            .arg(m.name, Me::measureTypeToString(m.type))
            .arg(m.nominal)
            .arg(m.upper)
            .arg(m.lower)
            .arg(m.decimals)
            .arg(geometryText));
}

QImage MainWindow::makeOverlayImage(const QImage &source, const Me::CycleResult &result) const
{
    return Me::ResultOverlay::render(source, result);
}

void MainWindow::applyCycleResult(const Me::CycleResult &result)
{
    if (result.alignmentOk || !m_currentImage.isNull()) {
        m_imageView->setImage(makeOverlayImage(m_currentImage, result));
    }

    if (result.totalOk) {
        m_okNgLabel->setText(QStringLiteral("OK"));
        m_okNgLabel->setStyleSheet(
            QStringLiteral("QLabel { font-size: 40px; font-weight: bold; background: #1f5c2e; color: #e8ffe8; }"));
    } else {
        m_okNgLabel->setText(QStringLiteral("NG"));
        m_okNgLabel->setStyleSheet(
            QStringLiteral("QLabel { font-size: 40px; font-weight: bold; background: #6b1f1f; color: #ffe8e8; }"));
    }

    m_resultTable->setRowCount(0);
    for (const auto &m : result.measures) {
        const int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        m_resultTable->setItem(row, 0, new QTableWidgetItem(m.name));
        m_resultTable->setItem(row, 1, new QTableWidgetItem(
            m.measured ? QStringLiteral("%1 %2").arg(m.value, 0, 'f', m.decimals).arg(m.unit)
                       : QStringLiteral("--")));
        m_resultTable->setItem(row, 2, new QTableWidgetItem(
            QStringLiteral("[%1, %2]")
                .arg(m.nominal + m.lower, 0, 'f', m.decimals)
                .arg(m.nominal + m.upper, 0, 'f', m.decimals)));
        auto *judgeItem = new QTableWidgetItem(m.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        judgeItem->setForeground(m.ok ? QColor(40, 160, 60) : QColor(200, 50, 50));
        m_resultTable->setItem(row, 3, judgeItem);
    }

    statusBar()->showMessage(
        tr("试跑完成: %1 | 耗时 %2 ms | %3")
            .arg(result.totalOk ? QStringLiteral("OK") : QStringLiteral("NG"))
            .arg(result.elapsedMs)
            .arg(result.reason),
        8000);

    qInfo().noquote() << tr("试跑完成 [%1] %2 (%3 ms)")
                             .arg(result.totalOk ? QStringLiteral("OK") : QStringLiteral("NG"),
                                  result.reason)
                             .arg(result.elapsedMs);
}

void MainWindow::maybeLoadImageFolderFromProject()
{
    const auto &src = m_projectService->project().imageSource;
    if (src.type != Me::ImageSourceType::Folder || src.path.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(src.path)) {
        qWarning().noquote() << tr("工程图像文件夹不存在: %1").arg(src.path);
        return;
    }
    if (m_sourceThumbs->loadFolder(src.path)) {
        qInfo().noquote() << tr("已按工程加载文件夹: %1（%2 张）")
                                 .arg(src.path)
                                 .arg(m_sourceThumbs->count());
    }
}

bool MainWindow::maybeSaveBeforeContinue()
{
    if (!m_projectService->isDirty()) {
        return true;
    }

    const auto ret = QMessageBox::question(
        this,
        tr("未保存的更改"),
        tr("当前工程有未保存的更改，是否保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (ret == QMessageBox::Cancel) {
        return false;
    }
    if (ret == QMessageBox::Discard) {
        return true;
    }
    return saveProjectInternal(false);
}

bool MainWindow::saveProjectInternal(bool forceSaveAs)
{
    QString path = m_projectService->filePath();
    if (forceSaveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            tr("保存工程"),
            path.isEmpty() ? (m_projectService->project().name + QStringLiteral(".mexp.json"))
                           : path,
            QString::fromUtf8(Me::kProjectFileFilter));
        if (path.isEmpty()) {
            return false;
        }
        if (!path.endsWith(QLatin1String(".mexp.json"), Qt::CaseInsensitive)
            && !path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
            path += QStringLiteral(".mexp.json");
        }
    }

    QString error;
    if (!m_projectService->saveToFile(path, &error)) {
        QMessageBox::warning(this, tr("保存失败"), error);
        return false;
    }
    updateWindowTitle();
    return true;
}

void MainWindow::onNewProject()
{
    if (!maybeSaveBeforeContinue()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("新建工程"), tr("工程名称:"), QLineEdit::Normal, tr("未命名工程"), &ok);
    if (!ok) {
        return;
    }
    m_projectService->createNew(name.trimmed().isEmpty() ? tr("未命名工程") : name.trimmed());
    m_okNgLabel->setText(QStringLiteral("---"));
    m_okNgLabel->setStyleSheet(
        QStringLiteral("QLabel { font-size: 36px; font-weight: bold; background: #3a3a3a; color: #dddddd; }"));
    m_resultTable->setRowCount(0);
    qInfo().noquote() << tr("已新建工程: %1").arg(m_projectService->project().name);
    updateWindowTitle();
}

void MainWindow::onOpenProject()
{
    if (!maybeSaveBeforeContinue()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开工程"), QString(), QString::fromUtf8(Me::kProjectFileFilter));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!m_projectService->loadFromFile(path, &error)) {
        QMessageBox::warning(this, tr("打开失败"), error);
        return;
    }
    updateWindowTitle();
}

void MainWindow::onSaveProject()
{
    saveProjectInternal(false);
}

void MainWindow::onSaveProjectAs()
{
    saveProjectInternal(true);
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
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("加载失败"), tr("无法读取图像。"));
        return;
    }
    m_currentImage = img;
    m_imageView->setImage(img);
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

    m_projectService->setImageSourceFolder(m_sourceThumbs->folderPath());
    updateWindowTitle();

    qInfo().noquote() << tr("已加载文件夹: %1（%2 张）")
                             .arg(m_sourceThumbs->folderPath())
                             .arg(m_sourceThumbs->count());
}

void MainWindow::onCalibration()
{
    CalibrationDialog dlg(m_projectService->project().calibration, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_projectService->setCalibration(dlg.result());
    updateCalibrationStatus();
    qInfo().noquote() << tr("已更新标定: calibrated=%1 pixelSizeMm=%2")
                             .arg(m_projectService->project().calibration.calibrated)
                             .arg(m_projectService->project().calibration.pixelSizeMm);
}

void MainWindow::onTrainAlignment()
{
    if (m_roiEditMode == RoiEditMode::MeasureCaliper) {
        QMessageBox::information(this, tr("提示"), tr("当前正在编辑卡尺 ROI，请先确认或取消。"));
        return;
    }
    if (m_currentImage.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("请先加载图像。"));
        return;
    }
    auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        return;
    }

    // 第一次：进入主图画 ROI；第二次：用当前 ROI 确认训练
    if (m_roiEditMode != RoiEditMode::AlignTemplate) {
        m_imageView->clearRoi();
        enterRoiEditMode(false);
        return;
    }

    if (!m_imageView->hasRoi()) {
        QMessageBox::information(this, tr("提示"), tr("请先在主图上画出有效的 ROI 矩形。"));
        return;
    }

    const QRectF roi = m_imageView->roiImageRect();
    TrainAlignmentDialog dlg(recipe->alignment, roi, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    if (!m_projectService->hasFilePath()) {
        if (!saveProjectInternal(true)) {
            QMessageBox::information(this, tr("提示"), tr("需先保存工程，以便写入模型相对路径。"));
            return;
        }
    }

    const QString relModel = QStringLiteral("models/%1.shm").arg(recipe->id);
    const QString absModel = QDir(projectDir()).filePath(relModel);

    Me::AlignmentConfig cfg = dlg.alignmentParams();
    cfg.modelFile = relModel;
    cfg.searchRegion = Me::SearchRegion{};

    QString error;
    QVector<Me::Contour2D> previewContours;
    if (!Me::AlignmentService::createModel(m_currentImage,
                                          dlg.modelRoi(),
                                          absModel,
                                          cfg.angleStartDeg,
                                          cfg.angleExtentDeg,
                                          &error,
                                          &previewContours)) {
        QMessageBox::warning(this, tr("训练失败"), error);
        qWarning().noquote() << tr("定位模板训练失败: %1").arg(error);
        return;
    }

    m_projectService->updateAlignment(recipe->id, cfg);

    // 训练完成后在主图叠加模型轮廓（类似例程）
    const QRectF roiRect(roi.left(), roi.top(), roi.width(), roi.height());
    m_imageView->setImage(Me::ResultOverlay::renderContours(m_currentImage, previewContours, &roiRect));

    leaveRoiEditMode(false);
    qInfo().noquote() << tr("定位模板已创建: %1 (ROI %2,%3 - %4,%5)")
                             .arg(absModel)
                             .arg(roi.top(), 0, 'f', 1)
                             .arg(roi.left(), 0, 'f', 1)
                             .arg(roi.bottom(), 0, 'f', 1)
                             .arg(roi.right(), 0, 'f', 1);
    QMessageBox::information(this, tr("完成"), tr("定位模板已保存到:\n%1").arg(absModel));
}

void MainWindow::onDrawMeasureRoi()
{
    if (m_roiEditMode == RoiEditMode::AlignTemplate) {
        QMessageBox::information(this, tr("提示"), tr("当前正在编辑模板 ROI，请先确认或取消。"));
        return;
    }
    if (m_currentImage.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("请先加载图像。"));
        return;
    }

    const QString recipeId = selectedRecipeId();
    const QString measureId = selectedMeasureId();
    if (recipeId.isEmpty() || measureId.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择测量项。"));
        return;
    }

    const auto *recipe = m_projectService->project().findRecipe(recipeId);
    if (!recipe) {
        return;
    }
    if (recipe->alignment.modelFile.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先创建定位模板，再画卡尺 ROI。"));
        return;
    }
    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        return;
    }

    // 拷贝，避免后续 structureChanged 导致悬空引用
    const Me::AlignmentConfig alignCfg = recipe->alignment;
    const Me::MeasureItem measureCopy = recipe->measures.at(idx);
    const QRectF roiRect = m_imageView->roiImageRect();

    // 第一次：进入编辑；第二次：确认写入相对模板几何
    if (m_roiEditMode != RoiEditMode::MeasureCaliper) {
        const Me::AlignmentResult align =
            Me::AlignmentService::find(m_currentImage, alignCfg, projectDir(), false);
        QRectF seed;
        if (align.ok) {
            seed = measureGeometryToImageRect(measureCopy.geometry, align);
            qInfo().noquote() << tr("已按当前定位位姿投影卡尺框 (score=%1)")
                                     .arg(align.score, 0, 'f', 3);
        } else {
            const int w = m_currentImage.width();
            const int h = m_currentImage.height();
            seed = QRectF(w * 0.35, h * 0.45, w * 0.3, h * 0.08);
            qWarning().noquote() << tr("当前图定位失败，先给默认卡尺框；确认时需定位成功。%1")
                                        .arg(align.message);
        }
        m_imageView->setRoiImageRect(seed);
        enterRoiEditMode(true);
        return;
    }

    if (!m_imageView->hasRoi()) {
        QMessageBox::information(this, tr("提示"), tr("请先在主图上画出有效的卡尺矩形。"));
        return;
    }

    qInfo().noquote() << tr("确认卡尺 ROI...");
    const Me::AlignmentResult align =
        Me::AlignmentService::find(m_currentImage, alignCfg, projectDir(), false);
    if (!align.ok) {
        QMessageBox::warning(this, tr("无法换算"),
                             tr("确认卡尺前需要当前图像定位成功。\n%1").arg(align.message));
        return;
    }

    Me::CaliperGeometry geo;
    if (!imageRectToMeasureGeometry(roiRect, align, &geo)) {
        QMessageBox::warning(this, tr("无法换算"), tr("卡尺矩形无效。"));
        return;
    }
    if (!qIsFinite(geo.row) || !qIsFinite(geo.column) || !qIsFinite(geo.phi)
        || !qIsFinite(geo.length1) || !qIsFinite(geo.length2)
        || geo.length1 < 2.0 || geo.length2 < 2.0) {
        QMessageBox::warning(this, tr("无法换算"), tr("卡尺几何无效，请重新绘制。"));
        return;
    }

    Me::MeasureItem item = measureCopy;
    item.geometry = geo;

    // 先退出 ROI 模式，再写回（避免 structureChanged / 刷新与 ROI 交互冲突）
    leaveRoiEditMode(false);
    m_projectService->updateMeasure(recipeId, item);
    refreshMeasureProperties();

    // 轻量预览：只画当前卡尺，不叠模型轮廓（避免大量 XLD 点导致崩溃）
    Me::CycleResult preview;
    preview.alignmentOk = true;
    preview.alignment.ok = true;
    preview.alignment.row = align.row;
    preview.alignment.column = align.column;
    preview.alignment.angleRad = align.angleRad;
    preview.alignment.score = align.score;
    Me::MeasureResult mr;
    mr.id = item.id;
    mr.name = item.name;
    mr.type = item.type;
    mr.ok = true;
    mr.measured = false;
    mr.imageRow = roiRect.center().y();
    mr.imageCol = roiRect.center().x();
    mr.imagePhi = 0.0; // 确认框为图像轴对齐
    mr.length1 = geo.length1;
    mr.length2 = geo.length2;
    mr.message = tr("卡尺ROI已更新");
    preview.measures.push_back(mr);
    preview.totalOk = true;
    preview.reason = tr("卡尺ROI已写入相对模板坐标");
    m_imageView->setImage(Me::ResultOverlay::render(m_currentImage, preview));

    qInfo().noquote() << tr("卡尺 ROI 已更新: %1  row=%2 col=%3 phi=%4 L1=%5 L2=%6")
                             .arg(item.name)
                             .arg(geo.row, 0, 'f', 2)
                             .arg(geo.column, 0, 'f', 2)
                             .arg(geo.phi, 0, 'f', 4)
                             .arg(geo.length1, 0, 'f', 2)
                             .arg(geo.length2, 0, 'f', 2);
}

void MainWindow::onEditMeasure()
{
    const QString recipeId = selectedRecipeId();
    const QString measureId = selectedMeasureId();
    if (recipeId.isEmpty() || measureId.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择测量项。"));
        return;
    }
    const auto *recipe = m_projectService->project().findRecipe(recipeId);
    if (!recipe) {
        return;
    }
    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        return;
    }

    EditMeasureDialog dlg(recipe->measures.at(idx), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    Me::MeasureItem item = dlg.result();
    if (item.name.isEmpty()) {
        item.name = tr("测量项");
    }
    // 单位跟随工程标定
    item.unit = m_projectService->project().calibration.calibrated
        ? m_projectService->project().calibration.unit
        : QStringLiteral("px");
    m_projectService->updateMeasure(recipeId, item);
    updateWindowTitle();
}

void MainWindow::onRunOnce()
{
    if (m_running) {
        return;
    }
    if (m_currentImage.isNull()) {
        QMessageBox::information(this, tr("提示"), tr("请先加载图像。"));
        return;
    }
    const auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        return;
    }
    if (recipe->alignment.modelFile.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先创建定位模板。"));
        return;
    }
    if (recipe->measures.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先添加测量项。"));
        return;
    }

    setRunningUi(true);
    qInfo().noquote() << tr("开始单次试跑...");
    QMetaObject::invokeMethod(m_runEngine,
                              "runOnce",
                              Qt::QueuedConnection,
                              Q_ARG(QImage, m_currentImage),
                              Q_ARG(Me::Recipe, *recipe),
                              Q_ARG(Me::CalibrationConfig, m_projectService->project().calibration),
                              Q_ARG(QString, projectDir()));
}

void MainWindow::onCycleFinished(const Me::CycleResult &result)
{
    setRunningUi(false);
    applyCycleResult(result);
}

void MainWindow::onRunError(const QString &message)
{
    qWarning().noquote() << message;
}

void MainWindow::onThumbImageChanged(const QImage &image)
{
    m_currentImage = image;
    m_imageView->setImage(image);
}

void MainWindow::onThumbPathChanged(const QString &path)
{
    updateImageSourceStatus(path);
    qInfo().noquote() << tr("当前图像: %1").arg(QFileInfo(path).fileName());
}

void MainWindow::onRecipeSelectionChanged()
{
    if (m_updatingLists) {
        return;
    }
    const QString id = selectedRecipeId();
    if (!id.isEmpty()) {
        m_projectService->setActiveRecipeId(id);
    }
    refreshMeasureList();
}

void MainWindow::onMeasureSelectionChanged()
{
    if (m_updatingLists) {
        return;
    }
    refreshMeasureProperties();
}

void MainWindow::onMeasureDoubleClicked()
{
    onEditMeasure();
}

void MainWindow::onAddRecipe()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("添加配方"), tr("配方名称:"), QLineEdit::Normal, tr("新配方"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_projectService->addRecipe(name.trimmed());
    updateWindowTitle();
}

void MainWindow::onRemoveRecipe()
{
    const QString id = selectedRecipeId();
    if (id.isEmpty()) {
        return;
    }
    if (m_projectService->project().recipes.size() <= 1) {
        QMessageBox::information(this, tr("提示"), tr("至少保留一个配方。"));
        return;
    }
    if (QMessageBox::question(this, tr("删除配方"), tr("确定删除当前配方？"))
        != QMessageBox::Yes) {
        return;
    }
    m_projectService->removeRecipe(id);
    updateWindowTitle();
}

void MainWindow::onAddMeasure()
{
    const QString recipeId = selectedRecipeId();
    if (recipeId.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("添加测量项"), tr("测量项名称:"), QLineEdit::Normal, tr("宽度1"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    const QString id = m_projectService->addMeasure(recipeId, name.trimmed(), MeasureType::CaliperWidth);
    // 给一个可用的默认几何（相对模板原点偏置）
    if (!id.isEmpty()) {
        auto *recipe = m_projectService->project().findRecipe(recipeId);
        if (recipe) {
            const int idx = recipe->indexOfMeasure(id);
            if (idx >= 0) {
                Me::MeasureItem item = recipe->measures.at(idx);
                item.geometry.row = 0;
                item.geometry.column = 60;
                item.geometry.phi = 0;
                item.geometry.length1 = 40;
                item.geometry.length2 = 8;
                item.nominal = 20;
                item.upper = 5;
                item.lower = -5;
                item.unit = m_projectService->project().calibration.calibrated
                    ? m_projectService->project().calibration.unit
                    : QStringLiteral("px");
                m_projectService->updateMeasure(recipeId, item);
            }
        }
    }
    updateWindowTitle();
}

void MainWindow::onRemoveMeasure()
{
    const QString recipeId = selectedRecipeId();
    const QString measureId = selectedMeasureId();
    if (recipeId.isEmpty() || measureId.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, tr("删除测量项"), tr("确定删除当前测量项？"))
        != QMessageBox::Yes) {
        return;
    }
    m_projectService->removeMeasure(recipeId, measureId);
    updateWindowTitle();
}

void MainWindow::onProjectStructureChanged()
{
    refreshRecipeList();
    refreshMeasureList();
    updateWindowTitle();
    updateCalibrationStatus();
}

void MainWindow::onDirtyChanged(bool dirty)
{
    Q_UNUSED(dirty);
    updateWindowTitle();
}
