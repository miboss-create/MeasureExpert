#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Alg/AlgTypes.h"

#include <QImage>
#include <QMainWindow>
#include <QString>

namespace Me {
class ProjectService;
class RunEngine;
class CameraHub;
}

class QAction;
class QCloseEvent;
class QEvent;
class QLabel;
class QTextEdit;
class QTableWidget;
class QSplitter;
class QPushButton;
class QToolButton;
class QThread;
class CustomImageView;
class LogWidget;
class ThumbnailStrip;

/**
 * 主窗口：MeasureExpert 的核心 UI 壳层。
 *
 * 职责概览：
 * - 工程/配方/测量项的编辑入口（委托 ProjectService 持久化）
 * - 主图 ROI 交互（定位模板、卡尺/找圆几何）
 * - 后台 RunEngine 单次/连续试跑，结果叠加与 CSV 输出
 * - 编辑模式 vs 运行模式：后者试跑会写 CSV
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // ---- 工程（菜单「工程」）----
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();

    // ---- 图像源 ----
    void onLoadImage();
    void onLoadFolder();
    void onConnectCamera();
    void onGrabCameraFrame();
    void onToggleCameraPreview();
    void onDisconnectCamera();
    void onCameraFrameReady(const QImage &image);
    void onCameraOpenedChanged(bool open);
    void onCameraPreviewChanged(bool on);
    void onThumbImageChanged(const QImage &image);
    void onThumbPathChanged(const QString &path);

    // ---- 标定与定位 ----
    void onCalibration();
    void onTrainAlignment();

    // ---- ROI 编辑 ----
    void onCancelRoiEdit();
    void onDrawMeasureRoi();

    // ---- 测量项 ----
    void onEditMeasure();
    void onMeasureSelectionChanged();
    void onMeasureDoubleClicked();
    void onAddMeasure();
    void onRemoveMeasure();

    // ---- 试跑 ----
    void onRunOnce();
    void onContinuousRun();
    void onStopRun();
    void onToggleEditRunMode();
    void onCycleFinished(const Me::CycleResult &result);
    void onRunError(const QString &message);

    // ---- 配方 ----
    void onRecipeSelectionChanged();
    void onAddRecipe();
    void onRemoveRecipe();
    void onManageRecipes();

    // ---- 工程状态 ----
    void onProjectStructureChanged();
    void onDirtyChanged(bool dirty);

private:
    /** 应用模式：编辑（配置/试跑不写 CSV）vs 运行（试跑写 CSV） */
    enum class AppMode {
        Edit,
        Run
    };

    /** ROI 编辑子模式；None 表示未在编辑 */
    enum class RoiEditMode {
        None,
        AlignTemplate,   ///< 绿色矩形，训练形状模型
        MeasureCaliper   ///< 黄色矩形/圆，写入测量几何
    };

    // ---- 初始化 ----
    bool initWidget();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void setupRunEngine();
    /** 析构/关窗前安全停止工作线程并 deleteLater RunEngine */
    void shutdownRunEngine();
    /** 关窗与析构共用：停线程、相机、断开日志，只执行一次 */
    void teardownOnExit();

    // ---- 窗口与列表刷新 ----
    void updateWindowTitle();
    void updateImageSourceStatus(const QString &source);
    void updateCalibrationStatus();
    void refreshRecipeList();
    void refreshMeasureList();
    void refreshMeasureProperties();
    /** 拖拽结果表行后，按表内 id 顺序写回配方 */
    void syncMeasureOrderFromTable();
    void applyAppModeUi();
    void setBusyUi(bool busy);
    /** 更新主图旁定位得分/角度标签 */
    void updateAlignmentStatusUi(const Me::CycleResult &result);

    // ---- 试跑调度 ----
    bool prepareRun(QString *failReason = nullptr) const;
    /** 经 QueuedConnection 向工作线程派发 runOnce */
    void dispatchRunOnce();
    void applyCycleResult(const Me::CycleResult &result);
    /** state: 1=OK, 0=NG, 其他=尚未判定 */
    void setTotalVerdict(int state);
    QImage makeOverlayImage(const QImage &source, const Me::CycleResult &result) const;
    void maybeWriteCsv(const Me::CycleResult &result);
    /** 将相机帧设为当前工作图（供预览/单帧/连续采图） */
    void applyCameraFrame(const QImage &image, bool fromPreview);
    bool grabCameraFrameToCurrent(QString *error = nullptr);
    void updateCameraActionsUi();

    // ---- 工程持久化 ----
    void maybeLoadImageFolderFromProject();
    bool maybeSaveBeforeContinue();
    bool saveProjectInternal(bool forceSaveAs);
    QString selectedRecipeId() const;
    QString selectedMeasureId() const;
    QString projectDir() const;

    // ---- ROI 编辑 ----
    void enterRoiEditMode(bool forMeasure);
    void leaveRoiEditMode(bool clearRoi);
    void updateRoiActionTexts();
    /** 相对模板几何 + 定位位姿 → 图像坐标定向 ROI（中心/phi/半长） */
    bool measureGeometryToImageRoi(const Me::CaliperGeometry &geo,
                                   const Me::AlignmentResult &align,
                                   Me::MeasureType type,
                                   QPointF *centerOut,
                                   qreal *phiOut,
                                   qreal *len1Out,
                                   qreal *len2Out) const;
    /** 图像定向 ROI → 相对模板（或图像）CaliperGeometry；确认卡尺 ROI 时调用 */
    bool imageRoiToMeasureGeometry(const QPointF &centerImg,
                                   qreal phiImg,
                                   qreal length1,
                                   qreal length2,
                                   const Me::AlignmentResult &align,
                                   Me::CaliperGeometry *out) const;

    Me::ProjectService *m_projectService = nullptr;
    Me::RunEngine *m_runEngine = nullptr;
    Me::CameraHub *m_cameraHub = nullptr;
    QThread *m_runThread = nullptr;
    QAction *m_trainAlignAction = nullptr;
    QAction *m_cancelRoiAction = nullptr;
    QAction *m_runOnceAction = nullptr;
    QAction *m_continuousAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_modeToggleAction = nullptr;
    QAction *m_manageRecipesAction = nullptr;
    QAction *m_connectCameraAction = nullptr;
    QAction *m_grabCameraAction = nullptr;
    QAction *m_previewCameraAction = nullptr;
    QAction *m_disconnectCameraAction = nullptr;
    RoiEditMode m_roiEditMode = RoiEditMode::None;
    /** 进入画 ROI 时锁定的测量项。确认时写回这一项，不跟随后来的列表选中行。 */
    QString m_roiEditMeasureId;
    /** 刷新列表后要选中的测量项（添加后选中新建项） */
    QString m_pendingSelectMeasureId;
    AppMode m_appMode = AppMode::Edit;

    QLabel *m_activeRecipeLabel = nullptr;
    QPushButton *m_addMeasureBtn = nullptr;
    QPushButton *m_removeMeasureBtn = nullptr;
    QPushButton *m_editMeasureBtn = nullptr;
    QPushButton *m_drawMeasureRoiBtn = nullptr;
    QToolButton *m_propertyToggle = nullptr;
    LogWidget *m_logWidget = nullptr;

    ThumbnailStrip *m_sourceThumbs = nullptr;
    CustomImageView *m_imageView = nullptr;

    QLabel *m_okNgLabel = nullptr;
    QLabel *m_alignStatusLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_propertyPanel = nullptr;
    bool m_reorderingMeasures = false; ///< 拖拽排序写回时抑制列表刷新打断

    QSplitter *m_mainSplitter = nullptr;
    bool m_updatingLists = false;   ///< 刷新列表时抑制 selectionChanged 重入
    bool m_busy = false;            ///< 单次/连续周期进行中
    bool m_continuous = false;      ///< 连续运行（文件夹序列或相机连续采图）
    bool m_stopRequested = false;   ///< 用户点停止，当前周期结束后不再派发
    bool m_continuousFromCamera = false; ///< 连续运行使用相机 grab 而非缩略图翻页
    bool m_exitTornDown = false;    ///< closeEvent/析构已做过退出清理
    QString m_currentImageStatus;
    QString m_currentImagePath;
    QImage m_currentImage;
};

#endif // MAINWINDOW_H
