#include "Alg/MeasureService.h"

#include "Alg/ToolRegistry.h"

namespace Me {

/** 单测一项：不启用定位跟随（followPose=false），供调试/单工具调用 */
MeasureResult MeasureService::runOne(const QImage &image,
                                     const MeasureItem &item,
                                     const AlignmentResult &alignment,
                                     const CalibrationConfig &calibration)
{
    ToolContext ctx;
    ctx.image = image;
    ctx.alignment = alignment;
    ctx.calibration = calibration;
    ctx.followPose = false;
    return ToolRegistry::runMeasure(item, ctx);
}

/**
 * 按配方顺序跑全部测量项。
 * 每项完成后写入 ctx.priors，供点线距/交点/夹角等派生工具引用。
 * followPose 与配方是否配置了定位模板一致。
 */
QVector<MeasureResult> MeasureService::runAll(const QImage &image,
                                              const Recipe &recipe,
                                              const AlignmentResult &alignment,
                                              const CalibrationConfig &calibration)
{
    ToolContext ctx;
    ctx.image = image;
    ctx.alignment = alignment;
    ctx.calibration = calibration;
    ctx.followPose = !recipe.alignment.modelFile.isEmpty();

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
