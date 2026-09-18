#ifndef TOLERANCESERVICE_H
#define TOLERANCESERVICE_H

#include "Alg/AlgTypes.h"

namespace Me {

class ToleranceService
{
public:
    static bool judgeItem(MeasureResult &result);
    static bool judgeAll(QVector<MeasureResult> &results, QString *reason = nullptr);
};

} // namespace Me

#endif // TOLERANCESERVICE_H
