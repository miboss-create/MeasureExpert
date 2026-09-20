#ifndef CSVRESULTSTORE_H
#define CSVRESULTSTORE_H

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QString>

namespace Me {

// 将一次测量周期追加写入 CSV（UTF-8）
class CsvResultStore
{
public:
    // 默认目录：exe 同级 results/
    static QString defaultResultsDir();
    // 默认文件：results/yyyy-MM-dd.csv
    static QString defaultFilePath();

    static bool append(const QString &filePath,
                       const QString &recipeName,
                       const QString &imagePath,
                       const CycleResult &result,
                       QString *error = nullptr);
};

} // namespace Me

#endif // CSVRESULTSTORE_H
