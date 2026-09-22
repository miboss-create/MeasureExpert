#include "Alg/FindCircleTool.h"

#include "Alg/ToolCommon.h"

#include <QtMath>

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

// 沿圆周布径向卡尺。gen_measure_arc 的剖面贴着圆周走，圆边与剖面平行时往往只有 0～1 个点。
QVector<Point2D> sampleRadialEdges(const HImage &gray,
                                   Hlong width,
                                   Hlong height,
                                   double rowC,
                                   double colC,
                                   double radius,
                                   double phiStart,
                                   double angleExtent,
                                   double radialHalf,
                                   double threshold)
{
    QVector<Point2D> pts;
    if (radius < 2.0 || radialHalf < 1.0 || angleExtent < 0.2) {
        return pts;
    }

    const double arcLen = qMax(1.0, radius * angleExtent);
    const int samples = qBound(12, static_cast<int>(arcLen / 8.0), 72);
    const double step = angleExtent / static_cast<double>(samples);
    const double tangentialHalf = qBound(1.0, radius * step * 0.35, 8.0);

    for (int i = 0; i < samples; ++i) {
        const double ang = phiStart + (static_cast<double>(i) + 0.5) * step;
        const double row = rowC + radius * qSin(ang);
        const double col = colC + radius * qCos(ang);
        HTuple handle;
        try {
            GenMeasureRectangle2(row, col, ang, radialHalf, tangentialHalf,
                                 width, height, "bilinear", &handle);
            HTuple rowEdge, colEdge, amplitude, distance;
            MeasurePos(gray, handle, 1.0, threshold, "all", "all",
                       &rowEdge, &colEdge, &amplitude, &distance);
            CloseMeasure(handle);
            handle = HTuple();

            int best = -1;
            double bestAmp = 0.0;
            const int n = static_cast<int>(qMin(rowEdge.Length(), amplitude.Length()));
            for (int k = 0; k < n; ++k) {
                const double amp = qAbs(amplitude[k].D());
                if (amp >= bestAmp) {
                    bestAmp = amp;
                    best = k;
                }
            }
            if (best < 0 || best >= colEdge.Length()) {
                continue;
            }
            Point2D p;
            p.row = rowEdge[best].D();
            p.col = colEdge[best].D();
            if (qIsFinite(p.row) && qIsFinite(p.col)) {
                pts.push_back(p);
            }
        } catch (HException &) {
            try {
                if (handle.Length() > 0) {
                    CloseMeasure(handle);
                }
            } catch (...) {
            }
        } catch (...) {
        }
    }
    return pts;
}

} // namespace

MeasureResult FindCircleTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    const double radius = item.geometry.length1;
    const double annulus = item.geometry.length2;
    if (!qIsFinite(radius) || !qIsFinite(annulus) || radius < 2.0 || annulus < 1.0) {
        return failResult(item, QStringLiteral("找圆半径/环宽无效"));
    }
    if (annulus >= radius) {
        return failResult(item, QStringLiteral("环宽须小于半径"));
    }

    double angleExtent = item.geometry.angleExtent;
    if (!qIsFinite(angleExtent) || angleExtent <= 0.0) {
        angleExtent = 6.283185307179586;
    }

    try {
        HImage gray = toGrayHImage(ctx.image);
        if (!gray.IsInitialized()) {
            return failResult(item, QStringLiteral("图像转换失败"));
        }

        Hlong width = 0;
        Hlong height = 0;
        gray.GetImageSize(&width, &height);

        double rowImg = 0.0;
        double colImg = 0.0;
        double phiImg = 0.0;
        if (!resolveImageCaliper(item, ctx.alignment, ctx.followPose, &rowImg, &colImg, &phiImg)) {
            return failResult(item,
                              ctx.followPose ? QStringLiteral("定位失败，无法跟随找圆 ROI")
                                             : QStringLiteral("圆心坐标无效"));
        }
        if (!qIsFinite(rowImg) || !qIsFinite(colImg) || !qIsFinite(phiImg)) {
            return failResult(item, QStringLiteral("圆心坐标非有限值"));
        }

        // 先在环带内找边；点不够时把径向搜索加宽到大半个圆，避免 ROI 略大时采空
        QVector<Point2D> edges = sampleRadialEdges(
            gray, width, height, rowImg, colImg, radius, phiImg, angleExtent, annulus, 10.0);
        if (edges.size() < 3) {
            const double wide = qMax(annulus, radius * 0.85);
            const QVector<Point2D> wideEdges = sampleRadialEdges(
                gray, width, height, rowImg, colImg, radius, phiImg, angleExtent, wide, 8.0);
            if (wideEdges.size() > edges.size()) {
                edges = wideEdges;
            }
        }

        MeasureResult result = baseResult(item);
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = radius;
        result.length2 = annulus;
        result.angleExtent = angleExtent;
        result.firstEdges = edges;

        if (result.firstEdges.size() < 3) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("边缘点不足（%1），无法拟合圆")
                                 .arg(result.firstEdges.size());
            return result;
        }

        HTuple rows;
        HTuple cols;
        for (const Point2D &p : result.firstEdges) {
            rows.Append(p.row);
            cols.Append(p.col);
        }

        HXLDCont contour;
        contour.GenContourPolygonXld(rows, cols);

        HTuple rowCenter, colCenter, radiusOut, startPhi, endPhi, pointOrder;
        contour.FitCircleContourXld("algebraic",
                                    -1,
                                    0,
                                    0,
                                    3,
                                    2,
                                    &rowCenter,
                                    &colCenter,
                                    &radiusOut,
                                    &startPhi,
                                    &endPhi,
                                    &pointOrder);

        if (rowCenter.Length() < 1 || colCenter.Length() < 1 || radiusOut.Length() < 1) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("圆拟合失败");
            return result;
        }

        const double cRow = rowCenter[0].D();
        const double cCol = colCenter[0].D();
        const double cRad = radiusOut[0].D();
        const double maxRad = qMax(2.0, qSqrt(static_cast<double>(width * width + height * height)));
        if (!qIsFinite(cRow) || !qIsFinite(cCol) || !qIsFinite(cRad)
            || cRad < 1.0 || cRad > maxRad * 2.0) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("圆拟合结果无效");
            return result;
        }

        result.circle.valid = true;
        result.circle.row = cRow;
        result.circle.col = cCol;
        result.circle.radius = cRad;
        result.hasPoint = true;
        result.point.row = cRow;
        result.point.col = cCol;

        QString unit;
        result.value = applyCalibration(cRad, ctx.calibration, &unit);
        result.unit = unit;
        result.measured = true;
        result.message = QStringLiteral("找圆完成（%1 点）").arg(result.firstEdges.size());
        return result;
    } catch (HException &ex) {
        return failResult(item, fromHException(ex));
    } catch (...) {
        return failResult(item, QStringLiteral("找圆异常退出"));
    }
}

} // namespace Me
