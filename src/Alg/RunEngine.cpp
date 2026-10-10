#include "Alg/RunEngine.h"

#include "Alg/MeasureService.h"
#include "Alg/ToleranceService.h"
#include "Alg/ToolCommon.h"
#include "Alg/ToolRegistry.h"

#include <QElapsedTimer>

namespace Me {

RunEngine::RunEngine(QObject *parent)
    : QObject(parent)
{
}

/**
 * 单次试跑流水线（工作线程中调用）：
 *   1) 校验图像 / 配方启用
 *   2) 若配方有定位模板 → 形状匹配；否则跳过定位（ROI 按图像坐标）
 *   3) 按配方顺序跑全部测量项
 *   4) 公差判定 → 汇总 OK/NG
 *
 * 注意：定位失败时不整单中止；跟随类工具会各自报错，非跟随项仍可测。
 */
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

    // modelFile 非空即视为「启用定位跟随」
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
    if (cycle.measures.isEmpty()) {
        bool waitingRoi = false;
        for (const MeasureItem &item : recipe.measures) {
            if (ToolCommon::measureRoiMissing(item)) {
                waitingRoi = true;
                break;
            }
        }
        if (waitingRoi) {
            cycle.reason = QStringLiteral("请先为测量项画卡尺 ROI");
            cycle.totalOk = false;
            cycle.elapsedMs = static_cast<int>(timer.elapsed());
            emit cycleFinished(cycle);
            return;
        }
    }
    QString reason;
    cycle.totalOk = ToleranceService::judgeAll(cycle.measures, &reason);
    cycle.reason = reason;
    cycle.elapsedMs = static_cast<int>(timer.elapsed());
    emit cycleFinished(cycle);
}

} // namespace Me
