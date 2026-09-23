#include "Alg/AlignmentTool.h"

#include "Alg/AlignmentService.h"

namespace Me {

/** 转到 AlignmentService，便于 ToolRegistry 统一调度 */
AlignmentResult AlignmentTool::run(const ToolContext &ctx, const AlignmentConfig &config) const
{
    return AlignmentService::find(ctx.image, config, ctx.projectDir);
}

} // namespace Me
