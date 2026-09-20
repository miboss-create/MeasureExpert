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

inline void affinePoint(const double hom[6], double row, double col, double *outRow, double *outCol)
{
    *outRow = hom[0] * row + hom[1] * col + hom[2];
    *outCol = hom[3] * row + hom[4] * col + hom[5];
}

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

inline bool resolveImageCaliper(const MeasureItem &item,
                                const AlignmentResult &alignment,
                                double *rowImg,
                                double *colImg,
                                double *phiImg)
{
    if (!alignment.ok || !rowImg || !colImg || !phiImg) {
        return false;
    }
    affinePoint(alignment.homMat2D,
                item.geometry.row,
                item.geometry.column,
                rowImg,
                colImg);
    *phiImg = alignment.angleRad + item.geometry.phi;
    return true;
}

} // namespace ToolCommon
} // namespace Me

#endif // TOOLCOMMON_H
