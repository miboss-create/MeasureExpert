#ifndef LOGSINK_H
#define LOGSINK_H

#include <QObject>
#include <QString>

/**
 * 全局日志汇聚（单例）
 *
 * 通过 qInstallMessageHandler 拦截 Qt 日志（qDebug/qInfo/qWarning 等）：
 *   - 写入 exe 同级 log/yyyy-MM-dd/log.txt（UTF-8 追加）
 *   - 单文件超约 1MB 时轮转至 log_N.txt
 *   - 同时 emit messageReady / htmlMessageReady 供 LogWidget 显示
 *
 * 线程安全：messageHandler 内加互斥锁。
 */
class LogSink : public QObject
{
    Q_OBJECT
public:
    static LogSink *instance();

    /** 安装自定义 Qt 消息处理器（main 启动时调用） */
    void installMessageHandler();
    void uninstallMessageHandler();

    /** 当日日志目录：{appDir}/log/yyyy-MM-dd/ */
    QString logDirPath() const;
    QString logFilePath() const;

signals:
    void messageReady(const QString &plain);
    void htmlMessageReady(const QString &html); ///< 带级别颜色的 HTML 片段

private:
    explicit LogSink(QObject *parent = nullptr);
    ~LogSink() override;

    static LogSink *s_instance;
};

#endif // LOGSINK_H
