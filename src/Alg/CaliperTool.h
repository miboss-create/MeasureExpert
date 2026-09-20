#ifndef CALIPERTOOL_H
#define CALIPERTOOL_H

#include "Alg/ITool.h"

namespace Me {

class CaliperWidthTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::CaliperWidth; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

class CaliperDiameterTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::CaliperDiameter; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // CALIPERTOOL_H
