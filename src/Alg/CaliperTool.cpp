#include "Alg/CaliperTool.h"

#include "Alg/ToolCommon.h"

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

/**
 * 卡尺宽度/直径共用实现。
 *
 * 流程：
 *   1. resolveImageCaliper → 当前图上的卡尺中心/朝向
 *   2. GenMeasureRectangle2 + MeasurePairs → 两组边缘及对内距离
 *   3. 选一对距离作为测量值：
 *        - 有名义值：选最接近名义（已标定则先把名义换成像素）的一对
 *        - 直径模式且无名义：多对距离取平均
 *        - 否则：用第一对
 *
 * @param asDiameter  true=直径策略；false=宽度策略
 */
MeasureResult runCaliperPairs(const MeasureItem &item, const ToolContext &ctx, bool asDiameter)
{
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
                              ctx.followPose ? QStringLiteral("定位失败，无法跟随卡尺 ROI")
                                             : QStringLiteral("卡尺坐标无效"));
        }

        // length1：沿 phi 半长（搜索方向）；length2：垂直半宽
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

        const double sigma = qBound(0.4, item.edgeSigma, 8.0);
        const double threshold = qBound(1.0, item.edgeThreshold, 255.0);

        // MeasurePairs：在同一剖面上找成对边缘；intraDistance[i] = 第 i 对的间距（像素）
        MeasurePairs(gray,
                     measureHandle,
                     sigma,
                     threshold,
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

        MeasureResult result = baseResult(item);
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
            return result;
        }

        int used = 0;
        double valuePx = intraDistance[0].D();
        if (intraDistance.Length() > 1 && item.nominal != 0.0) {
            // 多对边时按名义值挑最接近的一对，减少量到错边
            double bestDiff = 1e300;
            const double nominalPx = (ctx.calibration.calibrated && ctx.calibration.pixelSizeMm > 0.0)
                ? (item.nominal / ctx.calibration.pixelSizeMm)
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
        } else if (asDiameter && intraDistance.Length() > 1) {
            // 直径：无名义时对所有对距平均
            double sum = 0.0;
            for (int i = 0; i < intraDistance.Length(); ++i) {
                sum += intraDistance[i].D();
            }
            valuePx = sum / static_cast<double>(intraDistance.Length());
            used = 0;
        }
        result.usedPairIndex = used; // 叠图高亮该对边缘

        QString unit;
        result.value = applyCalibration(valuePx, ctx.calibration, &unit);
        result.unit = unit;
        result.measured = true;
        result.message = asDiameter ? QStringLiteral("直径测量完成")
                                    : QStringLiteral("宽度测量完成");
        return result;
    } catch (HException &ex) {
        return failResult(item, fromHException(ex));
    }
}

} // namespace

MeasureResult CaliperWidthTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    return runCaliperPairs(item, ctx, false);
}

MeasureResult CaliperDiameterTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    return runCaliperPairs(item, ctx, true);
}

} // namespace Me
