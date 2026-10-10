// ============================================================================
// MainWindow.cpp — 主窗口实现
//
// 主要流程簇：
//   1. 构造 / RunEngine 工作线程
//   2. ROI 编辑（定位模板、卡尺/找圆，二次点击确认）
//   3. 模板↔图像坐标换算（followPose）
//   4. 工程 / 图像 / 标定
//   5. 试跑调度（单次 / 连续 / CSV）
//   6. 配方与测量项管理
// ============================================================================

#include "MainWindow.h"

#include "Alg/AlignmentService.h"
#include "Alg/RunEngine.h"
#include "Alg/ToolCommon.h"
#include "Core/CameraHub.h"
#include "Core/CsvResultStore.h"
#include "Core/ProjectService.h"
#include "CustomImageView.h"
#include "UI/CameraConnectDialog.h"
#include "LogWidget.h"
#include "ThumbnailStrip.h"
#include "UI/CalibrationDialog.h"
#include "UI/EditMeasureDialog.h"
#include "UI/ManageRecipesDialog.h"
#include "UI/ResultOverlay.h"
#include "UI/TrainAlignmentDialog.h"

#include <QAction>
#include <QAbstractItemView>
#include <QBrush>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <cstdio>
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
#include <QMessageBox>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

using Me::CalibrationDialog;
using Me::CameraConnectDialog;
using Me::CameraHub;
using Me::EditMeasureDialog;
using Me::MeasureType;
using Me::ProjectService;
using Me::RunEngine;
using Me::TrainAlignmentDialog;

namespace {

void exitTrace(const char *step)
{
    FILE *f = nullptr;
    if (fopen_s(&f, "D:\\learning\\MeasureExpert_runtime\\log\\exit_trace.txt", "a") != 0 || !f) {
        return;
    }
    fprintf(f, "%s\n", step);
    fflush(f);
    fclose(f);
}

} // namespace

// ---- 构造与生命周期 ----

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_projectService(new ProjectService(this))
{
    // 跨线程信号槽（QueuedConnection）需注册自定义类型
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

    m_cameraHub = new CameraHub(this);
    connect(m_cameraHub, &CameraHub::frameReady, this, &MainWindow::onCameraFrameReady);
    connect(m_cameraHub, &CameraHub::openedChanged, this, &MainWindow::onCameraOpenedChanged);
    connect(m_cameraHub, &CameraHub::previewChanged, this, &MainWindow::onCameraPreviewChanged);
    connect(m_cameraHub, &CameraHub::errorOccurred, this, [this](const QString &msg) {
        qWarning().noquote() << tr("相机: %1").arg(msg);
        statusBar()->showMessage(tr("相机: %1").arg(msg), 5000);
    });

    createMenus();
    createToolBar();
    createStatusBar();
    setupRunEngine();
    applyAppModeUi();
    updateCameraActionsUi();
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
    exitTrace("dtor:begin");
    teardownOnExit();
    exitTrace("dtor:afterTeardown");
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
    // 关窗前先停工作线程/相机/日志，避免析构过程中还有排队槽打到半销毁对象
    teardownOnExit();
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

// ---- ROI 编辑模式 ----

void MainWindow::updateRoiActionTexts()
{
    if (m_trainAlignAction) {
        if (m_roiEditMode == RoiEditMode::AlignTemplate) {
            m_trainAlignAction->setText(tr("确认训练模板"));
        } else {
            m_trainAlignAction->setText(tr("创建定位模板"));
        }
    }
    if (m_drawMeasureRoiBtn) {
        m_drawMeasureRoiBtn->setText(
            (m_roiEditMode == RoiEditMode::MeasureCaliper) ? tr("确认ROI") : tr("画ROI"));
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
                tr("卡尺 ROI：左键拖拽绘制，右键确认后可拖图；拖橙色圆点旋转，角点缩放。调好后点「确认卡尺ROI」。Esc 取消。"),
                0);
            qInfo().noquote() << tr("已进入卡尺 ROI 编辑：请在主图上调整黄色矩形");
        }
    } else {
        statusBar()->showMessage(
            tr("模板 ROI：左键拖拽绘制，右键确认后可拖图；拖橙色圆点旋转。调好后点「确认训练模板」。Esc 取消。"),
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
    m_roiEditMeasureId.clear();
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

// ---- 测量几何：模板坐标 ↔ 图像坐标 ----

bool MainWindow::measureGeometryToImageRoi(const Me::CaliperGeometry &geo,
                                           const Me::AlignmentResult &align,
                                           Me::MeasureType type,
                                           QPointF *centerOut,
                                           qreal *phiOut,
                                           qreal *len1Out,
                                           qreal *len2Out) const
{
    if (!centerOut || !phiOut || !len1Out || !len2Out) {
        return false;
    }

    double rowImg = geo.row;
    double colImg = geo.column;
    double phiImg = geo.phi;
    if (align.ok) {
        if (!Me::ToolCommon::affineCaliper(align.homMat2D,
                                           geo.row,
                                           geo.column,
                                           geo.phi,
                                           &rowImg,
                                           &colImg,
                                           &phiImg)) {
            return false;
        }
    }

    *centerOut = QPointF(colImg, rowImg);
    // RoiRectItem 用 Qt φ（y 向下）；算法/配方存 HALCON φ
    *phiOut = Me::ToolCommon::halconPhiToQt(phiImg);

    if (type == MeasureType::FindCircle) {
        const double outer = qMax(4.0, geo.length1 + qMax(0.0, geo.length2));
        *len1Out = outer;
        *len2Out = outer;
        *phiOut = 0.0;
    } else {
        *len1Out = qMax(2.0, geo.length1);
        *len2Out = qMax(2.0, geo.length2);
    }
    return qIsFinite(rowImg) && qIsFinite(colImg) && qIsFinite(phiImg);
}

bool MainWindow::imageRoiToMeasureGeometry(const QPointF &centerImg,
                                           qreal phiImg,
                                           qreal length1,
                                           qreal length2,
                                           const Me::AlignmentResult &align,
                                           Me::CaliperGeometry *out) const
{
    if (!out || length1 < 2.0 || length2 < 2.0) {
        return false;
    }
    const double rowImg = centerImg.y();
    const double colImg = centerImg.x();
    const double phiHalcon = Me::ToolCommon::qtPhiToHalcon(phiImg);

    if (align.ok) {
        try {
            HalconCpp::HTuple invT;
            HomMat2dInvert(Me::ToolCommon::homTuple6(align.homMat2D), &invT);
            double inv6[6] = {1, 0, 0, 0, 1, 0};
            for (int i = 0; i < 6 && i < invT.Length(); ++i) {
                inv6[i] = invT[i].D();
            }
            if (!Me::ToolCommon::affineCaliper(inv6,
                                               rowImg,
                                               colImg,
                                               phiHalcon,
                                               &out->row,
                                               &out->column,
                                               &out->phi)) {
                return false;
            }
        } catch (...) {
            return false;
        }
    } else {
        out->row = rowImg;
        out->column = colImg;
        out->phi = phiHalcon;
    }
    out->length1 = qMax(2.0, length1);
    out->length2 = qMax(2.0, length2);
    return true;
}

// ---- RunEngine 工作线程 ----

void MainWindow::teardownOnExit()
{
    if (m_exitTornDown) {
        return;
    }
    m_exitTornDown = true;
    exitTrace("teardown:begin");

    m_stopRequested = true;
    m_continuous = false;
    m_continuousFromCamera = false;

    if (m_logWidget) {
        m_logWidget->detachFromLogSink();
        exitTrace("teardown:logDetached");
    }
    if (m_cameraHub) {
        disconnect(m_cameraHub, nullptr, this, nullptr);
        m_cameraHub->shutdown();
        exitTrace("teardown:cameraDown");
    }
    if (m_projectService) {
        disconnect(m_projectService, nullptr, this, nullptr);
    }
    // 丢掉已排队、指向本窗口的槽调用，避免半销毁时再进 refresh*
    QCoreApplication::removePostedEvents(this);
    if (m_logWidget) {
        QCoreApplication::removePostedEvents(m_logWidget);
    }
    exitTrace("teardown:beforeRunEngine");
    shutdownRunEngine();
    exitTrace("teardown:done");
}

void MainWindow::shutdownRunEngine()
{
    if (!m_runEngine && !m_runThread) {
        return;
    }

    RunEngine *engine = m_runEngine;
    QThread *thread = m_runThread;
    m_runEngine = nullptr;
    m_runThread = nullptr;

    if (engine) {
        disconnect(engine, nullptr, this, nullptr);
        disconnect(this, nullptr, engine, nullptr);
    }
    if (!thread) {
        delete engine;
        return;
    }

    // 不挂父对象：在工作线程事件循环里同步 delete this，再 quit/wait
    if (engine && thread->isRunning()) {
        exitTrace("runEngine:invokeShutdown");
        const bool ok = QMetaObject::invokeMethod(engine, &RunEngine::shutdown,
                                                  Qt::BlockingQueuedConnection);
        exitTrace(ok ? "runEngine:shutdownOk" : "runEngine:shutdownFail");
        engine = nullptr; // 已在工作线程 delete this
    } else if (engine) {
        delete engine;
        engine = nullptr;
        exitTrace("runEngine:deletedOnMain");
    }

    if (thread->isRunning()) {
        exitTrace("runEngine:quit");
        thread->quit();
        if (!thread->wait(5000)) {
            exitTrace("runEngine:terminate");
            thread->terminate();
            thread->wait(2000);
        }
    }
    delete thread;
    exitTrace("runEngine:threadDeleted");
}

void MainWindow::setupRunEngine()
{
    m_runThread = new QThread();
    m_runEngine = new RunEngine();
    m_runEngine->moveToThread(m_runThread);

    connect(m_runEngine, &RunEngine::cycleFinished, this, &MainWindow::onCycleFinished);
    connect(m_runEngine, &RunEngine::errorOccurred, this, &MainWindow::onRunError);

    m_runThread->start();
}

// ---- 界面布局：缩略图 | 主图 | 测量结果；底部日志 ----

bool MainWindow::initWidget()
{
    m_sourceThumbs = new ThumbnailStrip(ThumbnailStrip::Orientation::Vertical, this);
    connect(m_sourceThumbs, &ThumbnailStrip::currentImageChanged,
            this, &MainWindow::onThumbImageChanged);
    connect(m_sourceThumbs, &ThumbnailStrip::currentPathChanged,
            this, &MainWindow::onThumbPathChanged);

    m_activeRecipeLabel = new QLabel(this);
    m_activeRecipeLabel->setWordWrap(false);
    m_activeRecipeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_okNgLabel = new QLabel(tr("---"), this);
    m_okNgLabel->setAlignment(Qt::AlignCenter);
    m_okNgLabel->setFixedSize(72, 28);
    setTotalVerdict(-1);

    m_imageView = new CustomImageView(this);
    m_imageView->setMinimumSize(400, 300);
    connect(m_imageView, &CustomImageView::roiDrawConfirmed, this, [this]() {
        statusBar()->showMessage(
            tr("ROI 已确认：可拖动画布；拖角点缩放，拖橙色圆点旋转；选中后右键可删除。"),
            0);
        qInfo().noquote() << tr("ROI 绘制已确认（右键），可拖图并继续调整 ROI");
    });
    connect(m_imageView, &CustomImageView::roiDeleted, this, [this]() {
        statusBar()->showMessage(
            tr("ROI 已删除。左键拖拽重新绘制，右键确认。"),
            0);
        qInfo().noquote() << tr("ROI 已删除");
    });

    auto *imageBar = new QWidget(this);
    auto *imageBarLayout = new QHBoxLayout(imageBar);
    imageBarLayout->setContentsMargins(6, 2, 6, 2);
    imageBarLayout->setSpacing(8);
    imageBarLayout->addWidget(m_activeRecipeLabel, 1);
    imageBarLayout->addWidget(m_okNgLabel);

    auto *imageColumn = new QWidget(this);
    auto *imageLayout = new QVBoxLayout(imageColumn);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(2);
    imageLayout->addWidget(imageBar);
    imageLayout->addWidget(m_imageView, 1);

    auto *measurePanel = new QWidget(this);
    measurePanel->setMinimumWidth(280);
    measurePanel->setMaximumWidth(360);
    auto *measureLayout = new QVBoxLayout(measurePanel);
    measureLayout->setContentsMargins(4, 4, 4, 4);
    measureLayout->setSpacing(4);

    auto *title = new QLabel(tr("测量"), measurePanel);
    m_addMeasureBtn = new QPushButton(tr("添加"), measurePanel);
    m_editMeasureBtn = new QPushButton(tr("编辑"), measurePanel);
    m_drawMeasureRoiBtn = new QPushButton(tr("画ROI"), measurePanel);
    m_removeMeasureBtn = new QPushButton(tr("删除"), measurePanel);
    for (QPushButton *btn : {m_addMeasureBtn, m_editMeasureBtn, m_drawMeasureRoiBtn, m_removeMeasureBtn}) {
        btn->setFixedHeight(24);
    }
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(m_addMeasureBtn);
    titleRow->addWidget(m_editMeasureBtn);
    titleRow->addWidget(m_drawMeasureRoiBtn);
    titleRow->addWidget(m_removeMeasureBtn);
    measureLayout->addLayout(titleRow);

    m_resultTable = new QTableWidget(0, 4, measurePanel);
    m_resultTable->setHorizontalHeaderLabels(
        {tr("名称"), tr("测量值"), tr("公差"), tr("判定")});
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setShowGrid(false);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->verticalHeader()->setDefaultSectionSize(26);
    auto *header = m_resultTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::Fixed);
    m_resultTable->setColumnWidth(3, 48);
    measureLayout->addWidget(m_resultTable, 1);

    m_propertyToggle = new QToolButton(measurePanel);
    m_propertyToggle->setText(tr("属性"));
    m_propertyToggle->setCheckable(true);
    m_propertyToggle->setChecked(false);
    m_propertyToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_propertyToggle->setArrowType(Qt::RightArrow);
    m_propertyPanel = new QTextEdit(measurePanel);
    m_propertyPanel->setReadOnly(true);
    m_propertyPanel->setPlaceholderText(tr("选中测量项后展开，查看名义值与几何"));
    m_propertyPanel->setVisible(false);
    m_propertyPanel->setMaximumHeight(140);
    measureLayout->addWidget(m_propertyToggle);
    measureLayout->addWidget(m_propertyPanel);

    connect(m_propertyToggle, &QToolButton::toggled, this, [this](bool on) {
        m_propertyToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_propertyPanel->setVisible(on);
        if (on) {
            refreshMeasureProperties();
        }
    });
    connect(m_resultTable, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { onMeasureSelectionChanged(); });
    connect(m_resultTable, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onMeasureDoubleClicked(); });
    connect(m_addMeasureBtn, &QPushButton::clicked, this, &MainWindow::onAddMeasure);
    connect(m_editMeasureBtn, &QPushButton::clicked, this, &MainWindow::onEditMeasure);
    connect(m_drawMeasureRoiBtn, &QPushButton::clicked, this, &MainWindow::onDrawMeasureRoi);
    connect(m_removeMeasureBtn, &QPushButton::clicked, this, &MainWindow::onRemoveMeasure);

    auto *workRow = new QWidget(this);
    auto *workLayout = new QHBoxLayout(workRow);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->setSpacing(4);
    workLayout->addWidget(m_sourceThumbs);
    workLayout->addWidget(imageColumn, 1);
    workLayout->addWidget(measurePanel);

    m_logWidget = new LogWidget(this);
    m_logWidget->setMinimumHeight(72);

    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->addWidget(workRow);
    m_mainSplitter->addWidget(m_logWidget);
    m_mainSplitter->setStretchFactor(0, 5);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({640, 140});

    setCentralWidget(m_mainSplitter);
    return true;
}

// ---- 菜单 / 工具栏 / 状态栏 ----

void MainWindow::createMenus()
{
    // 工程：文件生命周期 + 标定（低频配置）
    auto *projectMenu = menuBar()->addMenu(tr("工程(&P)"));
    projectMenu->addAction(tr("新建工程"), this, &MainWindow::onNewProject);
    projectMenu->addAction(tr("打开工程..."), this, &MainWindow::onOpenProject);
    projectMenu->addAction(tr("保存工程"), this, &MainWindow::onSaveProject);
    projectMenu->addAction(tr("工程另存为..."), this, &MainWindow::onSaveProjectAs);
    projectMenu->addSeparator();
    projectMenu->addAction(tr("标定..."), this, &MainWindow::onCalibration);
    projectMenu->addSeparator();
    projectMenu->addAction(tr("退出"), this, &QWidget::close);

    // 采集：离线图源 + 相机（仅菜单）
    auto *acqMenu = menuBar()->addMenu(tr("采集(&A)"));
    acqMenu->addAction(tr("加载图像..."), this, &MainWindow::onLoadImage);
    acqMenu->addAction(tr("加载图像文件夹..."), this, &MainWindow::onLoadFolder);
    acqMenu->addSeparator();
    m_connectCameraAction = acqMenu->addAction(tr("连接相机..."), this, &MainWindow::onConnectCamera);
    m_grabCameraAction = acqMenu->addAction(tr("采集一帧"), this, &MainWindow::onGrabCameraFrame);
    m_previewCameraAction = acqMenu->addAction(tr("开始预览"), this, &MainWindow::onToggleCameraPreview);
    m_disconnectCameraAction = acqMenu->addAction(tr("断开相机"), this, &MainWindow::onDisconnectCamera);

    // 配方
    auto *recipeMenu = menuBar()->addMenu(tr("配方(&F)"));
    m_manageRecipesAction = recipeMenu->addAction(tr("管理配方..."), this, &MainWindow::onManageRecipes);

    // 运行菜单：定位模板 / 取消 ROI；试跑与模式只在工具栏；画 ROI / 编辑只在右侧面板
    auto *runMenu = menuBar()->addMenu(tr("运行(&R)"));
    m_trainAlignAction = runMenu->addAction(tr("创建定位模板..."), this, &MainWindow::onTrainAlignment);
    m_cancelRoiAction = runMenu->addAction(tr("取消 ROI 编辑"), this, &MainWindow::onCancelRoiEdit);
    m_cancelRoiAction->setEnabled(false);

    // 工具栏用的高频动作（不进菜单）
    m_runOnceAction = new QAction(tr("单次试跑"), this);
    connect(m_runOnceAction, &QAction::triggered, this, &MainWindow::onRunOnce);
    m_continuousAction = new QAction(tr("连续运行"), this);
    connect(m_continuousAction, &QAction::triggered, this, &MainWindow::onContinuousRun);
    m_stopAction = new QAction(tr("停止"), this);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopRun);
    m_stopAction->setEnabled(false);
    m_modeToggleAction = new QAction(tr("切换到运行模式"), this);
    connect(m_modeToggleAction, &QAction::triggered, this, &MainWindow::onToggleEditRunMode);
}

void MainWindow::createToolBar()
{
    auto *tb = addToolBar(tr("主工具栏"));
    tb->setMovable(false);
    // 仅放高频节拍控件；标定/定位模板在菜单，画 ROI 在右侧面板
    tb->addAction(m_runOnceAction);
    tb->addAction(m_continuousAction);
    tb->addAction(m_stopAction);
    tb->addSeparator();
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
    if (!m_resultTable) {
        return {};
    }
    const int row = m_resultTable->currentRow();
    if (row < 0) {
        return {};
    }
    const auto *item = m_resultTable->item(row, 0);
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
    // 编辑能力：仅 Edit 模式且未在试跑；运行按钮：未 busy/continuous 时可启动
    const bool edit = (m_appMode == AppMode::Edit);
    const bool canEdit = edit && !m_busy && !m_continuous;

    const bool hasMeasure = m_resultTable && m_resultTable->rowCount() > 0;
    m_addMeasureBtn->setEnabled(canEdit);
    m_editMeasureBtn->setEnabled(canEdit && hasMeasure);
    m_drawMeasureRoiBtn->setEnabled(canEdit && hasMeasure);
    m_removeMeasureBtn->setEnabled(canEdit && hasMeasure);
    if (m_manageRecipesAction) {
        m_manageRecipesAction->setEnabled(canEdit);
    }

    if (m_trainAlignAction) {
        m_trainAlignAction->setEnabled(canEdit);
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
    updateCameraActionsUi();
}

// ---- 列表刷新与属性面板 ----

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
    QString keepMeasureId = selectedMeasureId();
    if (!m_pendingSelectMeasureId.isEmpty()) {
        keepMeasureId = m_pendingSelectMeasureId;
        m_pendingSelectMeasureId.clear();
    }
    m_resultTable->setRowCount(0);

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
        const int dec = qBound(0, m.decimals, 6);
        m_resultTable->insertRow(i);
        QString label = QStringLiteral("%1  %2").arg(m.name, Me::measureTypeToString(m.type));
        if (Me::ToolCommon::measureRoiMissing(m)) {
            label += tr("  未画ROI");
        }
        auto *nameItem = new QTableWidgetItem(label);
        nameItem->setData(Qt::UserRole, m.id);
        nameItem->setToolTip(m.name);
        m_resultTable->setItem(i, 0, nameItem);

        auto *valueItem = new QTableWidgetItem(QStringLiteral("--"));
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_resultTable->setItem(i, 1, valueItem);

        auto *tolItem = new QTableWidgetItem(
            QStringLiteral("[%1, %2]")
                .arg(m.nominal + m.lower, 0, 'f', dec)
                .arg(m.nominal + m.upper, 0, 'f', dec));
        tolItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_resultTable->setItem(i, 2, tolItem);

        auto *judgeItem = new QTableWidgetItem(QStringLiteral("-"));
        judgeItem->setTextAlignment(Qt::AlignCenter);
        m_resultTable->setItem(i, 3, judgeItem);

        if (m.id == keepMeasureId) {
            selectRow = i;
        }
    }

    if (m_resultTable->rowCount() > 0) {
        m_resultTable->setCurrentCell(selectRow, 0);
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
    text += tr("ROI: %1\n").arg(m.roiDefined ? tr("已确认") : tr("未画"));
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

void MainWindow::setTotalVerdict(int state)
{
    if (!m_okNgLabel) {
        return;
    }
    if (state > 0) {
        m_okNgLabel->setText(QStringLiteral("OK"));
        m_okNgLabel->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 16px; font-weight: bold; background: #1f5c2e; color: #e8ffe8; border-radius: 4px; }"));
    } else if (state == 0) {
        m_okNgLabel->setText(QStringLiteral("NG"));
        m_okNgLabel->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 16px; font-weight: bold; background: #6b1f1f; color: #ffe8e8; border-radius: 4px; }"));
    } else {
        m_okNgLabel->setText(QStringLiteral("---"));
        m_okNgLabel->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 16px; font-weight: bold; background: #3a3a3a; color: #dddddd; border-radius: 4px; }"));
    }
}

void MainWindow::applyCycleResult(const Me::CycleResult &result)
{
    if (result.alignmentOk || !m_currentImage.isNull()) {
        m_imageView->setImage(makeOverlayImage(m_currentImage, result));
    }

    setTotalVerdict(result.totalOk ? 1 : 0);

    for (int row = 0; row < m_resultTable->rowCount(); ++row) {
        auto *nameItem = m_resultTable->item(row, 0);
        if (!nameItem) {
            continue;
        }
        const QString id = nameItem->data(Qt::UserRole).toString();
        const Me::MeasureResult *found = nullptr;
        for (const auto &m : result.measures) {
            if (m.id == id) {
                found = &m;
                break;
            }
        }
        auto *valueItem = m_resultTable->item(row, 1);
        auto *judgeItem = m_resultTable->item(row, 3);
        if (!valueItem || !judgeItem) {
            continue;
        }
        if (!found) {
            valueItem->setText(QStringLiteral("--"));
            judgeItem->setText(QStringLiteral("-"));
            judgeItem->setForeground(QBrush());
            for (int col = 0; col < m_resultTable->columnCount(); ++col) {
                if (auto *cell = m_resultTable->item(row, col)) {
                    cell->setBackground(QBrush());
                }
            }
            continue;
        }
        const int dec = qBound(0, found->decimals, 6);
        valueItem->setText(found->measured
                               ? QStringLiteral("%1 %2").arg(found->value, 0, 'f', dec).arg(found->unit)
                               : QStringLiteral("--"));
        judgeItem->setText(found->ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        // 深色界面下不整行铺浅粉/浅绿底，只改判定列字色，避免白字看不清
        judgeItem->setForeground(found->ok ? QColor(80, 220, 120) : QColor(255, 110, 110));
        for (int col = 0; col < m_resultTable->columnCount(); ++col) {
            if (auto *cell = m_resultTable->item(row, col)) {
                cell->setBackground(QBrush());
            }
        }
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

// ---- 工程文件操作 ----

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
    setTotalVerdict(-1);
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

// ---- 图像加载 ----

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

// ---- 物理相机 ----

void MainWindow::applyCameraFrame(const QImage &image, bool fromPreview)
{
    if (image.isNull()) {
        return;
    }
    m_currentImage = image;
    // 连续运行：只更新算法用图，主视图保留上一周期叠加，等 onCycleFinished 再刷
    if (!m_continuous) {
        m_imageView->setImage(image);
    }
    const QString name = m_cameraHub && m_cameraHub->isOpen()
        ? m_cameraHub->currentDevice().displayName
        : tr("相机");
    m_currentImagePath = fromPreview
        ? QStringLiteral("camera://%1/preview").arg(name)
        : QStringLiteral("camera://%1/frame").arg(name);
    updateImageSourceStatus(tr("相机 · %1").arg(name));
}

bool MainWindow::grabCameraFrameToCurrent(QString *error)
{
    if (!m_cameraHub || !m_cameraHub->isOpen()) {
        if (error) {
            *error = tr("相机未连接");
        }
        return false;
    }
    // 采测量帧时暂停预览，避免与 grab 抢同一设备
    const bool wasPreview = m_cameraHub->isPreviewing();
    if (wasPreview) {
        m_cameraHub->stopPreview();
    }
    QImage frame;
    QString err;
    const bool ok = m_cameraHub->grabOnce(&frame, &err);
    if (!ok) {
        if (error) {
            *error = err;
        }
        if (wasPreview) {
            m_cameraHub->startPreview();
        }
        return false;
    }
    applyCameraFrame(frame, false);
    return true;
}

void MainWindow::updateCameraActionsUi()
{
    const bool open = m_cameraHub && m_cameraHub->isOpen();
    const bool preview = open && m_cameraHub->isPreviewing();
    const bool idle = !m_busy && !m_continuous;
    if (m_connectCameraAction) {
        m_connectCameraAction->setEnabled(idle);
    }
    if (m_grabCameraAction) {
        m_grabCameraAction->setEnabled(open && idle);
    }
    if (m_previewCameraAction) {
        m_previewCameraAction->setEnabled(open && idle);
        m_previewCameraAction->setText(preview ? tr("停止预览") : tr("开始预览"));
    }
    if (m_disconnectCameraAction) {
        m_disconnectCameraAction->setEnabled(open && idle);
    }
}

void MainWindow::onConnectCamera()
{
    if (!m_cameraHub || m_busy || m_continuous) {
        return;
    }
    CameraConnectDialog dlg(m_cameraHub, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const auto device = dlg.selectedDevice();
    if (device.deviceId.isEmpty()) {
        return;
    }
    QString err;
    if (!m_cameraHub->openDevice(device, &err)) {
        QMessageBox::warning(this, tr("连接失败"), err);
        return;
    }
    m_projectService->setImageSourceCamera(device.backendId, device.deviceId, device.displayName);
    updateWindowTitle();
    qInfo().noquote() << tr("已连接相机: %1 (%2/%3)")
                             .arg(device.displayName, device.backendId, device.deviceId);

    QString grabErr;
    if (grabCameraFrameToCurrent(&grabErr)) {
        statusBar()->showMessage(tr("相机已连接并采得一帧"), 5000);
    } else {
        statusBar()->showMessage(tr("相机已连接，但首帧失败: %1").arg(grabErr), 8000);
    }
    updateCameraActionsUi();
}

void MainWindow::onGrabCameraFrame()
{
    if (m_busy || m_continuous) {
        return;
    }
    QString err;
    if (!grabCameraFrameToCurrent(&err)) {
        QMessageBox::warning(this, tr("采图失败"), err);
        return;
    }
    qInfo().noquote() << tr("已采集一帧 (%1×%2)")
                             .arg(m_currentImage.width())
                             .arg(m_currentImage.height());
    statusBar()->showMessage(tr("已采集一帧，可单次试跑"), 4000);
    updateCameraActionsUi();
}

void MainWindow::onToggleCameraPreview()
{
    if (!m_cameraHub || !m_cameraHub->isOpen() || m_busy || m_continuous) {
        return;
    }
    if (m_cameraHub->isPreviewing()) {
        m_cameraHub->stopPreview();
        qInfo().noquote() << tr("已停止相机预览");
    } else {
        m_cameraHub->startPreview();
        qInfo().noquote() << tr("已开始相机预览");
    }
    updateCameraActionsUi();
}

void MainWindow::onDisconnectCamera()
{
    if (!m_cameraHub) {
        return;
    }
    m_cameraHub->closeDevice();
    qInfo().noquote() << tr("已断开相机");
    statusBar()->showMessage(tr("已断开相机"), 3000);
    updateCameraActionsUi();
}

void MainWindow::onCameraFrameReady(const QImage &image)
{
    // 预览帧：只更新显示与当前图，不跑算法
    applyCameraFrame(image, true);
}

void MainWindow::onCameraOpenedChanged(bool open)
{
    Q_UNUSED(open);
    updateCameraActionsUi();
}

void MainWindow::onCameraPreviewChanged(bool on)
{
    Q_UNUSED(on);
    updateCameraActionsUi();
}

// ---- 标定 ----

void MainWindow::onCalibration()
{
    CalibrationDialog dlg(m_projectService->project().calibration, this);
    connect(&dlg,
            &CalibrationDialog::previewRequested,
            this,
            [this](const QImage &image, const QString &caption) {
                if (!image.isNull()) {
                    m_imageView->setImage(image);
                }
                if (!caption.isEmpty()) {
                    statusBar()->showMessage(caption, 8000);
                }
            });
    connect(&dlg,
            &CalibrationDialog::calibrationVisualsReady,
            this,
            [this](const QStringList &labels, const QVector<QImage> &images) {
                if (!m_sourceThumbs->loadImages(labels, images)) {
                    qWarning().noquote() << tr("标定可视化结果未载入缩略图");
                    return;
                }
                // 若含畸变对比项，默认选中最后一张（对比图）
                if (!labels.isEmpty()
                    && labels.last().contains(QStringLiteral("畸变矫正对比"))) {
                    m_sourceThumbs->setCurrentIndex(m_sourceThumbs->count() - 1);
                }
                qInfo().noquote() << tr("标定可视化已载入列表（%1 张）")
                                         .arg(m_sourceThumbs->count());
                statusBar()->showMessage(
                    tr("左侧列表：角点结果 + 畸变矫正对比，点选可查看"),
                    8000);
            });

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    m_projectService->setCalibration(dlg.result());
    updateCalibrationStatus();
    qInfo().noquote() << tr("已更新标定: calibrated=%1 pixelSizeMm=%2")
                             .arg(m_projectService->project().calibration.calibrated)
                             .arg(m_projectService->project().calibration.pixelSizeMm);
    statusBar()->showMessage(
        tr("标定已写入工程；可在左侧列表点选标定图查看，主图当前为标定预览"),
        10000);
}

// ---- 定位模板训练（二次点击：进入 ROI → 确认训练） ----

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

// ---- 测量 ROI（followPose：有模型则相对模板，否则图像坐标） ----

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
    QString measureId = selectedMeasureId();
    if (m_roiEditMode == RoiEditMode::MeasureCaliper && !m_roiEditMeasureId.isEmpty()) {
        measureId = m_roiEditMeasureId;
    }
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
    const bool followPose = !alignCfg.modelFile.isEmpty();

    const auto t = measureCopy.type;
    if (t == MeasureType::PointToLine || t == MeasureType::LineIntersection
        || t == MeasureType::Angle || t == MeasureType::PointDistance) {
        QMessageBox::information(this, tr("提示"),
                                 tr("派生测量项通过编辑对话框设置引用 A/B，无需画 ROI。"));
        return;
    }

    // 第一次点击：进入编辑，按 followPose 投影已有几何为种子 ROI
    // 第二次点击：确认写入几何（有定位则相对模板，否则图像坐标）
    if (m_roiEditMode != RoiEditMode::MeasureCaliper) {
        Me::AlignmentResult align;
        if (followPose) {
            // followPose：用当前图定位位姿，把相对模板几何反投影到图像
            align = Me::AlignmentService::find(m_currentImage, alignCfg, projectDir(), false);
        }
        QPointF seedCenter;
        qreal seedPhi = 0.0;
        qreal seedL1 = 0.0;
        qreal seedL2 = 0.0;
        bool haveSeed = false;
        if (measureCopy.roiDefined && followPose && align.ok) {
            haveSeed = measureGeometryToImageRoi(measureCopy.geometry, align, measureCopy.type,
                                                 &seedCenter, &seedPhi, &seedL1, &seedL2);
            qInfo().noquote() << tr("已按当前定位位姿投影卡尺 (score=%1)")
                                     .arg(align.score, 0, 'f', 3);
        } else if (measureCopy.roiDefined && !followPose) {
            haveSeed = measureGeometryToImageRoi(measureCopy.geometry, {}, measureCopy.type,
                                                 &seedCenter, &seedPhi, &seedL1, &seedL2);
            qInfo().noquote() << tr("已载入图像坐标 ROI（未启用定位跟随）");
        }
        if (!haveSeed) {
            const int w = m_currentImage.width();
            const int h = m_currentImage.height();
            if (measureCopy.type == MeasureType::FindCircle) {
                const double side = qMin(w, h) * 0.3;
                seedCenter = QPointF(w * 0.5, h * 0.5);
                seedPhi = 0.0;
                seedL1 = seedL2 = side * 0.5;
            } else {
                seedCenter = QPointF(w * 0.5, h * 0.5);
                seedPhi = 0.0;
                seedL1 = w * 0.15;
                seedL2 = h * 0.04;
            }
            if (followPose && !align.ok) {
                qWarning().noquote() << tr("定位失败，使用默认 ROI；确认时将写入图像坐标。%1")
                                            .arg(align.message);
            }
        }
        m_imageView->setRoiShape(measureCopy.type == MeasureType::FindCircle
                                     ? RoiRectItem::Shape::Circle
                                     : RoiRectItem::Shape::Rectangle);
        m_imageView->setRoiOriented(seedCenter, seedPhi, seedL1, seedL2);
        m_roiEditMeasureId = measureCopy.id;
        enterRoiEditMode(true);
        qInfo().noquote() << tr("正在编辑测量项「%1」的 ROI").arg(measureCopy.name);
        return;
    }

    if (!m_imageView->hasRoi()) {
        QMessageBox::information(this, tr("提示"),
                                 measureCopy.type == MeasureType::FindCircle
                                     ? tr("请先在主图上画出有效的圆形 ROI。")
                                     : tr("请先在主图上画出有效的卡尺矩形。"));
        return;
    }

    // --- ROI 确认：再次定位，将图像定向 ROI 逆变换为相对模板 CaliperGeometry ---
    qInfo().noquote() << tr("确认测量项「%1」的卡尺 ROI").arg(measureCopy.name);
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

    const QPointF centerImg = m_imageView->roiCenter();
    const qreal phiImg = m_imageView->roiPhi();
    const qreal len1Img = m_imageView->roiLength1();
    const qreal len2Img = m_imageView->roiLength2();

    Me::CaliperGeometry geo;
    if (!imageRoiToMeasureGeometry(centerImg, phiImg, len1Img, len2Img, align, &geo)) {
        QMessageBox::warning(this, tr("无法换算"), tr("卡尺 ROI 无效。"));
        return;
    }
    // 找圆：正圆外径 → 半径 length1 + 环宽 length2
    if (measureCopy.type == MeasureType::FindCircle) {
        const double outerR = qMax(4.0, qMin(len1Img, len2Img));
        double ann = measureCopy.geometry.length2;
        if (!(ann > 0.0) || ann >= outerR) {
            ann = qMax(2.0, outerR * 0.15);
        }
        if (ann >= outerR) {
            ann = qMax(2.0, outerR * 0.15);
        }
        geo.length1 = qMax(2.0, outerR - ann);
        geo.length2 = ann;
        geo.phi = 0.0;
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
    item.roiDefined = true;

    // 先退出 ROI 模式，再写回（避免 structureChanged / 刷新与 ROI 交互冲突）
    leaveRoiEditMode(false);
    if (!m_projectService->updateMeasure(recipeId, item)) {
        qWarning().noquote() << tr("测量 ROI 写回失败: %1").arg(item.name);
        return;
    }
    if (auto *storedRecipe = m_projectService->activeRecipe()) {
        const int storedIdx = storedRecipe->indexOfMeasure(item.id);
        if (storedIdx >= 0) {
            storedRecipe->measures[storedIdx].roiDefined = true;
            storedRecipe->measures[storedIdx].geometry = item.geometry;
        }
    }
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
    mr.imageRow = centerImg.y();
    mr.imageCol = centerImg.x();
    mr.imagePhi = Me::ToolCommon::qtPhiToHalcon(phiImg);
    mr.length1 = geo.length1;
    mr.length2 = geo.length2;
    mr.angleExtent = geo.angleExtent;
    mr.message = tr("测量ROI已更新");
    preview.measures.push_back(mr);
    if (const auto *recipeNow = m_projectService->project().findRecipe(recipeId)) {
        const Me::AlignmentResult sibAlign = align.ok ? align : Me::AlignmentResult{};
        if (!followPose || align.ok) {
            for (const Me::MeasureItem &sib : recipeNow->measures) {
                if (sib.id == item.id || !sib.roiDefined
                    || !Me::ToolCommon::measureNeedsRoi(sib.type)) {
                    continue;
                }
                QPointF center;
                qreal phi = 0.0;
                qreal len1 = 0.0;
                qreal len2 = 0.0;
                if (!measureGeometryToImageRoi(sib.geometry, sibAlign, sib.type,
                                               &center, &phi, &len1, &len2)) {
                    continue;
                }
                Me::MeasureResult other;
                other.id = sib.id;
                other.name = sib.name;
                other.type = sib.type;
                other.ok = true;
                other.imageRow = center.y();
                other.imageCol = center.x();
                other.imagePhi = (sib.type == MeasureType::FindCircle)
                    ? 0.0
                    : Me::ToolCommon::qtPhiToHalcon(phi);
                other.length1 = (sib.type == MeasureType::FindCircle)
                    ? sib.geometry.length1
                    : len1;
                other.length2 = (sib.type == MeasureType::FindCircle)
                    ? sib.geometry.length2
                    : len2;
                other.angleExtent = sib.geometry.angleExtent;
                preview.measures.push_back(other);
            }
        }
    }
    preview.totalOk = true;
    preview.reason = tr("测量ROI已写入");
    m_imageView->setImage(Me::ResultOverlay::render(m_currentImage, preview));

    qInfo().noquote() << tr("测量 ROI 已更新: %1  row=%2 col=%3 phi=%4 L1=%5 L2=%6")
                             .arg(item.name)
                             .arg(geo.row, 0, 'f', 2)
                             .arg(geo.column, 0, 'f', 2)
                             .arg(geo.phi, 0, 'f', 4)
                             .arg(geo.length1, 0, 'f', 2)
                             .arg(geo.length2, 0, 'f', 2);
}

// ---- 测量项编辑 ----

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

// ---- 试跑：单次 / 连续 / 停止 ----

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

    m_continuousFromCamera = m_cameraHub && m_cameraHub->isOpen();
    if (!m_continuousFromCamera && m_sourceThumbs->count() <= 0) {
        QMessageBox::information(
            this,
            tr("提示"),
            tr("连续运行需要先加载图像文件夹，或连接相机。"));
        return;
    }

    if (m_continuousFromCamera && m_cameraHub->isPreviewing()) {
        m_cameraHub->stopPreview();
    }

    m_continuous = true;
    m_stopRequested = false;
    applyAppModeUi();
    updateCameraActionsUi();
    updateCalibrationStatus();
    qInfo().noquote() << (m_continuousFromCamera
                              ? tr("开始连续运行（相机采图→测量，点停止结束）...")
                              : tr("开始连续运行（文件夹序列循环，点停止结束）..."));
    dispatchRunOnce();
}

void MainWindow::onStopRun()
{
    if (!m_busy && !m_continuous) {
        return;
    }
    m_stopRequested = true;
    m_continuous = false;
    m_continuousFromCamera = false;
    applyAppModeUi();
    updateCameraActionsUi();
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

    // 非连续或用户已点停止：结束 busy 状态
    if (m_stopRequested || !m_continuous) {
        m_continuous = false;
        m_continuousFromCamera = false;
        m_stopRequested = false;
        setBusyUi(false);
        updateCameraActionsUi();
        return;
    }

    setBusyUi(false); // 周期间隙短暂释放 UI，便于响应停止
    if (m_stopRequested) {
        m_continuous = false;
        m_continuousFromCamera = false;
        m_stopRequested = false;
        applyAppModeUi();
        updateCameraActionsUi();
        return;
    }

    if (m_continuousFromCamera) {
        QString err;
        if (!grabCameraFrameToCurrent(&err)) {
            m_continuous = false;
            m_continuousFromCamera = false;
            setBusyUi(false);
            updateCameraActionsUi();
            qWarning().noquote() << tr("连续运行中断：相机采图失败 — %1").arg(err);
            QMessageBox::warning(this, tr("连续运行中断"), err);
            return;
        }
    } else {
        if (m_sourceThumbs->count() <= 0) {
            m_continuous = false;
            m_continuousFromCamera = false;
            setBusyUi(false);
            updateCameraActionsUi();
            qWarning().noquote() << tr("连续运行中断：无图像");
            return;
        }
        m_sourceThumbs->selectNext();
    }

    m_continuous = true;
    dispatchRunOnce();
}

void MainWindow::onRunError(const QString &message)
{
    qWarning().noquote() << message;
}

// ---- 缩略图 / 配方 / 测量项槽 ----

void MainWindow::onThumbImageChanged(const QImage &image)
{
    m_currentImage = image;
    // 连续运行切下一张时勿用原图盖掉上一周期结果叠加
    if (!m_continuous) {
        m_imageView->setImage(image);
    }
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
        item.roiDefined = false;
        m_pendingSelectMeasureId = id;
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
