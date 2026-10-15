#include "Alg/FindCircleTool.h"

#include "Alg/ToolCommon.h"

#include <QtMath>

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

/**
 * 沿圆周等角布设径向卡尺，每条只保留幅度最强的一个边缘点。
 *
 * 为何不用 gen_measure_arc + 单次 measure_pos：
 *   圆弧卡尺的灰度剖面贴着圆周切向走，圆边与剖面近乎平行时，
 *   整条剖面往往只能得到 0～1 个跃变点，无法拟合圆。
 *
 * 本函数做法（类似 HALCON 多卡尺找圆）：
 *   在半径 radius 的圆周上按 angleExtent 均匀取 samples 个位置，
 *   每个位置放一条「径向」细卡尺（phi=径向角），在 ±radialHalf 内找边。
 *
 * @param rowC/colC     圆心（图像坐标）
 * @param radius        环带中心半径
 * @param phiStart      起始角（弧度，HALCON 约定：0 指向列正方向）
 * @param angleExtent   展角（弧度，正为逆时针）
 * @param radialHalf    径向半搜索长度（≈环宽 length2）
 * @param sigma         MeasurePos 平滑
 * @param threshold     MeasurePos 边缘幅度阈值
 * @param samplePitchPx 沿弧长的采样步长（像素）
 */
QVector<Point2D> sampleRadialEdges(const HImage &gray,
                                   Hlong width,
                                   Hlong height,
                                   double rowC,
                                   double colC,
                                   double radius,
                                   double phiStart,
                                   double angleExtent,
                                   double radialHalf,
                                   double sigma,
                                   double threshold,
                                   double samplePitchPx)
{
    QVector<Point2D> pts;
    if (radius < 2.0 || radialHalf < 1.0 || angleExtent < 0.2) {
        return pts;
    }

    const double pitch = qBound(2.0, samplePitchPx, 40.0);
    const double sig = qBound(0.4, sigma, 8.0);
    const double thr = qBound(1.0, threshold, 255.0);
    // 弧长越长采样越密；上限 96 平衡速度与拟合稳定性
    const double arcLen = qMax(1.0, radius * angleExtent);
    const int samples = qBound(12, static_cast<int>(arcLen / pitch), 96);
    const double step = angleExtent / static_cast<double>(samples);
    // 切向半宽：略小于角步长对应的弧长，避免相邻卡尺严重重叠
    const double tangentialHalf = qBound(1.0, radius * step * 0.35, 8.0);

    for (int i = 0; i < samples; ++i) {
        // 取扇区中点角，避免端点重复/遗漏
        const double ang = phiStart + (static_cast<double>(i) + 0.5) * step;
        // HALCON 极坐标：row = r0 + R*sin(a), col = c0 + R*cos(a)
        const double row = rowC + radius * qSin(ang);
        const double col = colC + radius * qCos(ang);
        if (row < -radialHalf || col < -radialHalf
            || row > static_cast<double>(height) + radialHalf
            || col > static_cast<double>(width) + radialHalf) {
            continue;
        }
        HTuple handle;
        try {
            // length1=径向搜索，length2=切向半宽；phi=ang 使搜索沿半径方向
            GenMeasureRectangle2(row, col, ang, radialHalf, tangentialHalf,
                                 width, height, "bilinear", &handle);
            HTuple rowEdge, colEdge, amplitude, distance;
            MeasurePos(gray, handle, sig, thr, "all", "all",
                       &rowEdge, &colEdge, &amplitude, &distance);
            CloseMeasure(handle);
            handle = HTuple();

            // 同一剖面可能有多个跃变，只取 |amplitude| 最大者作为该方位的圆边
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
    // --- 几何参数校验：length1=半径，length2=环宽，且环宽 < 半径 ---
    const double radius = item.geometry.length1;
    const double annulus = item.geometry.length2;
    if (!qIsFinite(radius) || !qIsFinite(annulus) || radius < 2.0 || annulus < 1.0) {
        return failResult(item, QStringLiteral("找圆半径/环宽无效"));
    }
    if (annulus >= radius) {
        return failResult(item, QStringLiteral("环宽须小于半径"));
    }

    // angleExtent≤0 时按整圆处理
    double angleExtent = item.geometry.angleExtent;
    if (!qIsFinite(angleExtent) || angleExtent <= 0.0) {
        angleExtent = 6.283185307179586; // 2π
    }

    try {
        HImage gray = toGrayHImage(ctx.image);
        if (!gray.IsInitialized()) {
            return failResult(item, QStringLiteral("图像转换失败"));
        }

        Hlong width = 0;
        Hlong height = 0;
        gray.GetImageSize(&width, &height);

        // followPose=true：ROI 相对模板，需用定位 hom 仿射到当前图
        // followPose=false：ROI 已是图像坐标，直接使用
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

        // 第一次：在用户设定的环宽内找边
        QVector<Point2D> edges = sampleRadialEdges(
            gray, width, height, rowImg, colImg, radius, phiImg, angleExtent, annulus,
            item.edgeSigma, item.edgeThreshold, item.samplePitchPx);
        // 第二次兜底：环宽偏小或 ROI 略偏时，放宽径向搜索到约 0.85R，阈值略降
        if (edges.size() < 3) {
            const double wide = qMax(annulus, radius * 0.85);
            const double thrLoose = qMax(1.0, item.edgeThreshold * 0.8);
            const QVector<Point2D> wideEdges = sampleRadialEdges(
                gray, width, height, rowImg, colImg, radius, phiImg, angleExtent, wide,
                item.edgeSigma, thrLoose, item.samplePitchPx);
            if (wideEdges.size() > edges.size()) {
                edges = wideEdges;
            }
        }

        MeasureResult result = baseResult(item);
        // 叠图仍画用户 ROI（名义半径/环宽），不是拟合后的圆
        result.imageRow = rowImg;
        result.imageCol = colImg;
        result.imagePhi = phiImg;
        result.length1 = radius;
        result.length2 = annulus;
        result.angleExtent = angleExtent;
        result.firstEdges = edges;

        // 代数圆拟合至少需要 3 点
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

        // algebraic：对噪声点较稳；后续参数为 HALCON 默认推荐值
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
        // 拒绝明显离谱的半径（超过对角线量级）
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
        // 圆心同时作为可被派生工具引用的“点”
        result.hasPoint = true;
        result.point.row = cRow;
        result.point.col = cCol;

        // 测量值 = 拟合半径；已标定则换算为 mm
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
