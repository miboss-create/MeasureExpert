#include "Alg/MeasureService.h"

#include "Alg/HalconUtils.h"
#include "ImageConvert.h"

#include <QtMath>

#include "HalconCpp.h"

using namespace HalconCpp;

namespace Me {
namespace {

void affinePoint(const double hom[6], double row, double col, double *outRow, double *outCol)
{
    *outRow = hom[0] * row + hom[1] * col + hom[2];
    *outCol = hom[3] * row + hom[4] * col + hom[5];
}

HImage toGrayHImage(QImage image)
{
    if (image.isNull()) {
        return {};
    }
    if (image.format() != QImage::Format_Grayscale8
        && image.format() != QImage::Format_Indexed8) {
        image = image.convertToFormat(QImage::Format_RGB888);
    }
    HImage color;
    if (!QImage2HImage(image, color)) {
        return {};
    }
    if (color.CountChannels().I() == 1) {
        return color;
    }
    return color.Rgb1ToGray();
}

double applyCalibration(double valuePx, const CalibrationConfig &cal, QString *unit)
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

MeasureResult failResult(const MeasureItem &item, const QString &message)
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
    r.measured = false;
    r.ok = false;
    r.message = message;
    return r;
}

QVector<Point2D> tupleToPoints(const HTuple &rows, const HTuple &cols)
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

} // namespace

MeasureResult MeasureService::runOne(const QImage &image,
                                     const MeasureItem &item,
                                     const AlignmentResult &alignment,
                                     const CalibrationConfig &calibration)
{
    if (!alignment.ok) {
        return failResult(item, QStringLiteral("定位失败，跳过测量"));
    }

    if (item.type != MeasureType::CaliperWidth && item.type != MeasureType::CaliperDiameter) {
        return failResult(item, QStringLiteral("该测量类型尚未实现"));
    }

    try {
        HImage gray = toGrayHImage(image);
        if (!gray.IsInitialized()) {
            return failResult(item, QStringLiteral("图像转换失败"));
        }

        Hlong width = 0;
        Hlong height = 0;
        gray.GetImageSize(&width, &height);

        double rowImg = 0.0;
        double colImg = 0.0;
        affinePoint(alignment.homMat2D,
                    item.geometry.row,
                    item.geometry.column,
                    &rowImg,
                    &colImg);
        const double phiImg = alignment.angleRad + item.geometry.phi;

        HTuple measureHandle;
        GenMeasureRectangle2(rowImg,
                             colImg,
                             phiImg,
                             item.geometry.length1,
                             item.geometry.length2,
                             width,
                             height,
                             "bilinear",
                             &measureHandle);

        HTuple rowEdgeFirst, colEdgeFirst, ampFirst;
        HTuple rowEdgeSecond, colEdgeSecond, ampSecond;
        HTuple intraDistance, interDistance;

        MeasurePairs(gray,
                     measureHandle,
                     1.5,
                     20,
                     "all",
                     "all",
                     &rowEdgeFirst,
                     &colEdgeFirst,
                     &ampFirst,
                     &rowEdgeSecond,
                     &colEdgeSecond,
                     &ampSecond,
                     &intraDistance,
                     &interDistance);
        CloseMeasure(measureHandle);

        MeasureResult result;
        result.id = item.id;
        result.name = item.name;
        result.type = item.type;
        result.nominal = item.nominal;
        result.upper = item.upper;
        result.lower = item.lower;
        result.decimals = item.decimals;
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = item.geometry.length1;
        result.length2 = item.geometry.length2;
        result.firstEdges = tupleToPoints(rowEdgeFirst, colEdgeFirst);
        result.secondEdges = tupleToPoints(rowEdgeSecond, colEdgeSecond);

        if (intraDistance.Length() < 1) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("未检测到边缘对");
            result.unit = item.unit;
            return result;
        }

        int used = 0;
        double valuePx = intraDistance[0].D();
        if (intraDistance.Length() > 1 && item.nominal != 0.0) {
            double bestDiff = 1e300;
            const double nominalPx = (calibration.calibrated && calibration.pixelSizeMm > 0.0)
                ? (item.nominal / calibration.pixelSizeMm)
                : item.nominal;
            for (int i = 0; i < intraDistance.Length(); ++i) {
                const double v = intraDistance[i].D();
                const double d = qAbs(v - nominalPx);
                if (d < bestDiff) {
                    bestDiff = d;
                    valuePx = v;
                    used = i;
                }
            }
        }
        result.usedPairIndex = used;

        QString unit;
        result.value = applyCalibration(valuePx, calibration, &unit);
        result.unit = unit;
        result.measured = true;
        result.message = QStringLiteral("测量完成");
        return result;
    } catch (HException &ex) {
        return failResult(item, fromHException(ex));
    }
}

QVector<MeasureResult> MeasureService::runAll(const QImage &image,
                                              const Recipe &recipe,
                                              const AlignmentResult &alignment,
                                              const CalibrationConfig &calibration)
{
    QVector<MeasureResult> out;
    out.reserve(recipe.measures.size());
    for (const MeasureItem &item : recipe.measures) {
        out.push_back(runOne(image, item, alignment, calibration));
    }
    return out;
}

} // namespace Me
