#ifndef TOOLREGISTRY_H
#define TOOLREGISTRY_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 测量/定位工具静态注册表。
 * MeasureType → 具体 IMeasureTool 实现；单次试跑经此分发，避免 UI 直接依赖各工具头文件。
 */
class ToolRegistry
{
public:
    static const IMeasureTool *toolFor(MeasureType type);
    static MeasureResult runMeasure(const MeasureItem &item, const ToolContext &ctx);
    static AlignmentResult runAlignment(const ToolContext &ctx, const AlignmentConfig &config);
};

} // namespace Me

#endif // TOOLREGISTRY_H
