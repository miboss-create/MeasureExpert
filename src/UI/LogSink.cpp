#include "LogSink.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace {

/**
 * Qt 全局消息回调：格式化 → 写文件 → 通知 UI。
 * 与 LogSink 单例配合，避免多处重复安装 handler。
 */
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);

    // 槽里若再打日志，避免重入死锁/半销毁 UI
    thread_local int depth = 0;
    if (depth > 0) {
        return;
    }
    struct DepthGuard {
        int *d;
        explicit DepthGuard(int *p) : d(p) { ++(*d); }
        ~DepthGuard() { --(*d); }
    } depthGuard(&depth);

    QString level;
    QString color = QStringLiteral("red");
    switch (type) {
    case QtDebugMsg:
        level = QStringLiteral("[Debug]");
        color = QStringLiteral("#a0a0a0");
        break;
    case QtInfoMsg:
        level = QStringLiteral("[Info]");
        color = QStringLiteral("#70c0ff");
        break;
    case QtWarningMsg:
        level = QStringLiteral("[Warning]");
        color = QStringLiteral("darkorange");
        break;
    case QtCriticalMsg:
        level = QStringLiteral("[Error]");
        color = QStringLiteral("#ff6060");
        break;
    case QtFatalMsg:
        level = QStringLiteral("[Fatal]");
        color = QStringLiteral("#ff3030");
        break;
    }

    LogSink *sink = LogSink::instance();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    const QString plain = QStringLiteral("[%1]%2 %3").arg(stamp, level, msg);
    const QString html = QStringLiteral("<font color=\"%1\">%2</font>")
                             .arg(color, plain.toHtmlEscaped());

    {
        static QMutex mutex;
        QMutexLocker lock(&mutex);

        QDir().mkpath(sink->logDirPath());
        QFile file(sink->logFilePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << plain << '\n';
            file.flush();
            const qint64 size = file.size();
            file.close();

            // 单文件超过约 1MB 时轮转，避免无限增大
            if (size >= 1024 * 1024) {
                for (int i = 1; i < 100; ++i) {
                    const QString rotated = sink->logDirPath()
                                            + QStringLiteral("log_%1.txt").arg(i);
                    if (!QFile::exists(rotated) || QFile(rotated).size() < 4) {
                        QFile::rename(sink->logFilePath(), rotated);
                        break;
                    }
                }
            }
        }
    }

    // 先解锁再发信号，避免关窗时排队槽重入日志持锁
    emit sink->messageReady(plain);
    emit sink->htmlMessageReady(html);
}

} // namespace

// ---------------------------------------------------------------------------
// 单例 / 消息处理器安装
// ---------------------------------------------------------------------------

LogSink *LogSink::s_instance = nullptr;

LogSink *LogSink::instance()
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if (!s_instance) {
        s_instance = new LogSink();
    }
    return s_instance;
}

LogSink::LogSink(QObject *parent)
    : QObject(parent)
{
}

LogSink::~LogSink() = default;

void LogSink::installMessageHandler()
{
    qInstallMessageHandler(messageHandler);
}

void LogSink::uninstallMessageHandler()
{
    qInstallMessageHandler(nullptr);
}

QString LogSink::logDirPath() const
{
    const QString day = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    return QCoreApplication::applicationDirPath()
           + QStringLiteral("/log/")
           + day
           + QLatin1Char('/');
}

QString LogSink::logFilePath() const
{
    return logDirPath() + QStringLiteral("log.txt");
}
