#include "Alg/AlignmentTool.h"

#include "Alg/AlignmentService.h"

namespace Me {

AlignmentResult AlignmentTool::run(const ToolContext &ctx, const AlignmentConfig &config) const
{
    return AlignmentService::find(ctx.image, config, ctx.projectDir);
}

} // namespace Me
