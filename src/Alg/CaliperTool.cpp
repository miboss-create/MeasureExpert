#include "Alg/CaliperTool.h"

#include "Alg/ToolCommon.h"

using namespace HalconCpp;
using namespace Me::ToolCommon;

namespace Me {
namespace {

MeasureResult runCaliperPairs(const MeasureItem &item, const ToolContext &ctx, bool asDiameter)
{
    if (!ctx.alignment.ok) {
        return failResult(item, QStringLiteral("定位失败，跳过测量"));
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
            double sum = 0.0;
            for (int i = 0; i < intraDistance.Length(); ++i) {
                sum += intraDistance[i].D();
            }
            valuePx = sum / static_cast<double>(intraDistance.Length());
            used = 0;
        }
        result.usedPairIndex = used;

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
