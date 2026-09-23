#ifndef TOOLCOMMON_H
#define TOOLCOMMON_H

#include "Alg/AlgTypes.h"
#include "Alg/HalconUtils.h"
#include "Core/ProjectTypes.h"
#include "ImageConvert.h"

#include <QImage>
#include <QtMath>

#include "HalconCpp.h"

namespace Me {
namespace ToolCommon {

/** 用 2D 仿射矩阵（行优先 6 元组）变换点：out = H * (row, col, 1) */
inline void affinePoint(const double hom[6], double row, double col, double *outRow, double *outCol)
{
    *outRow = hom[0] * row + hom[1] * col + hom[2];
    *outCol = hom[3] * row + hom[4] * col + hom[5];
}

/** QImage → 单通道 HImage（彩色则 Rgb1ToGray） */
inline HalconCpp::HImage toGrayHImage(QImage image)
{
    if (image.isNull()) {
        return {};
    }
    if (image.format() != QImage::Format_Grayscale8
        && image.format() != QImage::Format_Indexed8) {
        image = image.convertToFormat(QImage::Format_RGB888);
    }
    HalconCpp::HImage color;
    if (!QImage2HImage(image, color)) {
        return {};
    }
    if (color.CountChannels().I() == 1) {
        return color;
    }
    return color.Rgb1ToGray();
}

/**
 * 像素值 → 显示/判定用工程单位。
 * 已标定：value * pixelSizeMm，单位默认 mm；否则保持 px。
 */
inline double applyCalibration(double valuePx, const CalibrationConfig &cal, QString *unit)
{
    if (cal.calibrated && cal.pixelSizeMm > 0.0) {
        if (unit) {
            *unit = cal.unit.isEmpty() ? QStringLiteral("mm") : cal.unit;
        }
        return valuePx * cal.pixelSizeMm;
    }
    if (unit) {
        *unit = QStringLiteral("px");
    }
    return valuePx;
}

/** 从配方项复制 id/名称/公差等到结果骨架 */
inline MeasureResult baseResult(const MeasureItem &item)
{
    MeasureResult r;
    r.id = item.id;
    r.name = item.name;
    r.type = item.type;
    r.nominal = item.nominal;
    r.upper = item.upper;
    r.lower = item.lower;
    r.unit = item.unit;
    r.decimals = item.decimals;
    return r;
}

inline MeasureResult failResult(const MeasureItem &item, const QString &message)
{
    MeasureResult r = baseResult(item);
    r.measured = false;
    r.ok = false;
    r.message = message;
    return r;
}

/** HALCON 行列 Tuple → Point2D 列表（跳过非有限值） */
inline QVector<Point2D> tupleToPoints(const HalconCpp::HTuple &rows, const HalconCpp::HTuple &cols)
{
    QVector<Point2D> pts;
    const Hlong n = qMin(rows.Length(), cols.Length());
    if (n <= 0 || n > 100000) {
        return pts;
    }
    pts.reserve(static_cast<int>(n));
    for (Hlong i = 0; i < n; ++i) {
        Point2D p;
        p.row = rows[i].D();
        p.col = cols[i].D();
        if (!qIsFinite(p.row) || !qIsFinite(p.col)) {
            continue;
        }
        pts.push_back(p);
    }
    return pts;
}

/**
 * 把配方里存的卡尺几何换算到「当前图像坐标」。
 *
 * @param followPose
 *   true  — ROI 相对定位模板；必须 alignment.ok，用 homMat2D 仿射中心，
 *           phiImg = 定位角 + 相对角
 *   false — ROI 本身就是图像坐标（无模板或未启用跟随），直接拷贝
 */
inline bool resolveImageCaliper(const MeasureItem &item,
                                const AlignmentResult &alignment,
                                bool followPose,
                                double *rowImg,
                                double *colImg,
                                double *phiImg)
{
    if (!rowImg || !colImg || !phiImg) {
        return false;
    }

    if (followPose) {
        if (!alignment.ok) {
            return false;
        }
        affinePoint(alignment.homMat2D,
                    item.geometry.row,
                    item.geometry.column,
                    rowImg,
                    colImg);
        *phiImg = alignment.angleRad + item.geometry.phi;
    } else {
        *rowImg = item.geometry.row;
        *colImg = item.geometry.column;
        *phiImg = item.geometry.phi;
    }

    return qIsFinite(*rowImg) && qIsFinite(*colImg) && qIsFinite(*phiImg);
}

} // namespace ToolCommon
} // namespace Me

#endif // TOOLCOMMON_H
