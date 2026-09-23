/**
 * @file CsvResultStore.cpp
 * @brief CsvResultStore 实现：RFC 4180 风格转义与按日分文件追加。
 */

#include "Core/CsvResultStore.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>

namespace Me {
namespace {

/** 含逗号/引号/换行时加双引号并转义内部引号 */
QString csvEscape(const QString &text)
{
    if (!text.contains(QLatin1Char(',')) && !text.contains(QLatin1Char('"'))
        && !text.contains(QLatin1Char('\n'))) {
        return text;
    }
    QString escaped = text;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace

QString CsvResultStore::defaultResultsDir()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/results");
}

QString CsvResultStore::defaultFilePath()
{
    const QString day = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    return defaultResultsDir() + QLatin1Char('/') + day + QStringLiteral(".csv");
}

bool CsvResultStore::append(const QString &filePath,
                            const QString &recipeName,
                            const QString &imagePath,
                            const CycleResult &result,
                            QString *error)
{
    if (filePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("CSV 路径为空");
        }
        return false;
    }

    QDir().mkpath(QFileInfo(filePath).absolutePath());

    const bool exists = QFile::exists(filePath);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法写入 CSV: %1").arg(file.errorString());
        }
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 新文件写表头
    if (!exists) {
        out << QStringLiteral(
            "timestamp,recipe,image,total_ok,elapsed_ms,reason,"
            "measure_name,value,unit,ok,message\n");
    }

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    const QString totalOk = result.totalOk ? QStringLiteral("OK") : QStringLiteral("NG");

    // 无测量项时仍写一行汇总，便于追溯定位失败等场景
    if (result.measures.isEmpty()) {
        out << csvEscape(stamp) << ','
            << csvEscape(recipeName) << ','
            << csvEscape(imagePath) << ','
            << totalOk << ','
            << result.elapsedMs << ','
            << csvEscape(result.reason) << ','
            << QStringLiteral(",,,,\n");
    } else {
        for (const MeasureResult &m : result.measures) {
            out << csvEscape(stamp) << ','
                << csvEscape(recipeName) << ','
                << csvEscape(imagePath) << ','
                << totalOk << ','
                << result.elapsedMs << ','
                << csvEscape(result.reason) << ','
                << csvEscape(m.name) << ','
                << (m.measured ? QString::number(m.value, 'f', m.decimals) : QString()) << ','
                << csvEscape(m.unit) << ','
                << (m.ok ? QStringLiteral("OK") : QStringLiteral("NG")) << ','
                << csvEscape(m.message) << '\n';
        }
    }

    file.flush();
    return true;
}

} // namespace Me
