#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QListWidget;
class QTextEdit;
class QTableWidget;
class QSplitter;
class CustomImageView;
class LogWidget;
class ThumbnailStrip;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLoadImage();
    void onLoadFolder();
    void onThumbImageChanged(const QImage &image);
    void onThumbPathChanged(const QString &path);
    void onSourceIndexChanged(int index);
    void onResultIndexChanged(int index);

private:
    bool initWidget();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void updateImageSourceStatus(const QString &source);
    void syncThumbSelection(ThumbnailStrip *from, ThumbnailStrip *to, int index);

    QListWidget *m_recipeList = nullptr;
    QListWidget *m_measureList = nullptr;
    LogWidget *m_logWidget = nullptr;

    ThumbnailStrip *m_sourceThumbs = nullptr;
    CustomImageView *m_imageView = nullptr;
    ThumbnailStrip *m_resultThumbs = nullptr;

    QLabel *m_okNgLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_propertyPanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    bool m_syncingThumbs = false;
};

#endif // MAINWINDOW_H
