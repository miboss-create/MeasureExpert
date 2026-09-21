#include "MainWindow.h"

#include "Alg/AlignmentService.h"
#include "Alg/RunEngine.h"
#include "Core/CsvResultStore.h"
#include "Core/ProjectService.h"
#include "CustomImageView.h"
#include "LogWidget.h"
#include "ThumbnailStrip.h"
#include "UI/CalibrationDialog.h"
#include "UI/EditMeasureDialog.h"
#include "UI/ManageRecipesDialog.h"
#include "UI/ResultOverlay.h"
#include "UI/TrainAlignmentDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
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
#include <QTimer>
#include <QToolBar>
#include <QThread>
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

    if (!initWidget()) {
        auto *fallback = new QTextEdit(this);
        setCentralWidget(fallback);
        fallback->setPlainText(tr("界面初始化失败。"));
        return;
    }

    createMenus();
    createToolBar();
    createStatusBar();
    setupRunEngine();
    applyAppModeUi();
    refreshRecipeList();
    refreshMeasureList();

    connect(m_projectService, &ProjectService::structureChanged,
            this, &MainWindow::onProjectStructureChanged, Qt::QueuedConnection);
    connect(m_projectService, &ProjectService::dirtyChanged,
            this, &MainWindow::onDirtyChanged, Qt::QueuedConnection);
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
    shutdownRunEngine();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_busy || m_continuous) {
        QMessageBox::information(this, tr("提示"), tr("正在运行，请先停止再关闭。"));
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

    bool circleRoi = false;
    if (forMeasure) {
        const QString measureId = selectedMeasureId();
        const auto *recipe = m_projectService->activeRecipe();
        if (recipe && !measureId.isEmpty()) {
            const int idx = recipe->indexOfMeasure(measureId);
            if (idx >= 0) {
                circleRoi = (recipe->measures.at(idx).type == MeasureType::FindCircle);
            }
        }
        m_imageView->setRoiShape(circleRoi ? RoiRectItem::Shape::Circle
                                           : RoiRectItem::Shape::Rectangle);
        m_imageView->setRoiStyle(QColor(255, 200, 0),
                                 circleRoi ? tr("找圆") : tr("卡尺"));
    } else {
        m_imageView->setRoiShape(RoiRectItem::Shape::Rectangle);
        m_imageView->setRoiStyle(QColor(0, 220, 120), tr("模板"));
    }

    if (!m_imageView->hasRoi()) {
        const int w = m_currentImage.width();
        const int h = m_currentImage.height();
        if (circleRoi) {
            const double side = qMin(w, h) * 0.35;
            m_imageView->setRoiImageRect(
                QRectF((w - side) * 0.5, (h - side) * 0.5, side, side));
        } else {
            m_imageView->setRoiImageRect(QRectF(w * 0.25, h * 0.25, w * 0.5, h * 0.5));
        }
    }
    m_imageView->setRoiEditEnabled(true);
    if (m_cancelRoiAction) {
        m_cancelRoiAction->setEnabled(true);
    }
    updateRoiActionTexts();
    if (forMeasure) {
        if (circleRoi) {
            statusBar()->showMessage(
                tr("找圆 ROI：左键拖出圆形，右键确认后可拖图并调整；调好后点「确认卡尺ROI」。Esc 取消。"),
                0);
            qInfo().noquote() << tr("已进入找圆 ROI 编辑：请在主图上调整黄色圆");
        } else {
            statusBar()->showMessage(
                tr("卡尺 ROI：左键拖拽绘制，右键确认后可拖图并调整黄框；选中右键可删除。调好后点「确认卡尺ROI」。Esc 取消。"),
                0);
            qInfo().noquote() << tr("已进入卡尺 ROI 编辑：请在主图上调整黄色矩形");
        }
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
    m_imageView->setRoiShape(RoiRectItem::Shape::Rectangle);
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
                                             const Me::AlignmentResult &align,
                                             Me::MeasureType type) const
{
    double rowImg = geo.row;
    double colImg = geo.column;
    if (align.ok) {
        const double cA = qCos(align.angleRad);
        const double sA = qSin(align.angleRad);
        rowImg = cA * geo.row - sA * geo.column + align.row;
        colImg = sA * geo.row + cA * geo.column + align.column;
    }

    if (type == MeasureType::FindCircle) {
        const double outer = qMax(4.0, geo.length1 + qMax(0.0, geo.length2));
        const double side = qMax(8.0, outer * 2.0);
        return QRectF(colImg - side * 0.5, rowImg - side * 0.5, side, side);
    }

    const double w = qMax(8.0, geo.length1 * 2.0);
    const double h = qMax(8.0, geo.length2 * 2.0);
    return QRectF(colImg - w * 0.5, rowImg - h * 0.5, w, h);
}

bool MainWindow::imageRectToMeasureGeometry(const QRectF &imageRect,
                                           const Me::AlignmentResult &align,
                                           Me::CaliperGeometry *out) const
{
    if (!out || !imageRect.isValid()) {
        return false;
    }
    const QRectF r = imageRect.normalized();
    const double rowImg = r.center().y();
    const double colImg = r.center().x();

    if (align.ok) {
        const double cA = qCos(align.angleRad);
        const double sA = qSin(align.angleRad);
        const double dRow = rowImg - align.row;
        const double dCol = colImg - align.column;
        // 相对模板坐标，运行时再仿射跟随
        out->row = cA * dRow + sA * dCol;
        out->column = -sA * dRow + cA * dCol;
        out->phi = -align.angleRad;
    } else {
        // 图像坐标：无定位时直接使用
        out->row = rowImg;
        out->column = colImg;
        out->phi = 0.0;
    }
    out->length1 = qMax(2.0, r.width() * 0.5);
    out->length2 = qMax(2.0, r.height() * 0.5);
    return true;
}

void MainWindow::shutdownRunEngine()
{
    if (!m_runEngine && !m_runThread) {
        return;
    }

    RunEngine *engine = m_runEngine;
    QThread *thread = m_runThread;
    m_runEngine = nullptr;

    if (engine) {
        disconnect(engine, nullptr, this, nullptr);
        disconnect(this, nullptr, engine, nullptr);
        if (thread && thread->isRunning()) {
            // 在工作线程里排队删除，禁止主线程直接 delete
            QMetaObject::invokeMethod(engine, &RunEngine::shutdown, Qt::BlockingQueuedConnection);
        }
    }
    if (thread && thread->isRunning()) {
        thread->quit();
        thread->wait(3000);
    }
}

void MainWindow::setupRunEngine()
{
    m_runThread = new QThread(this);
    m_runEngine = new RunEngine();
    m_runEngine->moveToThread(m_runThread);

    connect(m_runEngine, &RunEngine::cycleFinished, this, &MainWindow::onCycleFinished);
    connect(m_runEngine, &RunEngine::errorOccurred, this, &MainWindow::onRunError);

    m_runThread->start();
}

bool MainWindow::initWidget()
{
    auto *configPanel = new QWidget(this);
    auto *configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(4, 4, 4, 4);

    m_activeRecipeLabel = new QLabel(configPanel);
    m_activeRecipeLabel->setWordWrap(true);
    configLayout->addWidget(m_activeRecipeLabel);

    configLayout->addWidget(new QLabel(tr("测量项"), configPanel));
    m_measureList = new QListWidget(configPanel);
    configLayout->addWidget(m_measureList, 1);

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

    connect(m_measureList, &QListWidget::currentRowChanged,
            this, &MainWindow::onMeasureSelectionChanged);
    connect(m_measureList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onMeasureDoubleClicked);
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
    m_mainSplitter->setSizes({220, 760, 280});

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
    acqMenu->addAction(tr("加载图像..."), this, &MainWindow::onLoadImage);
    acqMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);
    acqMenu->addSeparator();
    acqMenu->addAction(tr("上一张"), m_sourceThumbs, &ThumbnailStrip::selectPrevious);
    acqMenu->addAction(tr("下一张"), m_sourceThumbs, &ThumbnailStrip::selectNext);

    auto *recipeMenu = menuBar()->addMenu(tr("配方(&P)"));
    m_manageRecipesAction = recipeMenu->addAction(tr("管理配方..."), this, &MainWindow::onManageRecipes);

    auto *calMenu = menuBar()->addMenu(tr("标定(&C)"));
    calMenu->addAction(tr("像素当量..."), this, &MainWindow::onCalibration);

    auto *runMenu = menuBar()->addMenu(tr("运行(&R)"));
    m_trainAlignAction = runMenu->addAction(tr("创建定位模板..."), this, &MainWindow::onTrainAlignment);
    m_drawMeasureRoiAction = runMenu->addAction(tr("画卡尺ROI..."), this, &MainWindow::onDrawMeasureRoi);
    m_cancelRoiAction = runMenu->addAction(tr("取消 ROI 编辑"), this, &MainWindow::onCancelRoiEdit);
    m_cancelRoiAction->setEnabled(false);
    runMenu->addAction(tr("编辑测量项..."), this, &MainWindow::onEditMeasure);
    runMenu->addSeparator();
    m_runOnceAction = runMenu->addAction(tr("单次试跑"), this, &MainWindow::onRunOnce);
    m_continuousAction = runMenu->addAction(tr("连续运行"), this, &MainWindow::onContinuousRun);
    m_stopAction = runMenu->addAction(tr("停止"), this, &MainWindow::onStopRun);
    m_stopAction->setEnabled(false);
    runMenu->addSeparator();
    m_modeToggleAction = runMenu->addAction(tr("切换到运行模式"), this, &MainWindow::onToggleEditRunMode);

    menuBar()->addMenu(tr("工具(&T)"));
    menuBar()->addMenu(tr("帮助(&H)"));
}

void MainWindow::createToolBar()
{
    auto *tb = addToolBar(tr("主工具栏"));
    tb->setMovable(false);
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
    tb->addSeparator();
    if (!m_runOnceAction) {
        m_runOnceAction = new QAction(tr("单次试跑"), this);
        connect(m_runOnceAction, &QAction::triggered, this, &MainWindow::onRunOnce);
    }
    tb->addAction(m_runOnceAction);
    if (!m_continuousAction) {
        m_continuousAction = new QAction(tr("连续运行"), this);
        connect(m_continuousAction, &QAction::triggered, this, &MainWindow::onContinuousRun);
    }
    tb->addAction(m_continuousAction);
    if (!m_stopAction) {
        m_stopAction = new QAction(tr("停止"), this);
        connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopRun);
        m_stopAction->setEnabled(false);
    }
    tb->addAction(m_stopAction);
    tb->addSeparator();
    if (!m_modeToggleAction) {
        m_modeToggleAction = new QAction(tr("切换到运行模式"), this);
        connect(m_modeToggleAction, &QAction::triggered, this, &MainWindow::onToggleEditRunMode);
    }
    tb->addAction(m_modeToggleAction);
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
    const QString modeText =
        (m_appMode == AppMode::Edit) ? tr("编辑") : tr("运行");
    const QString runText = m_continuous ? tr("连续") : (m_busy ? tr("试跑中") : tr("-"));
    statusBar()->showMessage(
        tr("图像源: %1 | 标定: %2 | 节拍: %3 | 模式: %4 | 通信: 预留")
            .arg(m_currentImageStatus, calText, runText, modeText));
}

QString MainWindow::selectedRecipeId() const
{
    return m_projectService->project().activeRecipeId;
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

void MainWindow::setBusyUi(bool busy)
{
    m_busy = busy;
    applyAppModeUi();
    updateCalibrationStatus();
}

void MainWindow::applyAppModeUi()
{
    const bool edit = (m_appMode == AppMode::Edit);
    const bool canEdit = edit && !m_busy && !m_continuous;

    m_addMeasureBtn->setEnabled(canEdit);
    m_editMeasureBtn->setEnabled(canEdit && m_measureList->count() > 0);
    m_drawMeasureRoiBtn->setEnabled(canEdit && m_measureList->count() > 0);
    m_removeMeasureBtn->setEnabled(canEdit && m_measureList->count() > 0);
    if (m_manageRecipesAction) {
        m_manageRecipesAction->setEnabled(canEdit);
    }

    if (m_trainAlignAction) {
        m_trainAlignAction->setEnabled(canEdit);
    }
    if (m_drawMeasureRoiAction) {
        m_drawMeasureRoiAction->setEnabled(canEdit);
    }
    if (m_runOnceAction) {
        m_runOnceAction->setEnabled(!m_busy && !m_continuous);
    }
    if (m_continuousAction) {
        m_continuousAction->setEnabled(!m_busy && !m_continuous);
    }
    if (m_stopAction) {
        m_stopAction->setEnabled(m_busy || m_continuous);
    }
    if (m_modeToggleAction) {
        m_modeToggleAction->setEnabled(!m_busy && !m_continuous);
        m_modeToggleAction->setText(edit ? tr("切换到运行模式") : tr("切换到编辑模式"));
    }
}

void MainWindow::refreshRecipeList()
{
    const auto *recipe = m_projectService->activeRecipe();
    if (m_activeRecipeLabel) {
        if (recipe) {
            m_activeRecipeLabel->setText(tr("当前配方: %1").arg(recipe->name));
        } else {
            m_activeRecipeLabel->setText(tr("当前配方: (无)"));
        }
    }
    applyAppModeUi();
}

void MainWindow::refreshMeasureList()
{
    m_updatingLists = true;
    const QString keepMeasureId = selectedMeasureId();
    m_measureList->clear();

    const auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        m_updatingLists = false;
        applyAppModeUi();
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
    applyAppModeUi();
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
    const int dec = qBound(0, m.decimals, 6);
    // 避免 QString::arg 链式替换触发 MaxSize 断言：全部用拼接
    QString text;
    text += tr("名称: %1\n").arg(m.name);
    text += tr("类型: %1\n").arg(Me::measureTypeToString(m.type));
    text += tr("名义值: %1\n").arg(m.nominal, 0, 'f', dec);
    text += tr("上公差: %1\n").arg(m.upper, 0, 'f', dec);
    text += tr("下公差: %1\n").arg(m.lower, 0, 'f', dec);
    text += tr("小数位: %1\n").arg(dec);
    text += tr("几何(相对模板):\n");
    text += QStringLiteral("  row=%1  col=%2  phi=%3\n")
                .arg(m.geometry.row, 0, 'f', 3)
                .arg(m.geometry.column, 0, 'f', 3)
                .arg(m.geometry.phi, 0, 'f', 4);
    text += QStringLiteral("  length1=%1  length2=%2  angleExtent=%3\n")
                .arg(m.geometry.length1, 0, 'f', 3)
                .arg(m.geometry.length2, 0, 'f', 3)
                .arg(m.geometry.angleExtent, 0, 'f', 4);
    text += QStringLiteral("  refA=%1  refB=%2")
                .arg(m.refIdA.isEmpty() ? QStringLiteral("-") : m.refIdA,
                     m.refIdB.isEmpty() ? QStringLiteral("-") : m.refIdB);
    m_propertyPanel->setPlainText(text);
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
    m_currentImagePath = path;
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
    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        return;
    }

    // 拷贝，避免后续 structureChanged 导致悬空引用
    const Me::AlignmentConfig alignCfg = recipe->alignment;
    const Me::MeasureItem measureCopy = recipe->measures.at(idx);
    const QRectF roiRect = m_imageView->roiImageRect();
    const bool followPose = !alignCfg.modelFile.isEmpty();

    const auto t = measureCopy.type;
    if (t == MeasureType::PointToLine || t == MeasureType::LineIntersection
        || t == MeasureType::Angle || t == MeasureType::PointDistance) {
        QMessageBox::information(this, tr("提示"),
                                 tr("派生测量项通过编辑对话框设置引用 A/B，无需画 ROI。"));
        return;
    }

    // 第一次：进入编辑；第二次：确认写入几何（有定位则相对模板，否则图像坐标）
    if (m_roiEditMode != RoiEditMode::MeasureCaliper) {
        Me::AlignmentResult align;
        if (followPose) {
            align = Me::AlignmentService::find(m_currentImage, alignCfg, projectDir(), false);
        }
        QRectF seed;
        if (followPose && align.ok) {
            seed = measureGeometryToImageRect(measureCopy.geometry, align, measureCopy.type);
            qInfo().noquote() << tr("已按当前定位位姿投影卡尺 (score=%1)")
                                     .arg(align.score, 0, 'f', 3);
        } else if (!followPose
                   && (measureCopy.geometry.length1 > 1.0 || measureCopy.geometry.length2 > 1.0)) {
            seed = measureGeometryToImageRect(measureCopy.geometry, {}, measureCopy.type);
            qInfo().noquote() << tr("已载入图像坐标 ROI（未启用定位跟随）");
        } else {
            const int w = m_currentImage.width();
            const int h = m_currentImage.height();
            if (measureCopy.type == MeasureType::FindCircle) {
                const double side = qMin(w, h) * 0.3;
                seed = QRectF((w - side) * 0.5, (h - side) * 0.5, side, side);
            } else {
                seed = QRectF(w * 0.35, h * 0.45, w * 0.3, h * 0.08);
            }
            if (followPose && !align.ok) {
                qWarning().noquote() << tr("定位失败，使用默认 ROI；确认时将写入图像坐标。%1")
                                            .arg(align.message);
            }
        }
        m_imageView->setRoiShape(measureCopy.type == MeasureType::FindCircle
                                     ? RoiRectItem::Shape::Circle
                                     : RoiRectItem::Shape::Rectangle);
        m_imageView->setRoiImageRect(seed);
        enterRoiEditMode(true);
        return;
    }

    if (!m_imageView->hasRoi()) {
        QMessageBox::information(this, tr("提示"),
                                 measureCopy.type == MeasureType::FindCircle
                                     ? tr("请先在主图上画出有效的圆形 ROI。")
                                     : tr("请先在主图上画出有效的卡尺矩形。"));
        return;
    }

    qInfo().noquote() << tr("确认卡尺 ROI...");
    Me::AlignmentResult align;
    if (followPose) {
        align = Me::AlignmentService::find(m_currentImage, alignCfg, projectDir(), false);
        if (!align.ok) {
            const auto choice = QMessageBox::question(
                this,
                tr("定位失败"),
                tr("当前图定位失败，是否仍按图像坐标保存 ROI（本次不跟随）？\n%1")
                    .arg(align.message),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (choice != QMessageBox::Yes) {
                return;
            }
        }
    }

    Me::CaliperGeometry geo;
    if (!imageRectToMeasureGeometry(roiRect, align, &geo)) {
        QMessageBox::warning(this, tr("无法换算"), tr("卡尺 ROI 无效。"));
        return;
    }
    // 找圆：正圆外径 → 半径 length1 + 环宽 length2
    if (measureCopy.type == MeasureType::FindCircle) {
        const QRectF r = roiRect.normalized();
        const double outerR = qMax(4.0, qMin(r.width(), r.height()) * 0.5);
        double ann = measureCopy.geometry.length2;
        if (!(ann > 0.0) || ann >= outerR) {
            ann = qMax(2.0, outerR * 0.15);
        }
        if (ann >= outerR) {
            ann = qMax(2.0, outerR * 0.15);
        }
        geo.length1 = qMax(2.0, outerR - ann);
        geo.length2 = ann;
        geo.angleExtent = measureCopy.geometry.angleExtent > 0
            ? measureCopy.geometry.angleExtent
            : 6.283185307179586;
    } else {
        geo.angleExtent = measureCopy.geometry.angleExtent;
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
    mr.angleExtent = geo.angleExtent;
    mr.message = tr("测量ROI已更新");
    preview.measures.push_back(mr);
    preview.totalOk = true;
    preview.reason = tr("测量ROI已写入相对模板坐标");
    m_imageView->setImage(Me::ResultOverlay::render(m_currentImage, preview));

    qInfo().noquote() << tr("测量 ROI 已更新: %1  row=%2 col=%3 phi=%4 L1=%5 L2=%6")
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

    // 拷贝，避免对话框打开期间 structureChanged 导致引用失效
    const Me::MeasureItem itemCopy = recipe->measures.at(idx);
    const QVector<Me::MeasureItem> siblingsCopy = recipe->measures;

    Me::MeasureItem item;
    {
        EditMeasureDialog dlg(itemCopy, siblingsCopy, this);
        const int code = dlg.exec();
        if (code != QDialog::Accepted) {
            return;
        }
        item = dlg.result();
    } // 对话框先销毁，再写回工程，避免关窗重入

    if (item.name.isEmpty()) {
        item.name = tr("测量项");
    }
    if (item.type == MeasureType::Angle) {
        item.unit = QStringLiteral("deg");
    } else {
        item.unit = m_projectService->project().calibration.calibrated
            ? m_projectService->project().calibration.unit
            : QStringLiteral("px");
    }
    m_projectService->updateMeasure(recipeId, item);
    updateWindowTitle();
}

void MainWindow::onRunOnce()
{
    if (m_busy || m_continuous) {
        return;
    }
    QString fail;
    if (!prepareRun(&fail)) {
        QMessageBox::information(this, tr("提示"), fail);
        return;
    }
    m_stopRequested = false;
    qInfo().noquote() << tr("开始单次试跑...");
    dispatchRunOnce();
}

void MainWindow::onContinuousRun()
{
    if (m_busy || m_continuous) {
        return;
    }
    QString fail;
    if (!prepareRun(&fail)) {
        QMessageBox::information(this, tr("提示"), fail);
        return;
    }
    if (m_sourceThumbs->count() <= 0) {
        QMessageBox::information(this, tr("提示"), tr("连续运行需要先加载图像文件夹。"));
        return;
    }

    m_continuous = true;
    m_stopRequested = false;
    applyAppModeUi();
    updateCalibrationStatus();
    qInfo().noquote() << tr("开始连续运行（文件夹序列循环，点停止结束）...");
    dispatchRunOnce();
}

void MainWindow::onStopRun()
{
    if (!m_busy && !m_continuous) {
        return;
    }
    m_stopRequested = true;
    m_continuous = false;
    applyAppModeUi();
    updateCalibrationStatus();
    qInfo().noquote() << tr("已请求停止，等待当前周期结束...");
}

void MainWindow::onToggleEditRunMode()
{
    if (m_busy || m_continuous) {
        return;
    }
    if (m_roiEditMode != RoiEditMode::None) {
        QMessageBox::information(this, tr("提示"), tr("请先结束 ROI 编辑。"));
        return;
    }
    m_appMode = (m_appMode == AppMode::Edit) ? AppMode::Run : AppMode::Edit;
    applyAppModeUi();
    updateCalibrationStatus();
    qInfo().noquote() << ((m_appMode == AppMode::Edit)
                              ? tr("已切换到编辑模式（试跑默认不写 CSV）")
                              : tr("已切换到运行模式（试跑/连续将写 CSV）"));
}

bool MainWindow::prepareRun(QString *failReason) const
{
    if (m_currentImage.isNull()) {
        if (failReason) {
            *failReason = tr("请先加载图像。");
        }
        return false;
    }
    const auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        if (failReason) {
            *failReason = tr("无可用配方。");
        }
        return false;
    }
    if (recipe->measures.isEmpty()) {
        if (failReason) {
            *failReason = tr("请先添加测量项。");
        }
        return false;
    }
    return true;
}

void MainWindow::dispatchRunOnce()
{
    const auto *recipe = m_projectService->activeRecipe();
    if (!recipe) {
        m_continuous = false;
        setBusyUi(false);
        return;
    }
    setBusyUi(true);
    QMetaObject::invokeMethod(m_runEngine,
                              "runOnce",
                              Qt::QueuedConnection,
                              Q_ARG(QImage, m_currentImage),
                              Q_ARG(Me::Recipe, *recipe),
                              Q_ARG(Me::CalibrationConfig, m_projectService->project().calibration),
                              Q_ARG(QString, projectDir()));
}

void MainWindow::maybeWriteCsv(const Me::CycleResult &result)
{
    // 运行模式必写；编辑模式不写（符合实施文档）
    if (m_appMode != AppMode::Run) {
        return;
    }
    const auto *recipe = m_projectService->activeRecipe();
    const QString recipeName = recipe ? recipe->name : tr("未知配方");
    QString error;
    const QString path = Me::CsvResultStore::defaultFilePath();
    if (!Me::CsvResultStore::append(path, recipeName, m_currentImagePath, result, &error)) {
        qWarning().noquote() << tr("CSV 写入失败: %1").arg(error);
        return;
    }
    qInfo().noquote() << tr("已追加 CSV: %1").arg(path);
}

void MainWindow::onCycleFinished(const Me::CycleResult &result)
{
    applyCycleResult(result);
    maybeWriteCsv(result);

    if (m_stopRequested || !m_continuous) {
        m_continuous = false;
        m_stopRequested = false;
        setBusyUi(false);
        return;
    }

    // 连续：切下一张（到末尾则回到第一张）再跑
    if (m_sourceThumbs->count() <= 0) {
        m_continuous = false;
        setBusyUi(false);
        qWarning().noquote() << tr("连续运行中断：无图像");
        return;
    }
    m_sourceThumbs->selectNext();
    // selectNext 会同步更新 m_currentImage
    setBusyUi(false); // 短暂释放，再派发下一轮
    if (m_stopRequested) {
        m_continuous = false;
        m_stopRequested = false;
        applyAppModeUi();
        return;
    }
    m_continuous = true;
    dispatchRunOnce();
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
    m_currentImagePath = path;
    updateImageSourceStatus(path);
    qInfo().noquote() << tr("当前图像: %1").arg(QFileInfo(path).fileName());
}

void MainWindow::onRecipeSelectionChanged()
{
    // 配方改由「管理配方」对话框切换，此处保留槽以兼容旧连接。
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

void MainWindow::onManageRecipes()
{
    if (m_appMode != AppMode::Edit || m_busy || m_continuous) {
        QMessageBox::information(this, tr("提示"), tr("请在编辑模式下管理配方。"));
        return;
    }
    ManageRecipesDialog dlg(m_projectService, this);
    dlg.exec();
    refreshRecipeList();
    refreshMeasureList();
    updateWindowTitle();
}

void MainWindow::onAddRecipe()
{
    onManageRecipes();
}

void MainWindow::onRemoveRecipe()
{
    onManageRecipes();
}

void MainWindow::onAddMeasure()
{
    const QString recipeId = selectedRecipeId();
    if (recipeId.isEmpty()) {
        return;
    }

    const auto *recipe = m_projectService->project().findRecipe(recipeId);
    const int seq = recipe ? recipe->measures.size() + 1 : 1;
    const QString name = tr("测量%1").arg(seq);

    Me::MeasureItem draft = Me::MeasureItem::createDefault(name);
    draft.geometry.row = 0;
    draft.geometry.column = 60;
    draft.geometry.phi = 0;
    draft.geometry.length1 = 40.0;
    draft.geometry.length2 = 8.0;
    draft.geometry.angleExtent = 6.283185307179586;
    draft.nominal = 20.0;
    draft.upper = 5;
    draft.lower = -5;
    draft.decimals = 3;
    draft.unit = m_projectService->project().calibration.calibrated
        ? m_projectService->project().calibration.unit
        : QStringLiteral("px");

    Me::MeasureItem item;
    {
        EditMeasureDialog dlg(draft, recipe ? recipe->measures : QVector<Me::MeasureItem>{}, this);
        dlg.setWindowTitle(tr("添加测量项"));
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        item = dlg.result();
    }

    if (item.name.isEmpty()) {
        item.name = name;
    }
    if (item.type == MeasureType::Angle) {
        item.unit = QStringLiteral("deg");
    } else {
        item.unit = m_projectService->project().calibration.calibrated
            ? m_projectService->project().calibration.unit
            : QStringLiteral("px");
    }

    // 对话框销毁后再写模型，避免关窗与列表刷新重入。
    QTimer::singleShot(0, this, [this, recipeId, item]() mutable {
        const QString id = m_projectService->addMeasure(recipeId, item.name, item.type);
        if (id.isEmpty()) {
            return;
        }
        item.id = id;
        m_projectService->updateMeasure(recipeId, item);
        updateWindowTitle();
        qInfo().noquote() << tr("已添加测量项: %1 (%2)")
                                 .arg(item.name, Me::measureTypeToString(item.type));
    });
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
