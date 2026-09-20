#ifndef ALIGNMENTTOOL_H
#define ALIGNMENTTOOL_H

#include "Alg/ITool.h"

namespace Me {

class AlignmentTool : public IAlignmentTool
{
public:
    AlignmentResult run(const ToolContext &ctx, const AlignmentConfig &config) const override;
};

} // namespace Me

#endif // ALIGNMENTTOOL_H
