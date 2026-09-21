#include "Alg/RunEngine.h"

#include "Alg/MeasureService.h"
#include "Alg/ToleranceService.h"
#include "Alg/ToolRegistry.h"

#include <QElapsedTimer>

namespace Me {

RunEngine::RunEngine(QObject *parent)
    : QObject(parent)
{
}

void RunEngine::runOnce(const QImage &image,
                        const Recipe &recipe,
                        const CalibrationConfig &calibration,
                        const QString &projectDir)
{
    CycleResult cycle;
    QElapsedTimer timer;
    timer.start();

    if (image.isNull()) {
        cycle.reason = QStringLiteral("当前无图像");
        cycle.totalOk = false;
        cycle.elapsedMs = static_cast<int>(timer.elapsed());
        emit errorOccurred(cycle.reason);
        emit cycleFinished(cycle);
        return;
    }

    if (!recipe.enabled) {
        cycle.reason = QStringLiteral("当前配方未启用");
        cycle.totalOk = false;
        cycle.elapsedMs = static_cast<int>(timer.elapsed());
        emit errorOccurred(cycle.reason);
        emit cycleFinished(cycle);
        return;
    }

    ToolContext ctx;
    ctx.image = image;
    ctx.calibration = calibration;
    ctx.projectDir = projectDir;

    const bool followPose = !recipe.alignment.modelFile.isEmpty();
    if (followPose) {
        cycle.alignment = ToolRegistry::runAlignment(ctx, recipe.alignment);
        cycle.alignmentOk = cycle.alignment.ok;
    } else {
        cycle.alignment = {};
        cycle.alignment.message = QStringLiteral("未启用定位跟随");
        cycle.alignmentOk = false;
    }

    cycle.measures = MeasureService::runAll(image, recipe, cycle.alignment, calibration);
    QString reason;
    cycle.totalOk = ToleranceService::judgeAll(cycle.measures, &reason);
    cycle.reason = reason;
    cycle.elapsedMs = static_cast<int>(timer.elapsed());
    emit cycleFinished(cycle);
}

} // namespace Me
