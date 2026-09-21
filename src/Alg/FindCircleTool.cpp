#include "Alg/FindCircleTool.h"

#include "Alg/ToolCommon.h"

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

class MeasureHandleGuard
{
public:
    ~MeasureHandleGuard()
    {
        try {
            if (handle.Length() > 0) {
                CloseMeasure(handle);
            }
        } catch (...) {
        }
    }
    HTuple handle;
};

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

        MeasureHandleGuard guard;
        GenMeasureArc(rowImg,
                      colImg,
                      radius,
                      phiImg,
                      angleExtent,
                      annulus,
                      width,
                      height,
                      "bilinear",
                      &guard.handle);

        HTuple rowEdge, colEdge, amplitude, distance;
        MeasurePos(gray,
                   guard.handle,
                   1.5,
                   20,
                   "all",
                   "all",
                   &rowEdge,
                   &colEdge,
                   &amplitude,
                   &distance);
        CloseMeasure(guard.handle);
        guard.handle = HTuple();

        MeasureResult result = baseResult(item);
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = radius;
        result.length2 = annulus;
        result.angleExtent = angleExtent;
        result.firstEdges = tupleToPoints(rowEdge, colEdge);

        if (result.firstEdges.size() < 3) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("边缘点不足，无法拟合圆");
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
