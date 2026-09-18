#ifndef MEASURESERVICE_H
#define MEASURESERVICE_H

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QImage>
#include <QVector>

namespace Me {

class MeasureService
{
public:
    static MeasureResult runOne(const QImage &image,
                                const MeasureItem &item,
                                const AlignmentResult &alignment,
                                const CalibrationConfig &calibration);

    static QVector<MeasureResult> runAll(const QImage &image,
                                         const Recipe &recipe,
                                         const AlignmentResult &alignment,
                                         const CalibrationConfig &calibration);
};

} // namespace Me

#endif // MEASURESERVICE_H
