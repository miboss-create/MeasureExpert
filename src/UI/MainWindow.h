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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLoadImage();

private:
    bool initWidget();
    void createMenus();
    void createToolBar();
    void createStatusBar();

    QListWidget *m_recipeList = nullptr;
    QListWidget *m_measureList = nullptr;
    LogWidget *m_logWidget = nullptr;

    QListWidget *m_sourceThumbList = nullptr;
    CustomImageView *m_imageView = nullptr;
    QListWidget *m_resultThumbList = nullptr;

    QLabel *m_okNgLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
    QTextEdit *m_propertyPanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
};

#endif // MAINWINDOW_H
