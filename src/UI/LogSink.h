#ifndef LOGSINK_H
#define LOGSINK_H

#include <QObject>
#include <QString>

// 全局日志：转发到 UI，并写入 exe 同级 log/yyyy-MM-dd/log.txt
class LogSink : public QObject
{
    Q_OBJECT
public:
    static LogSink *instance();

    void installMessageHandler();
    void uninstallMessageHandler();

    QString logDirPath() const;
    QString logFilePath() const;

signals:
    void messageReady(const QString &plain);
    void htmlMessageReady(const QString &html);

private:
    explicit LogSink(QObject *parent = nullptr);
    ~LogSink() override;

    static LogSink *s_instance;
};

#endif // LOGSINK_H
