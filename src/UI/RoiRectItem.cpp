#include "UI/RoiRectItem.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>
#include <QtMath>

// ---------------------------------------------------------------------------
// 构造 / 属性
// ---------------------------------------------------------------------------

RoiRectItem::RoiRectItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setZValue(1000);
}

QRectF RoiRectItem::roiRect() const
{
    return m_rect.normalized();
}

void RoiRectItem::setShape(Shape shape)
{
    if (m_shape == shape) {
        return;
    }
    prepareGeometryChange();
    m_shape = shape;
    if (!m_rect.isEmpty()) {
        m_rect = enforceShape(clampToBounds(m_rect.normalized()));
        emit roiChanged(m_rect);
    }
    update();
}

void RoiRectItem::setRoiRect(const QRectF &rect)
{
    prepareGeometryChange();
    m_rect = enforceShape(clampToBounds(rect.normalized()));
    update();
    emit roiChanged(m_rect);
}

void RoiRectItem::setImageBounds(const QRectF &bounds)
{
    m_imageBounds = bounds;
    if (!m_rect.isEmpty()) {
        setRoiRect(m_rect);
    }
}

void RoiRectItem::setActive(bool active)
{
    m_active = active;
    setVisible(active);
    update();
}

void RoiRectItem::setStyle(const QColor &color, const QString &label)
{
    m_color = color;
    m_label = label;
    update();
}

// ---------------------------------------------------------------------------
// 绘制 / 命中区域
// ---------------------------------------------------------------------------

QRectF RoiRectItem::boundingRect() const
{
    const qreal handlePad = (m_shape == Shape::Circle) ? (m_handleSize + 6.0) : (m_handleSize + 2.0);
    return m_rect.adjusted(-handlePad, -handlePad, handlePad, handlePad);
}

QPainterPath RoiRectItem::shape() const
{
    QPainterPath path;
    if (m_shape == Shape::Circle) {
        path.addEllipse(m_rect);
    } else {
        path.addRect(m_rect);
    }
    return path;
}

void RoiRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!m_active || m_rect.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = isSelected();
    QPen pen(m_color, selected ? 2.5 : 2.0);
    pen.setCosmetic(true);
    painter->setPen(pen);
    QColor fill = m_color;
    fill.setAlpha(selected ? 50 : 35);
    painter->setBrush(fill);

    if (m_shape == Shape::Circle) {
        painter->drawEllipse(m_rect);
        // 圆心十字，便于对齐
        const QPointF c = m_rect.center();
        const qreal arm = qMin(10.0, qMin(m_rect.width(), m_rect.height()) * 0.08);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
        painter->drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
    } else {
        painter->drawRect(m_rect);
    }

    // 圆模式始终显示手柄，便于确认后缩放；矩形仍按选中/悬停显示
    const bool showHandles = m_active
        && (m_shape == Shape::Circle
            || selected || m_activeHandle != None || m_hoverHandle != None);
    if (showHandles) {
        painter->setBrush(m_color);
        painter->setPen(QPen(Qt::white, 1.0));
        for (int h = TopLeft; h <= Left; ++h) {
            painter->drawRect(handleRect(static_cast<Handle>(h)));
        }
    }

    painter->setPen(QPen(Qt::white, 1.0));
    painter->drawText(m_rect.topLeft() + QPointF(4, -4), m_label);
}

// ---------------------------------------------------------------------------
// 手柄命中 / 光标 / 边界约束
// ---------------------------------------------------------------------------

RoiRectItem::Handle RoiRectItem::hitTest(const QPointF &pos) const
{
    for (int h = TopLeft; h <= Left; ++h) {
        if (handleRect(static_cast<Handle>(h)).contains(pos)) {
            return static_cast<Handle>(h);
        }
    }
    if (shape().contains(pos)) {
        return MoveBody;
    }
    return None;
}

QRectF RoiRectItem::handleRect(Handle h) const
{
    const qreal s = (m_shape == Shape::Circle) ? (m_handleSize + 4.0) : m_handleSize;
    const QRectF r = m_rect;
    QPointF c;
    if (m_shape == Shape::Circle) {
        // 手柄放在圆周上（含 45° 对角），避免落在圆外难点中
        const QPointF center = r.center();
        const qreal rad = qMin(r.width(), r.height()) * 0.5;
        const qreal d = rad * 0.7071067811865476; // 1/√2
        switch (h) {
        case Top:         c = QPointF(center.x(), center.y() - rad); break;
        case Bottom:      c = QPointF(center.x(), center.y() + rad); break;
        case Left:        c = QPointF(center.x() - rad, center.y()); break;
        case Right:       c = QPointF(center.x() + rad, center.y()); break;
        case TopLeft:     c = QPointF(center.x() - d, center.y() - d); break;
        case TopRight:    c = QPointF(center.x() + d, center.y() - d); break;
        case BottomLeft:  c = QPointF(center.x() - d, center.y() + d); break;
        case BottomRight: c = QPointF(center.x() + d, center.y() + d); break;
        default:          return {};
        }
    } else {
        switch (h) {
        case TopLeft:     c = r.topLeft(); break;
        case Top:         c = QPointF(r.center().x(), r.top()); break;
        case TopRight:    c = r.topRight(); break;
        case Right:       c = QPointF(r.right(), r.center().y()); break;
        case BottomRight: c = r.bottomRight(); break;
        case Bottom:      c = QPointF(r.center().x(), r.bottom()); break;
        case BottomLeft:  c = r.bottomLeft(); break;
        case Left:        c = QPointF(r.left(), r.center().y()); break;
        default:          return {};
        }
    }
    return QRectF(c.x() - s / 2.0, c.y() - s / 2.0, s, s);
}

void RoiRectItem::updateCursor(Handle h)
{
    switch (h) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case MoveBody:
        setCursor(Qt::SizeAllCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

QRectF RoiRectItem::clampToBounds(QRectF rect) const
{
    rect = rect.normalized();
    if (m_imageBounds.isEmpty()) {
        return rect;
    }
    rect.setLeft(qMax(rect.left(), m_imageBounds.left()));
    rect.setTop(qMax(rect.top(), m_imageBounds.top()));
    rect.setRight(qMin(rect.right(), m_imageBounds.right()));
    rect.setBottom(qMin(rect.bottom(), m_imageBounds.bottom()));
    if (rect.width() < 4.0) {
        rect.setWidth(4.0);
    }
    if (rect.height() < 4.0) {
        rect.setHeight(4.0);
    }
    if (rect.right() > m_imageBounds.right()) {
        rect.moveRight(m_imageBounds.right());
    }
    if (rect.bottom() > m_imageBounds.bottom()) {
        rect.moveBottom(m_imageBounds.bottom());
    }
    if (rect.left() < m_imageBounds.left()) {
        rect.moveLeft(m_imageBounds.left());
    }
    if (rect.top() < m_imageBounds.top()) {
        rect.moveTop(m_imageBounds.top());
    }
    return rect;
}

QRectF RoiRectItem::enforceShape(QRectF rect) const
{
    rect = rect.normalized();
    if (m_shape != Shape::Circle) {
        return rect;
    }
    const double side = qMax(4.0, qMax(rect.width(), rect.height()));
    const QPointF c = rect.center();
    return QRectF(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
}

void RoiRectItem::normalizeRect()
{
    m_rect = enforceShape(m_rect.normalized());
}

// ---------------------------------------------------------------------------
// 鼠标交互
// ---------------------------------------------------------------------------

void RoiRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_active || event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    m_activeHandle = hitTest(event->pos());
    if (m_activeHandle == None) {
        event->ignore();
        return;
    }
    setSelected(true);
    m_pressScenePos = event->scenePos();
    m_pressRect = m_rect;
    event->accept();
}

void RoiRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    const QPointF delta = event->scenePos() - m_pressScenePos;
    QRectF r = m_pressRect;

    if (m_shape == Shape::Circle && m_activeHandle != MoveBody) {
        // 圆心固定，按鼠标到圆心距离改半径（比改边再强制正方形跟手）
        const QPointF center = m_pressRect.center();
        const double rad = qMax(4.0, QLineF(center, event->scenePos()).length());
        r = QRectF(center.x() - rad, center.y() - rad, rad * 2.0, rad * 2.0);
    } else {
        switch (m_activeHandle) {
        case MoveBody:
            r.translate(delta);
            break;
        case TopLeft:
            r.setTopLeft(m_pressRect.topLeft() + delta);
            break;
        case Top:
            r.setTop(m_pressRect.top() + delta.y());
            break;
        case TopRight:
            r.setTopRight(m_pressRect.topRight() + delta);
            break;
        case Right:
            r.setRight(m_pressRect.right() + delta.x());
            break;
        case BottomRight:
            r.setBottomRight(m_pressRect.bottomRight() + delta);
            break;
        case Bottom:
            r.setBottom(m_pressRect.bottom() + delta.y());
            break;
        case BottomLeft:
            r.setBottomLeft(m_pressRect.bottomLeft() + delta);
            break;
        case Left:
            r.setLeft(m_pressRect.left() + delta.x());
            break;
        default:
            break;
        }
    }

    prepareGeometryChange();
    m_rect = enforceShape(clampToBounds(r.normalized()));
    update();
    emit roiChanged(m_rect);
    event->accept();
}

void RoiRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_activeHandle = None;
    normalizeRect();
    emit roiChanged(m_rect);
    QGraphicsObject::mouseReleaseEvent(event);
}

void RoiRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hoverHandle = hitTest(event->pos());
    updateCursor(m_hoverHandle);
    QGraphicsObject::hoverMoveEvent(event);
}

void RoiRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hoverHandle = None;
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}
