#include "UI/RoiRectItem.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>

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

void RoiRectItem::setRoiRect(const QRectF &rect)
{
    prepareGeometryChange();
    m_rect = clampToBounds(rect.normalized());
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

QRectF RoiRectItem::boundingRect() const
{
    const qreal m = m_handleSize + 2.0;
    return m_rect.adjusted(-m, -m, m, m);
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
    painter->drawRect(m_rect);

    // 选中或悬停/拖拽时显示手柄
    if (m_active && (selected || m_activeHandle != None || m_hoverHandle != None)) {
        painter->setBrush(m_color);
        painter->setPen(QPen(Qt::white, 1.0));
        for (int h = TopLeft; h <= Left; ++h) {
            painter->drawRect(handleRect(static_cast<Handle>(h)));
        }
    }

    painter->setPen(QPen(Qt::white, 1.0));
    painter->drawText(m_rect.topLeft() + QPointF(4, -4), m_label);
}

RoiRectItem::Handle RoiRectItem::hitTest(const QPointF &pos) const
{
    for (int h = TopLeft; h <= Left; ++h) {
        if (handleRect(static_cast<Handle>(h)).contains(pos)) {
            return static_cast<Handle>(h);
        }
    }
    if (m_rect.contains(pos)) {
        return MoveBody;
    }
    return None;
}

QRectF RoiRectItem::handleRect(Handle h) const
{
    const qreal s = m_handleSize;
    const QRectF r = m_rect;
    QPointF c;
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

void RoiRectItem::normalizeRect()
{
    m_rect = m_rect.normalized();
}

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

    prepareGeometryChange();
    m_rect = clampToBounds(r.normalized());
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
