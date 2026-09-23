#ifndef CSVRESULTSTORE_H
#define CSVRESULTSTORE_H

/**
 * @file CsvResultStore.h
 * @brief 测量周期结果 CSV 持久化（UTF-8 追加写入）。
 *
 * 每次 RunEngine 完成一个 CycleResult 后，由主流程调用 append 写入一行或多行。
 * 首行写表头；同一周期内各测量项各占一行，共享 timestamp/recipe/image 等字段。
 */

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QString>

namespace Me {

class CsvResultStore
{
public:
    /** 默认目录：exe 同级 results/ */
    static QString defaultResultsDir();
    /** 默认文件：results/yyyy-MM-dd.csv（按日分文件） */
    static QString defaultFilePath();

    /**
     * 追加一次测量周期到 CSV。
     * @param filePath    目标文件（父目录不存在时自动创建）
     * @param recipeName  当前配方名
     * @param imagePath   当前图像路径
     * @param result      RunEngine 输出的 CycleResult
     */
    static bool append(const QString &filePath,
                       const QString &recipeName,
                       const QString &imagePath,
                       const CycleResult &result,
                       QString *error = nullptr);
};

} // namespace Me

#endif // CSVRESULTSTORE_H
