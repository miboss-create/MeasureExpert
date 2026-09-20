#ifndef FINDCIRCLETOOL_H
#define FINDCIRCLETOOL_H

#include "Alg/ITool.h"

namespace Me {

class FindCircleTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::FindCircle; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // FINDCIRCLETOOL_H
