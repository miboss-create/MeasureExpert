#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>

class QTextBrowser;
class QPushButton;

/**
 * 运行日志面板
 *
 * 订阅 LogSink::htmlMessageReady，以富文本显示带级别颜色的日志行。
 * “清空”仅清除 UI 显示，不删除磁盘 log 文件；持久化由 LogSink 负责。
 */
class LogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LogWidget(QWidget *parent = nullptr);

    /** 便捷入口：经 qInfo 输出，走 LogSink 统一管道 */
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
