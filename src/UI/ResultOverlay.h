#ifndef RESULTOVERLAY_H
#define RESULTOVERLAY_H

#include "Alg/AlgTypes.h"

#include <QImage>
#include <QRectF>

namespace Me {

// 例程式可视化：模型轮廓 + 卡尺框 + 边缘点 + 测量连线/数值
class ResultOverlay
{
public:
    static QImage render(const QImage &source, const CycleResult &result);
    static QImage renderContours(const QImage &source,
                                 const QVector<Contour2D> &contours,
                                 const QRectF *roi = nullptr);
};

} // namespace Me

#endif // RESULTOVERLAY_H
