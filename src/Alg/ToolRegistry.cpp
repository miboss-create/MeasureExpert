#include "Alg/ToolRegistry.h"

#include "Alg/AlignmentTool.h"
#include "Alg/CaliperTool.h"
#include "Alg/FindCircleTool.h"
#include "Alg/FindLineTool.h"
#include "Alg/GeomDeriveTool.h"
#include "Alg/ToolCommon.h"

namespace Me {
namespace {

// 无状态工具单例：run() 为 const，全程只读上下文
const CaliperWidthTool kCaliperWidth;
const CaliperDiameterTool kCaliperDiameter;
const FindLineTool kFindLine;
const FindCircleTool kFindCircle;
const PointToLineTool kPointToLine;
const LineIntersectionTool kLineIntersection;
const AngleTool kAngle;
const PointDistanceTool kPointDistance;
const AlignmentTool kAlignment;

} // namespace

const IMeasureTool *ToolRegistry::toolFor(MeasureType type)
{
    switch (type) {
    case MeasureType::CaliperWidth:
        return &kCaliperWidth;
    case MeasureType::CaliperDiameter:
        return &kCaliperDiameter;
    case MeasureType::FindLine:
        return &kFindLine;
    case MeasureType::FindCircle:
        return &kFindCircle;
    case MeasureType::PointToLine:
        return &kPointToLine;
    case MeasureType::LineIntersection:
        return &kLineIntersection;
    case MeasureType::Angle:
        return &kAngle;
    case MeasureType::PointDistance:
        return &kPointDistance;
    }
    return &kCaliperWidth;
}

MeasureResult ToolRegistry::runMeasure(const MeasureItem &item, const ToolContext &ctx)
{
    const IMeasureTool *tool = toolFor(item.type);
    if (!tool) {
        return ToolCommon::failResult(item, QStringLiteral("未知工具类型"));
    }
    try {
        return tool->run(item, ctx);
    } catch (...) {
        return ToolCommon::failResult(item, QStringLiteral("工具执行异常"));
    }
}

AlignmentResult ToolRegistry::runAlignment(const ToolContext &ctx, const AlignmentConfig &config)
{
    return kAlignment.run(ctx, config);
}

} // namespace Me
