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
    /** 单次试跑：可选定位 → 全部测量 → 公差判定；结果经 cycleFinished 回主线程 */
    void runOnce(const QImage &image,
                 const Recipe &recipe,
                 const CalibrationConfig &calibration,
                 const QString &projectDir);
    /**
     * 必须在本对象所属线程的事件循环里调用。
     * 用 delete this 同步销毁（配合 BlockingQueuedConnection），
     * 避免 quit 之后 deleteLater 来不及处理导致悬空对象。
     */
    void shutdown() { delete this; }

signals:
    void cycleFinished(const Me::CycleResult &result);
    void errorOccurred(const QString &message);
};

} // namespace Me

#endif // RUNENGINE_H
