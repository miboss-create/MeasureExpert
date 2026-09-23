#include "Alg/FindLineTool.h"

#include "Alg/ToolCommon.h"

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

/**
 * 沿卡尺某一方向等距布细卡尺，每条 MeasurePos 只取最强边缘点。
 *
 * 为何不用单条 gen_measure_rectangle2 + measure_pos：
 *   单次剖面在一条明显边缘上通常只得到 1 个点，无法拟合直线。
 *
 * @param rowC/colC   卡尺中心
 * @param phi         该组细卡尺的朝向（搜索方向）
 * @param halfSearch  沿 phi 的半搜索长（Measure 的 length1）
 * @param halfAlong   垂直于 phi 的半跨度，用于决定采样条数与间距
 * @param threshold   边缘幅度阈值
 */
QVector<Point2D> sampleEdgesAlong(const HalconCpp::HImage &gray,
                                  Hlong width,
                                  Hlong height,
                                  double rowC,
                                  double colC,
                                  double phi,
                                  double halfSearch,
                                  double halfAlong,
                                  double threshold)
{
    QVector<Point2D> pts;
    if (halfSearch < 1.0 || halfAlong < 1.0) {
        return pts;
    }

    // 约每 6px 一条，限制在 [5, 31]
    const int samples = qBound(5, static_cast<int>((halfAlong * 2.0) / 6.0), 31);
    const double step = (2.0 * halfAlong) / static_cast<double>(samples - 1);
    // 细卡尺的 length2：略小于半步长，减少相邻剖面互相干扰
    const double subL2 = qBound(1.0, step * 0.45, 6.0);
    // 沿垂直于 phi 的方向平移中心：HALCON phi 的法向为 (cos, -sin) 在 row/col 下
    const double s = qSin(phi);
    const double c = qCos(phi);

    for (int i = 0; i < samples; ++i) {
        const double t = -halfAlong + i * step;
        const double row = rowC + t * c;
        const double col = colC - t * s;
        HTuple handle;
        try {
            GenMeasureRectangle2(row, col, phi, halfSearch, subL2,
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

        // 可选定位跟随：有模板时 ROI 相对模板，运行时仿射到当前图
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

        // 用户画的矩形：不知道长边还是短边横跨边缘，两个正交方向都采，取点数更多的一侧
        // alongPhi：phi 为搜索向，length1 为半搜索，length2 为沿线半跨度
        const QVector<Point2D> alongPhi = sampleEdgesAlong(
            gray, width, height, rowImg, colImg, phiImg,
            item.geometry.length1, item.geometry.length2, 10.0);
        // acrossPhi：旋转 90°，交换 length1/length2 角色
        const QVector<Point2D> acrossPhi = sampleEdgesAlong(
            gray, width, height, rowImg, colImg, phiImg + 1.5707963267948966,
            item.geometry.length2, item.geometry.length1, 10.0);

        MeasureResult result = baseResult(item);
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = item.geometry.length1;
        result.length2 = item.geometry.length2;
        result.firstEdges = alongPhi.size() >= acrossPhi.size() ? alongPhi : acrossPhi;

        if (result.firstEdges.size() < 2) {
            result.measured = false;
            result.ok = false;
            result.message = QStringLiteral("边缘点不足（%1），无法拟合直线")
                                 .arg(result.firstEdges.size());
            return result;
        }

        // 用过滤后的有限点建 XLD；regression 对少点/噪声更稳
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

        // HALCON 直线：nr*row + nc*col = dist（法线式）
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

        // 测量值取拟合线段两端点的欧氏长度（供显示；OK/NG 不依赖此值）
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
