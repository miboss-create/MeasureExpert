#ifndef ALIGNMENTSERVICE_H
#define ALIGNMENTSERVICE_H

/**
 * @file AlignmentService.h
 * @brief HALCON 形状匹配定位：训练模板与运行时 FindShapeModel。
 *
 * 被 AlignmentTool（RunEngine）与 UI 模板创建对话框共用。
 * 模型路径相对工程目录解析；训练与查找的角度范围须一致。
 */

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QImage>
#include <QString>

namespace Me {

class AlignmentService
{
public:
    /**
     * 在 modelRoi 内训练形状模型并写入 modelPath（建议绝对路径）。
     *
     * @param angleStartDeg/angleExtentDeg 须覆盖运行时可能出现的旋转（与 find 一致）
     * @param previewContours 可选：训练完成后在 ROI 中心姿态下的轮廓预览
     */
    static bool createModel(const QImage &image,
                            const SearchRegion &modelRoi,
                            const QString &modelPath,
                            double angleStartDeg = 0.0,
                            double angleExtentDeg = 360.0,
                            QString *error = nullptr,
                            QVector<Contour2D> *previewContours = nullptr);

    /**
     * 在当前图像中搜索形状模型。
     *
     * @param projectDir  工程文件所在目录，用于解析相对 modelFile
     * @param extractContours 是否在结果中填充 modelContours（UI 叠加）
     */
    static AlignmentResult find(const QImage &image,
                                const AlignmentConfig &config,
                                const QString &projectDir,
                                bool extractContours = true);
};

} // namespace Me

#endif // ALIGNMENTSERVICE_H
