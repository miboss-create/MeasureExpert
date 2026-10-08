#include "Alg/ToleranceService.h"

#include <QStringList>

namespace Me {
namespace {

/** 中间几何：找到即合格，不与名义公差比较 */
bool isGeometryPassThrough(MeasureType type)
{
    return type == MeasureType::FindLine
        || type == MeasureType::FindCircle
        || type == MeasureType::LineIntersection;
}

} // namespace

/**
 * 单项判定：
 *   - 未测到 → NG
 *   - 找线/找圆/交点 → 测到即 OK
 *   - 其余（宽度/直径/派生距离角等）→
 *       value ∈ [nominal+lower, nominal+upper]
 */
bool ToleranceService::judgeItem(MeasureResult &result)
{
    if (!result.measured) {
        result.ok = false;
        if (result.message.isEmpty()) {
            result.message = QStringLiteral("无测量值");
        }
        return false;
    }

    if (isGeometryPassThrough(result.type)) {
        result.ok = true;
        return true;
    }

    const double lo = result.nominal + result.lower;
    const double hi = result.nominal + result.upper;
    result.ok = (result.value >= lo && result.value <= hi);
    if (!result.ok) {
        result.message = QStringLiteral("超差");
    }
    return result.ok;
}

/** 全部合格才 totalOk；reason 列出不合格项名称 */
bool ToleranceService::judgeAll(QVector<MeasureResult> &results, QString *reason)
{
    if (results.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("无测量项");
        }
        return false;
    }

    bool allOk = true;
    QStringList fails;
    for (MeasureResult &r : results) {
        if (!judgeItem(r)) {
            allOk = false;
            fails << r.name;
        }
    }

    if (reason) {
        *reason = allOk ? QStringLiteral("全部合格")
                        : QStringLiteral("不合格: %1").arg(fails.join(QStringLiteral(", ")));
    }
    return allOk;
}

} // namespace Me
