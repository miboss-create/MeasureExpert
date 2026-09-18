#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>

class QTextBrowser;
class QPushButton;

// 运行日志面板：UI 显示 + 清空；文件由 LogSink 写入
class LogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LogWidget(QWidget *parent = nullptr);

    void appendInfo(const QString &text);
    void clearView();

private slots:
    void onClearClicked();

private:
    bool initWidget();

    QTextBrowser *m_browser = nullptr;
    QPushButton *m_clearBtn = nullptr;
};

#endif // LOGWIDGET_H
