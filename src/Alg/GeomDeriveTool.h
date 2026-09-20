#ifndef GEOMDERIVETOOL_H
#define GEOMDERIVETOOL_H

#include "Alg/ITool.h"

namespace Me {

class PointToLineTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::PointToLine; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

class LineIntersectionTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::LineIntersection; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

class AngleTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::Angle; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

class PointDistanceTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::PointDistance; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // GEOMDERIVETOOL_H
