#include "Alg/RunEngine.h"

#include "Alg/AlignmentService.h"
#include "Alg/MeasureService.h"
#include "Alg/ToleranceService.h"

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

    cycle.alignment = AlignmentService::find(image, recipe.alignment, projectDir);
    cycle.alignmentOk = cycle.alignment.ok;
    if (!cycle.alignment.ok) {
        cycle.reason = cycle.alignment.message;
        cycle.totalOk = false;
        // 仍填充测量项为失败行，便于结果表展示
        for (const MeasureItem &item : recipe.measures) {
            MeasureResult r;
            r.id = item.id;
            r.name = item.name;
            r.type = item.type;
            r.nominal = item.nominal;
            r.upper = item.upper;
            r.lower = item.lower;
            r.unit = item.unit;
            r.decimals = item.decimals;
            r.measured = false;
            r.ok = false;
            r.message = QStringLiteral("定位失败");
            cycle.measures.push_back(r);
        }
        cycle.elapsedMs = static_cast<int>(timer.elapsed());
        emit cycleFinished(cycle);
        return;
    }

    cycle.measures = MeasureService::runAll(image, recipe, cycle.alignment, calibration);
    QString reason;
    cycle.totalOk = ToleranceService::judgeAll(cycle.measures, &reason);
    cycle.reason = reason;
    cycle.elapsedMs = static_cast<int>(timer.elapsed());
    emit cycleFinished(cycle);
}

} // namespace Me
