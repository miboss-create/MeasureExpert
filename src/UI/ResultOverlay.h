#ifndef RESULTOVERLAY_H
#define RESULTOVERLAY_H

#include "Alg/AlgTypes.h"

#include <QImage>
#include <QRectF>

namespace Me {

/**
 * 测量结果叠加渲染（纯静态工具，不持有状态）
 *
 * render：完整 CycleResult 可视化
 *   - 定位：模型轮廓、原点、角度指示线
 *   - 各测量项：卡尺框/找圆环带、边缘十字、拟合线/圆、配对连线
 *   - 左上角 OK/NG 总判定水印
 *
 * renderContours：轻量轮廓叠加（可选 ROI 矩形）
 *
 * 坐标：AlgTypes 中 row/col → QPainter 的 (x=col, y=row)
 */
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
