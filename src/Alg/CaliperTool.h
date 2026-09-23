#ifndef CALIPERTOOL_H
#define CALIPERTOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 卡尺宽度：矩形卡尺 + MeasurePairs，取一对边缘的距离。
 * 多对边缘时优先选最接近名义值的一对。
 */
class CaliperWidthTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::CaliperWidth; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

/**
 * 卡尺直径：与宽度共用 MeasurePairs；
 * 多对边缘且未设名义时，对所有对距取平均（近似直径）。
 */
class CaliperDiameterTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::CaliperDiameter; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // CALIPERTOOL_H
