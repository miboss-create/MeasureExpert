#include "Alg/MeasureService.h"

#include "Alg/ToolRegistry.h"

namespace Me {

MeasureResult MeasureService::runOne(const QImage &image,
                                     const MeasureItem &item,
                                     const AlignmentResult &alignment,
                                     const CalibrationConfig &calibration)
{
    ToolContext ctx;
    ctx.image = image;
    ctx.alignment = alignment;
    ctx.calibration = calibration;
    return ToolRegistry::runMeasure(item, ctx);
}

QVector<MeasureResult> MeasureService::runAll(const QImage &image,
                                              const Recipe &recipe,
                                              const AlignmentResult &alignment,
                                              const CalibrationConfig &calibration)
{
    ToolContext ctx;
    ctx.image = image;
    ctx.alignment = alignment;
    ctx.calibration = calibration;

    QVector<MeasureResult> out;
    out.reserve(recipe.measures.size());
    for (const MeasureItem &item : recipe.measures) {
        MeasureResult r = ToolRegistry::runMeasure(item, ctx);
        out.push_back(r);
        ctx.priors.insert(item.id, r);
    }
    return out;
}

} // namespace Me
