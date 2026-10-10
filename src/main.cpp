/**
 * MeasureExpert 程序入口。
 *
 * 初始化 Qt 应用、Fusion 风格、全局日志捕获（LogSink），
 * 创建并显示 MainWindow。
 */
#include "UI/MainWindow.h"
#include "UI/LogSink.h"
#include "Core/SehGuard.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MeasureExpert"));
    QApplication::setOrganizationName(QStringLiteral("MeasureExpert"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    installCrashDump();

    // 安装 qInstallMessageHandler，将 qDebug/qInfo 等写入日志文件
    LogSink::instance()->installMessageHandler();

    const QIcon appIcon(QStringLiteral(":/icons/app_icon.png"));
    QApplication::setWindowIcon(appIcon);

    const bool autoQuit = app.arguments().contains(QStringLiteral("--auto-quit"));

    int code = 0;
    {
        MainWindow window;
        window.setWindowIcon(appIcon);
        window.show();

        qInfo().noquote() << QObject::tr("MeasureExpert 已启动（主图：Qt ImageView）");
        qInfo().noquote() << QObject::tr("日志目录: %1").arg(LogSink::instance()->logDirPath());

        if (autoQuit) {
            QTimer::singleShot(800, &window, &QWidget::close);
        }

        code = app.exec();
        {
            FILE *f = nullptr;
            if (fopen_s(&f, "D:\\learning\\MeasureExpert_runtime\\log\\exit_trace.txt", "a") == 0 && f) {
                fprintf(f, "main:afterExec code=%d\n", code);
                fflush(f);
                fclose(f);
            }
        }
    } // MainWindow 在此析构，QApplication 仍存活

    {
        FILE *f = nullptr;
        if (fopen_s(&f, "D:\\learning\\MeasureExpert_runtime\\log\\exit_trace.txt", "a") == 0 && f) {
            fprintf(f, "main:afterWindowDestroyed\n");
            fflush(f);
            fclose(f);
        }
    }
    LogSink::instance()->uninstallMessageHandler();
    {
        FILE *f = nullptr;
        if (fopen_s(&f, "D:\\learning\\MeasureExpert_runtime\\log\\exit_trace.txt", "a") == 0 && f) {
            fprintf(f, "main:exit\n");
            fflush(f);
            fclose(f);
        }
    }
    return code;
}
