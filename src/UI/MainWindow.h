#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Alg/AlgTypes.h"

#include <QImage>
#include <QMainWindow>
#include <QString>

namespace Me {
class ProjectService;
class RunEngine;
}

class QAction;
class QCloseEvent;
class QLabel;
class QListWidget;
class QTextEdit;
class QTableWidget;
class QSplitter;
class QPushButton;
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

private slots:
    // ---- 工程（菜单「工程」）----
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();

    // ---- 图像源 ----
    void onLoadImage();
    void onLoadFolder();
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

    // ---- 窗口与列表刷新 ----
    void updateWindowTitle();
    void updateImageSourceStatus(const QString &source);
    void updateCalibrationStatus();
    void refreshRecipeList();
    void refreshMeasureList();
    void refreshMeasureProperties();
    void applyAppModeUi();
    void setBusyUi(bool busy);

    // ---- 试跑调度 ----
    bool prepareRun(QString *failReason = nullptr) const;
    /** 经 QueuedConnection 向工作线程派发 runOnce */
    void dispatchRunOnce();
    void applyCycleResult(const Me::CycleResult &result);
    QImage makeOverlayImage(const QImage &source, const Me::CycleResult &result) const;
    void maybeWriteCsv(const Me::CycleResult &result);

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
    QThread *m_runThread = nullptr;
    QAction *m_trainAlignAction = nullptr;
    QAction *m_drawMeasureRoiAction = nullptr;
    QAction *m_cancelRoiAction = nullptr;
    QAction *m_runOnceAction = nullptr;
    QAction *m_continuousAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_modeToggleAction = nullptr;
    QAction *m_manageRecipesAction = nullptr;
    RoiEditMode m_roiEditMode = RoiEditMode::None;
    AppMode m_appMode = AppMode::Edit;

    QLabel *m_activeRecipeLabel = nullptr;
    QListWidget *m_measureList = nullptr;
    QPushButton *m_addMeasureBtn = nullptr;
    QPushButton *m_removeMeasureBtn = nullptr;
    QPushButton *m_editMeasureBtn = nullptr;
    QPushButton *m_drawMeasureRoiBtn = nullptr;
    LogWidget *m_logWidget = nullptr;

    ThumbnailStrip *m_sourceThumbs = nullptr;
    CustomImageView *m_imageView = nullptr;

    QLabel *m_okNgLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_propertyPanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    bool m_updatingLists = false;   ///< 刷新列表时抑制 selectionChanged 重入
    bool m_busy = false;            ///< 单次/连续周期进行中
    bool m_continuous = false;      ///< 连续运行（文件夹序列循环）
    bool m_stopRequested = false;   ///< 用户点停止，当前周期结束后不再派发
    QString m_currentImageStatus;
    QString m_currentImagePath;
    QImage m_currentImage;
};

#endif // MAINWINDOW_H
