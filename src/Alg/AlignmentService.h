#ifndef ALIGNMENTSERVICE_H
#define ALIGNMENTSERVICE_H

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QImage>
#include <QString>

namespace Me {

class AlignmentService
{
public:
    // 在 ROI 内训练形状模型并写入 modelPath（绝对路径）
    // angleStartDeg/angleExtentDeg 必须覆盖运行时可能出现的旋转（与 find 一致）
    static bool createModel(const QImage &image,
                            const SearchRegion &modelRoi,
                            const QString &modelPath,
                            double angleStartDeg = 0.0,
                            double angleExtentDeg = 360.0,
                            QString *error = nullptr,
                            QVector<Contour2D> *previewContours = nullptr);

    static AlignmentResult find(const QImage &image,
                                const AlignmentConfig &config,
                                const QString &projectDir,
                                bool extractContours = true);
};

} // namespace Me

#endif // ALIGNMENTSERVICE_H
