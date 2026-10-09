#include "Alg/FindLineTool.h"

#include "Alg/ToolCommon.h"

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

/**
 * 在定向卡尺内沿线布细测条，MeasurePos 取每条最强边缘点。
 *
 * geometry：phi / length1 = 搜索方向半长；length2 = 垂直半宽（沿线跨度）。
 * 中心与 phi 已由 resolveImageCaliper 变到当前图（含定位跟随）。
 */
QVector<Point2D> sampleEdgePoints(const HalconCpp::HImage &gray,
                                  Hlong width,
                                  Hlong height,
                                  double rowC,
                                  double colC,
                                  double phi,
                                  double length1,
                                  double length2,
                                  double threshold)
{
    QVector<Point2D> pts;
    if (length1 < 1.0 || length2 < 1.0) {
        return pts;
    }

    const int samples = qBound(5, static_cast<int>((length2 * 2.0) / 6.0), 31);
    const double step = (2.0 * length2) / static_cast<double>(samples - 1);
    const double subL2 = qBound(1.0, step * 0.45, 6.0);
    // 第二主轴（沿线）：(drow, dcol) = (cos φ, -sin φ)
    const double s = qSin(phi);
    const double c = qCos(phi);

    for (int i = 0; i < samples; ++i) {
        const double t = -length2 + i * step;
        const double row = rowC + t * c;
        const double col = colC - t * s;
        HTuple handle;
        try {
            GenMeasureRectangle2(row, col, phi, length1, subL2,
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

bool fitLine(const QVector<Point2D> &pts, LineGeom *lineOut)
{
    if (!lineOut || pts.size() < 2) {
        return false;
    }
    try {
        HTuple rows;
        HTuple cols;
        for (const Point2D &p : pts) {
            rows.Append(p.row);
            cols.Append(p.col);
        }

        HXLDCont contour;
        contour.GenContourPolygonXld(rows, cols);

        HTuple rowBegin, colBegin, rowEnd, colEnd, nr, nc, dist;
        contour.FitLineContourXld("tukey",
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
            return false;
        }

        lineOut->valid = true;
        lineOut->rowBegin = rowBegin[0].D();
        lineOut->colBegin = colBegin[0].D();
        lineOut->rowEnd = rowEnd[0].D();
        lineOut->colEnd = colEnd[0].D();
        lineOut->nr = nr[0].D();
        lineOut->nc = nc[0].D();
        lineOut->dist = dist[0].D();
        return qIsFinite(lineOut->rowBegin) && qIsFinite(lineOut->colBegin)
            && qIsFinite(lineOut->rowEnd) && qIsFinite(lineOut->colEnd)
            && qIsFinite(lineOut->nr) && qIsFinite(lineOut->nc)
            && qIsFinite(lineOut->dist);
    } catch (...) {
        *lineOut = {};
        return false;
    }
}

} // namespace

MeasureResult FindLineTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
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

        // 1) 定位模板 → 当前图同位置卡尺
        double rowImg = 0.0;
        double colImg = 0.0;
        double phiImg = 0.0;
        if (!resolveImageCaliper(item, ctx.alignment, ctx.followPose, &rowImg, &colImg, &phiImg)) {
            return failResult(item,
                              ctx.followPose ? QStringLiteral("定位失败，无法跟随找线 ROI")
                                             : QStringLiteral("找线卡尺坐标无效"));
        }
        if (!qIsFinite(rowImg) || !qIsFinite(colImg) || !qIsFinite(phiImg)) {
            return failResult(item, QStringLiteral("卡尺坐标非有限值"));
        }

        // 2) 卡尺内采边缘点 → 拟合直线
        const QVector<Point2D> edgePts = sampleEdgePoints(
            gray, width, height, rowImg, colImg, phiImg,
            item.geometry.length1, item.geometry.length2, 10.0);

        MeasureResult result = baseResult(item);
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = item.geometry.length1;
        result.length2 = item.geometry.length2;
        result.firstEdges = edgePts;

        LineGeom line;
        if (!fitLine(edgePts, &line)) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("边缘点不足（%1），无法拟合直线")
                                 .arg(edgePts.size());
            return result;
        }

        result.line = line;
        const double dr = line.rowEnd - line.rowBegin;
        const double dc = line.colEnd - line.colBegin;
        const double lenPx = qSqrt(dr * dr + dc * dc);

        QString unit;
        result.value = applyCalibration(lenPx, ctx.calibration, &unit);
        result.unit = unit;
        result.measured = true;
        result.message = QStringLiteral("找线完成（%1 点）").arg(edgePts.size());
        return result;
    } catch (HException &ex) {
        return failResult(item, fromHException(ex));
    } catch (...) {
        return failResult(item, QStringLiteral("找线异常退出"));
    }
}

} // namespace Me
