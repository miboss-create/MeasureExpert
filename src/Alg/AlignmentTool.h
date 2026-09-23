#ifndef ALIGNMENTTOOL_H
#define ALIGNMENTTOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 定位工具薄封装：试跑时若配方配置了 modelFile，由 RunEngine 调用。
 * 实际匹配逻辑在 AlignmentService::find（形状模板 find_shape_model）。
 */
class AlignmentTool : public IAlignmentTool
{
public:
    AlignmentResult run(const ToolContext &ctx, const AlignmentConfig &config) const override;
};

} // namespace Me

#endif // ALIGNMENTTOOL_H
