#include "UI/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MeasureExpert"));
    QApplication::setOrganizationName(QStringLiteral("MeasureExpert"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QIcon appIcon(QStringLiteral(":/icons/app_icon.png"));
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();
    return app.exec();
}
