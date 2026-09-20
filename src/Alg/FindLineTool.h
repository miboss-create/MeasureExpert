#ifndef FINDLINETOOL_H
#define FINDLINETOOL_H

#include "Alg/ITool.h"

namespace Me {

class FindLineTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::FindLine; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // FINDLINETOOL_H
