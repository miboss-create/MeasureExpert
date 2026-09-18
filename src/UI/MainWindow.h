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
    void onCycleFinished(const Me::CycleResult &result);
    void onRunError(const QString &message);
    void onThumbImageChanged(const QImage &image);
    void onThumbPathChanged(const QString &path);
    void onSourceIndexChanged(int index);
    void onResultIndexChanged(int index);
    void onRecipeSelectionChanged();
    void onMeasureSelectionChanged();
    void onMeasureDoubleClicked();
    void onAddRecipe();
    void onRemoveRecipe();
    void onAddMeasure();
    void onRemoveMeasure();
    void onProjectStructureChanged();
    void onDirtyChanged(bool dirty);

private:
    bool initWidget();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void setupRunEngine();
    void updateWindowTitle();
    void updateImageSourceStatus(const QString &source);
    void updateCalibrationStatus();
    void syncThumbSelection(ThumbnailStrip *from, ThumbnailStrip *to, int index);
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
    void setRunningUi(bool running);
    void enterRoiEditMode(bool forMeasure);
    void leaveRoiEditMode(bool clearRoi);
    void updateRoiActionTexts();
    QRectF measureGeometryToImageRect(const Me::CaliperGeometry &geo,
                                      const Me::AlignmentResult &align) const;
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
    RoiEditMode m_roiEditMode = RoiEditMode::None;

    QListWidget *m_recipeList = nullptr;
    QListWidget *m_measureList = nullptr;
    QPushButton *m_addRecipeBtn = nullptr;
    QPushButton *m_removeRecipeBtn = nullptr;
    QPushButton *m_addMeasureBtn = nullptr;
    QPushButton *m_removeMeasureBtn = nullptr;
    QPushButton *m_editMeasureBtn = nullptr;
    QPushButton *m_drawMeasureRoiBtn = nullptr;
    LogWidget *m_logWidget = nullptr;

    ThumbnailStrip *m_sourceThumbs = nullptr;
    CustomImageView *m_imageView = nullptr;
    ThumbnailStrip *m_resultThumbs = nullptr;

    QLabel *m_okNgLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_propertyPanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    bool m_syncingThumbs = false;
    bool m_updatingLists = false;
    bool m_running = false;
    QString m_currentImageStatus;
    QImage m_currentImage;
};

#endif // MAINWINDOW_H
