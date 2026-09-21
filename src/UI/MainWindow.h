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
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onLoadImage();
    void onLoadFolder();
    void onCalibration();
    void onTrainAlignment();
    void onCancelRoiEdit();
    void onEditMeasure();
    void onDrawMeasureRoi();
    void onRunOnce();
    void onContinuousRun();
    void onStopRun();
    void onToggleEditRunMode();
    void onCycleFinished(const Me::CycleResult &result);
    void onRunError(const QString &message);
    void onThumbImageChanged(const QImage &image);
    void onThumbPathChanged(const QString &path);
    void onRecipeSelectionChanged();
    void onMeasureSelectionChanged();
    void onMeasureDoubleClicked();
    void onAddRecipe();
    void onRemoveRecipe();
    void onManageRecipes();
    void onAddMeasure();
    void onRemoveMeasure();
    void onProjectStructureChanged();
    void onDirtyChanged(bool dirty);

private:
    enum class AppMode {
        Edit,
        Run
    };

    bool initWidget();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void setupRunEngine();
    void shutdownRunEngine();
    void updateWindowTitle();
    void updateImageSourceStatus(const QString &source);
    void updateCalibrationStatus();
    void refreshRecipeList();
    void refreshMeasureList();
    void refreshMeasureProperties();
    void applyCycleResult(const Me::CycleResult &result);
    QImage makeOverlayImage(const QImage &source, const Me::CycleResult &result) const;
    void maybeLoadImageFolderFromProject();
    bool maybeSaveBeforeContinue();
    bool saveProjectInternal(bool forceSaveAs);
    QString selectedRecipeId() const;
    QString selectedMeasureId() const;
    QString projectDir() const;
    void setBusyUi(bool busy);
    void applyAppModeUi();
    bool prepareRun(QString *failReason = nullptr) const;
    void dispatchRunOnce();
    void maybeWriteCsv(const Me::CycleResult &result);
    void enterRoiEditMode(bool forMeasure);
    void leaveRoiEditMode(bool clearRoi);
    void updateRoiActionTexts();
    QRectF measureGeometryToImageRect(const Me::CaliperGeometry &geo,
                                      const Me::AlignmentResult &align,
                                      Me::MeasureType type = Me::MeasureType::CaliperWidth) const;
    bool imageRectToMeasureGeometry(const QRectF &imageRect,
                                    const Me::AlignmentResult &align,
                                    Me::CaliperGeometry *out) const;

    enum class RoiEditMode {
        None,
        AlignTemplate,
        MeasureCaliper
    };

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
    bool m_updatingLists = false;
    bool m_busy = false;            // 单次/连续周期进行中
    bool m_continuous = false;      // 连续运行
    bool m_stopRequested = false;
    QString m_currentImageStatus;
    QString m_currentImagePath;
    QImage m_currentImage;
};

#endif // MAINWINDOW_H
