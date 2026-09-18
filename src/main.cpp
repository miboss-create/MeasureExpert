#include "UI/MainWindow.h"
#include "UI/LogSink.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MeasureExpert"));
    QApplication::setOrganizationName(QStringLiteral("MeasureExpert"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    LogSink::instance()->installMessageHandler();

    const QIcon appIcon(QStringLiteral(":/icons/app_icon.png"));
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();

    qInfo().noquote() << QObject::tr("MeasureExpert 已启动（主图：Qt ImageView）");
    qInfo().noquote() << QObject::tr("日志目录: %1").arg(LogSink::instance()->logDirPath());

    return app.exec();
}
