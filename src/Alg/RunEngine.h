#ifndef RUNENGINE_H
#define RUNENGINE_H

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QImage>
#include <QObject>
#include <QString>

namespace Me {

class RunEngine : public QObject
{
    Q_OBJECT
public:
    explicit RunEngine(QObject *parent = nullptr);

public slots:
    void runOnce(const QImage &image,
                 const Recipe &recipe,
                 const CalibrationConfig &calibration,
                 const QString &projectDir);

signals:
    void cycleFinished(const Me::CycleResult &result);
    void errorOccurred(const QString &message);
};

} // namespace Me

#endif // RUNENGINE_H
