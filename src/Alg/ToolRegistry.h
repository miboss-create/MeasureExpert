#ifndef TOOLREGISTRY_H
#define TOOLREGISTRY_H

#include "Alg/ITool.h"

namespace Me {

class ToolRegistry
{
public:
    static const IMeasureTool *toolFor(MeasureType type);
    static MeasureResult runMeasure(const MeasureItem &item, const ToolContext &ctx);
    static AlignmentResult runAlignment(const ToolContext &ctx, const AlignmentConfig &config);
};

} // namespace Me

#endif // TOOLREGISTRY_H
