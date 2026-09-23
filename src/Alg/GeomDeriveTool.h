#ifndef GEOMDERIVETOOL_H
#define GEOMDERIVETOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 派生几何工具：不直接采图，只引用配方中前序测量结果（refIdA / refIdB）。
 * 前序结果由 MeasureService 写入 ToolContext::priors。
 */

/** 点到直线距离：从点源 + 找线结果提取几何后算垂距 */
class PointToLineTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::PointToLine; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

/** 两直线交点：引用两条找线；平行则失败。判定为「找到即 OK」 */
class LineIntersectionTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::LineIntersection; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

/** 两直线夹角（锐角，度）：引用两条找线 */
class AngleTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::Angle; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

/** 两点欧氏距离：点可来自圆心、交点、卡尺边缘中点等 */
class PointDistanceTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::PointDistance; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // GEOMDERIVETOOL_H
