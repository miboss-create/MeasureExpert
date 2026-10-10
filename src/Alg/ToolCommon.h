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

inline HalconCpp::HTuple homTuple6(const double hom[6])
{
    HalconCpp::HTuple t;
    for (int i = 0; i < 6; ++i) {
        t.Append(hom[i]);
    }
    return t;
}

/**
 * 用定位 HomMat 把卡尺从「相对模板」变到目标坐标系。
 *
 * 约定：phi 一律为 HALCON rectangle2 / GenMeasureRectangle2 角
 * （Length1 相对 +column 的数学正方向：dcol=cosφ, drow=-sinφ）。
 *
 * 中心与 Length1 端点都走 AffineTransPixel；φ 用变换后的长轴还原：
 *   phi = atan2(-drow, dcol)
 * 等价于官方写法 phi' = phi + FindShapeModel.Angle，且与轮廓 HomMat 一致。
 * 不要用 atan2(drow,dcol)（那是 Qt/y 向下角，符号相反）；
 * 也不要用 SmallestRectangle2 的 phi（与 Length1 可能差符号/互换轴）。
 */
inline bool affineCaliper(const double hom[6],
                          double rowIn,
                          double colIn,
                          double phiIn,
                          double *rowOut,
                          double *colOut,
                          double *phiOut)
{
    if (!rowOut || !colOut || !phiOut || !hom) {
        return false;
    }
    if (!qIsFinite(rowIn) || !qIsFinite(colIn) || !qIsFinite(phiIn)) {
        return false;
    }

    try {
        const HalconCpp::HTuple homT = homTuple6(hom);
        // HALCON Length1 端点：(drow, dcol) = (-sin φ, cos φ)
        const double axisRow = rowIn - qSin(phiIn);
        const double axisCol = colIn + qCos(phiIn);

        HalconCpp::HTuple r0;
        HalconCpp::HTuple c0;
        HalconCpp::HTuple r1;
        HalconCpp::HTuple c1;
        AffineTransPixel(homT, rowIn, colIn, &r0, &c0);
        AffineTransPixel(homT, axisRow, axisCol, &r1, &c1);

        *rowOut = r0[0].D();
        *colOut = c0[0].D();
        const double drow = r1[0].D() - r0[0].D();
        const double dcol = c1[0].D() - c0[0].D();
        *phiOut = qAtan2(-drow, dcol);
        return qIsFinite(*rowOut) && qIsFinite(*colOut) && qIsFinite(*phiOut);
    } catch (...) {
        return false;
    }
}

/** HALCON rectangle2 φ ↔ Qt/RoiRectItem φ（y 向下 atan2(drow,dcol)），互为相反数 */
inline double halconPhiToQt(double phiHalcon)
{
    return -phiHalcon;
}
inline double qtPhiToHalcon(double phiQt)
{
    return -phiQt;
}

/** 卡尺类测量是否需要主图画 ROI */
inline bool measureNeedsRoi(MeasureType t)
{
    return t == MeasureType::CaliperWidth || t == MeasureType::CaliperDiameter
        || t == MeasureType::FindLine || t == MeasureType::FindCircle;
}

/** 添加测量项时的占位框，或结构体默认框。确认 ROI 后几何会离开这两个值。 */
inline bool isUnsetCaliperGeometry(const CaliperGeometry &g)
{
    const bool placeholder = qAbs(g.row) < 1e-3
        && qAbs(g.column - 60.0) < 1e-3
        && qAbs(g.phi) < 1e-3
        && qAbs(g.length1 - 40.0) < 1e-3
        && qAbs(g.length2 - 8.0) < 1e-3;
    const bool defaults = qAbs(g.row) < 1e-3
        && qAbs(g.column) < 1e-3
        && qAbs(g.phi) < 1e-3
        && qAbs(g.length1 - 50.0) < 1e-3
        && qAbs(g.length2 - 20.0) < 1e-3;
    return placeholder || defaults;
}

/**
 * 未在主图确认 ROI。
 * roiDefined 为真，或几何已不是占位框，都视为已画过。
 */
inline bool measureRoiMissing(const MeasureItem &item)
{
    if (!measureNeedsRoi(item.type)) {
        return false;
    }
    if (item.roiDefined) {
        return false;
    }
    return isUnsetCaliperGeometry(item.geometry);
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
 *   true  — ROI 相对定位模板；必须 alignment.ok，用 homMat2D 仿射中心与长轴
 *   false — ROI 本身就是图像坐标（无模板或未启用跟随），直接拷贝
 *
 * 输出 *phiImg 为 HALCON rectangle2 角（与 GenMeasureRectangle2 一致）。
 * 叠图 / RoiRectItem 需用 halconPhiToQt / qtPhiToHalcon 转换。
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
        return affineCaliper(alignment.homMat2D,
                             item.geometry.row,
                             item.geometry.column,
                             item.geometry.phi,
                             rowImg,
                             colImg,
                             phiImg);
    }
    *rowImg = item.geometry.row;
    *colImg = item.geometry.column;
    *phiImg = item.geometry.phi;

    return qIsFinite(*rowImg) && qIsFinite(*colImg) && qIsFinite(*phiImg);
}

} // namespace ToolCommon
} // namespace Me

#endif // TOOLCOMMON_H
