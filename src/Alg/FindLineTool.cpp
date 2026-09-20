#include "Alg/FindLineTool.h"

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

MeasureResult FindLineTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    if (!ctx.alignment.ok) {
        return failResult(item, QStringLiteral("定位失败，跳过找线"));
    }
    if (!qIsFinite(item.geometry.length1) || !qIsFinite(item.geometry.length2)
        || item.geometry.length1 < 1.0 || item.geometry.length2 < 1.0) {
        return failResult(item, QStringLiteral("找线卡尺尺寸无效"));
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
        if (!resolveImageCaliper(item, ctx.alignment, &rowImg, &colImg, &phiImg)) {
            return failResult(item, QStringLiteral("卡尺坐标变换失败"));
        }
        if (!qIsFinite(rowImg) || !qIsFinite(colImg) || !qIsFinite(phiImg)) {
            return failResult(item, QStringLiteral("卡尺坐标非有限值"));
        }

        MeasureHandleGuard guard;
        GenMeasureRectangle2(rowImg,
                             colImg,
                             phiImg,
                             item.geometry.length1,
                             item.geometry.length2,
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
        result.length1 = item.geometry.length1;
        result.length2 = item.geometry.length2;
        result.firstEdges = tupleToPoints(rowEdge, colEdge);

        if (result.firstEdges.size() < 2) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("边缘点不足，无法拟合直线");
            return result;
        }

        // 仅用过滤后的有限点拟合，避免脏 HTuple；regression 对少点更稳
        HTuple rows;
        HTuple cols;
        for (const Point2D &p : result.firstEdges) {
            rows.Append(p.row);
            cols.Append(p.col);
        }

        HXLDCont contour;
        contour.GenContourPolygonXld(rows, cols);

        HTuple rowBegin, colBegin, rowEnd, colEnd, nr, nc, dist;
        contour.FitLineContourXld("regression",
                                  -1,
                                  0,
                                  5,
                                  2,
                                  &rowBegin,
                                  &colBegin,
                                  &rowEnd,
                                  &colEnd,
                                  &nr,
                                  &nc,
                                  &dist);

        if (rowBegin.Length() < 1 || colBegin.Length() < 1
            || rowEnd.Length() < 1 || colEnd.Length() < 1
            || nr.Length() < 1 || nc.Length() < 1 || dist.Length() < 1) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("直线拟合失败");
            return result;
        }

        result.line.valid = true;
        result.line.rowBegin = rowBegin[0].D();
        result.line.colBegin = colBegin[0].D();
        result.line.rowEnd = rowEnd[0].D();
        result.line.colEnd = colEnd[0].D();
        result.line.nr = nr[0].D();
        result.line.nc = nc[0].D();
        result.line.dist = dist[0].D();
        if (!qIsFinite(result.line.rowBegin) || !qIsFinite(result.line.colBegin)
            || !qIsFinite(result.line.rowEnd) || !qIsFinite(result.line.colEnd)
            || !qIsFinite(result.line.nr) || !qIsFinite(result.line.nc)
            || !qIsFinite(result.line.dist)) {
            result.line = {};
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("直线拟合结果无效");
            return result;
        }

        const double dr = result.line.rowEnd - result.line.rowBegin;
        const double dc = result.line.colEnd - result.line.colBegin;
        const double lenPx = qSqrt(dr * dr + dc * dc);

        QString unit;
        result.value = applyCalibration(lenPx, ctx.calibration, &unit);
        result.unit = unit;
        result.measured = true;
        result.message = QStringLiteral("找线完成（%1 点）").arg(result.firstEdges.size());
        return result;
    } catch (HException &ex) {
        return failResult(item, fromHException(ex));
    } catch (...) {
        return failResult(item, QStringLiteral("找线异常退出"));
    }
}

} // namespace Me
