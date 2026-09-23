#ifndef ROIRECTITEM_H
#define ROIRECTITEM_H

#include <QColor>
#include <QGraphicsObject>
#include <QPainterPath>
#include <QRectF>
#include <QString>

/**
 * 可交互 ROI 图元（场景坐标 = 图像像素坐标）
 *
 * 形状：
 *   - Rectangle：轴对齐矩形，8 向手柄缩放 + 整体平移
 *   - Circle：正圆（外接正方形约束），手柄在圆周/对角 45° 位置
 *
 * 约束：ROI 始终 clamp 在 setImageBounds 内，最小边长 4px。
 * 交互由 CustomGraphicsView 协调绘制态/确认态；本类负责命中、拖拽与 roiChanged 信号。
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

    /** 归一化后的外接矩形（Circle 模式下为正方形） */
    QRectF roiRect() const;
    void setRoiRect(const QRectF &rect);

    void setShape(Shape shape);
    Shape shapeMode() const { return m_shape; }

    /** 图像有效区域，用于 clamp ROI */
    void setImageBounds(const QRectF &bounds);
    void setActive(bool active);
    void setStyle(const QColor &color, const QString &label);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void roiChanged(const QRectF &rect);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    /** 拖拽手柄类型：MoveBody=整体平移，其余为 8 向缩放 */
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
        Left
    };

    Handle hitTest(const QPointF &pos) const;
    QRectF handleRect(Handle h) const;
    void updateCursor(Handle h);
    QRectF clampToBounds(QRectF rect) const;
    /** Circle 模式：强制外接正方形 */
    QRectF enforceShape(QRectF rect) const;
    void normalizeRect();

    Shape m_shape = Shape::Rectangle;
    QRectF m_rect;
    QRectF m_imageBounds;
    Handle m_activeHandle = None;
    Handle m_hoverHandle = None;
    QPointF m_pressScenePos;  ///< 按下时的场景坐标
    QRectF m_pressRect;       ///< 按下时的 ROI 快照，用于 delta 计算
    bool m_active = true;
    qreal m_handleSize = 8.0;
    QColor m_color = QColor(0, 220, 120);
    QString m_label = QStringLiteral("ROI");
};

#endif // ROIRECTITEM_H
