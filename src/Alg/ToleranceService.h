#ifndef TOLERANCESERVICE_H
#define TOLERANCESERVICE_H

#include "Alg/AlgTypes.h"

namespace Me {

/**
 * OK/NG 判定：
 *   找线/交点 → 测到即 OK；
 *   其余 → value 落在 [nominal+lower, nominal+upper]。
 */
class ToleranceService
{
public:
    static bool judgeItem(MeasureResult &result);
    static bool judgeAll(QVector<MeasureResult> &results, QString *reason = nullptr);
};

} // namespace Me

#endif // TOLERANCESERVICE_H
