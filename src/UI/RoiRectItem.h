#ifndef ROIRECTITEM_H
#define ROIRECTITEM_H

#include <QColor>
#include <QGraphicsObject>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>

/**
 * 可交互 ROI 图元（场景坐标 = 图像像素坐标，x=col / y=row）
 *
 * 形状：
 *   - Rectangle：类 HALCON rectangle2（中心 + phi + length1/2），
 *     8 向缩放手柄 + 外侧旋转手柄 + 整体平移
 *   - Circle：正圆（外接正方形约束），手柄在圆周上；无旋转
 *
 * 约束：ROI 尽量 clamp 在 setImageBounds 内，最小半边长 2px。
 */
class RoiRectItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum class Shape {
        Rectangle,
        Circle
    };

    explicit RoiRectItem(QGraphicsItem *parent = nullptr);

    /** 外接轴对齐矩形（旋转后的 AABB；Circle 为正方形） */
    QRectF roiRect() const;

    /** 轴对齐设置：圆心/矩形中心取 rect 中心，phi 置 0，半长 = 宽高/2 */
    void setRoiRect(const QRectF &rect);

    /** 定向矩形（图像坐标）：center=(col,row)，phi 弧度，len1/len2 半边长 */
    void setOrientedRoi(const QPointF &center, qreal phiRad, qreal length1, qreal length2);
    QPointF center() const { return m_center; }
    qreal phi() const { return m_phi; }
    qreal length1() const { return m_len1; }
    qreal length2() const { return m_len2; }

    void setShape(Shape shape);
    Shape shapeMode() const { return m_shape; }

    void setImageBounds(const QRectF &bounds);
    void setActive(bool active);
    void setStyle(const QColor &color, const QString &label);
    /** 视图缩放变化后刷新手柄尺寸（屏幕像素恒定） */
    void refreshHandleMetrics();

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void roiChanged(const QRectF &aabb);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    enum Handle {
        None = -1,
        MoveBody = 0,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Rotate
    };

    QPointF localToScene(qreal lx, qreal ly) const;
    QPointF sceneToLocal(const QPointF &scenePos) const;
    QRectF axisAlignedBounds() const;
    Handle hitTest(const QPointF &pos) const;
    QRectF handleRect(Handle h) const;
    QPointF handleCenter(Handle h) const;
    /** 当前视图缩放（场景→屏幕），用于手柄屏幕像素恒定大小 */
    qreal viewScale() const;
    /** 手柄在场景坐标中的边长（约恒定屏幕像素） */
    qreal handleSceneSize(Handle h) const;
    /** 旋转杆伸出长度（场景坐标） */
    qreal rotateStickScene() const;
    void updateCursor(Handle h);
    void applyGeometry(const QPointF &center, qreal phi, qreal len1, qreal len2);
    void clampInPlace();
    void emitChanged();

    Shape m_shape = Shape::Rectangle;
    QPointF m_center;
    qreal m_phi = 0.0;
    qreal m_len1 = 0.0;
    qreal m_len2 = 0.0;
    QRectF m_imageBounds;

    Handle m_activeHandle = None;
    Handle m_hoverHandle = None;
    QPointF m_pressScenePos;
    QPointF m_pressCenter;
    qreal m_pressPhi = 0.0;
    qreal m_pressLen1 = 0.0;
    qreal m_pressLen2 = 0.0;
    qreal m_pressLocalX = 0.0; ///< 按下时鼠标局部坐标，用于对边固定缩放
    qreal m_pressLocalY = 0.0;

    bool m_active = true;
    qreal m_handleScreenPx = 5.0;   ///< 缩放角点目标屏幕边长
    qreal m_rotateScreenPx = 6.0;   ///< 旋转圆点目标屏幕直径
    qreal m_rotateStickPx = 14.0;   ///< 旋转杆目标屏幕伸出长度
    QColor m_color = QColor(0, 220, 120);
    QString m_label = QStringLiteral("ROI");
};

#endif // ROIRECTITEM_H
