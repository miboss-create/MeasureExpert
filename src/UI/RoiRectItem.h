#ifndef ROIRECTITEM_H
#define ROIRECTITEM_H

#include <QColor>
#include <QGraphicsObject>
#include <QRectF>
#include <QString>

// 可移动、可拖拽边角调整的轴对齐 ROI 矩形（场景坐标 = 图像像素坐标）
class RoiRectItem : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit RoiRectItem(QGraphicsItem *parent = nullptr);

    QRectF roiRect() const; // 场景坐标下的矩形
    void setRoiRect(const QRectF &rect);

    void setImageBounds(const QRectF &bounds);
    void setActive(bool active);
    void setStyle(const QColor &color, const QString &label);

    QRectF boundingRect() const override;
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
    void normalizeRect();

    QRectF m_rect; // item 局部坐标，origin 在 (0,0)，用 setPos 不偏移，直接用场景矩形
    QRectF m_imageBounds;
    Handle m_activeHandle = None;
    Handle m_hoverHandle = None;
    QPointF m_pressScenePos;
    QRectF m_pressRect;
    bool m_active = true;
    qreal m_handleSize = 8.0;
    QColor m_color = QColor(0, 220, 120);
    QString m_label = QStringLiteral("ROI");
};

#endif // ROIRECTITEM_H
