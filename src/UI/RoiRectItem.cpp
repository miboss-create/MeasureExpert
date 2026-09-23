#include "UI/RoiRectItem.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLineF>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace {

constexpr qreal kMinHalf = 2.0;

} // namespace

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
    return axisAlignedBounds();
}

void RoiRectItem::setShape(Shape shape)
{
    if (m_shape == shape) {
        return;
    }
    prepareGeometryChange();
    m_shape = shape;
    if (m_len1 > 0.0 || m_len2 > 0.0) {
        if (m_shape == Shape::Circle) {
            const qreal side = qMax(kMinHalf, qMax(m_len1, m_len2));
            m_phi = 0.0;
            m_len1 = side;
            m_len2 = side;
        }
        clampInPlace();
        emitChanged();
    }
    update();
}

void RoiRectItem::setRoiRect(const QRectF &rect)
{
    const QRectF n = rect.normalized();
    if (n.width() < 1.0 || n.height() < 1.0) {
        prepareGeometryChange();
        m_center = {};
        m_phi = 0.0;
        m_len1 = 0.0;
        m_len2 = 0.0;
        update();
        emitChanged();
        return;
    }
    applyGeometry(n.center(), 0.0, n.width() * 0.5, n.height() * 0.5);
}

void RoiRectItem::setOrientedRoi(const QPointF &center, qreal phiRad, qreal length1, qreal length2)
{
    applyGeometry(center, phiRad, length1, length2);
}

void RoiRectItem::setImageBounds(const QRectF &bounds)
{
    m_imageBounds = bounds;
    if (m_len1 > 0.0 || m_len2 > 0.0) {
        prepareGeometryChange();
        clampInPlace();
        update();
        emitChanged();
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

void RoiRectItem::refreshHandleMetrics()
{
    prepareGeometryChange();
    update();
}

// ---------------------------------------------------------------------------
// 坐标变换 / 边界
// ---------------------------------------------------------------------------

QPointF RoiRectItem::localToScene(qreal lx, qreal ly) const
{
    const qreal c = qCos(m_phi);
    const qreal s = qSin(m_phi);
    // 与 ResultOverlay / QPainter::rotate 一致（y 向下）
    return QPointF(m_center.x() + lx * c - ly * s,
                   m_center.y() + lx * s + ly * c);
}

QPointF RoiRectItem::sceneToLocal(const QPointF &scenePos) const
{
    const qreal dx = scenePos.x() - m_center.x();
    const qreal dy = scenePos.y() - m_center.y();
    const qreal c = qCos(m_phi);
    const qreal s = qSin(m_phi);
    return QPointF(dx * c + dy * s, -dx * s + dy * c);
}

QRectF RoiRectItem::axisAlignedBounds() const
{
    if (m_len1 <= 0.0 && m_len2 <= 0.0) {
        return {};
    }
    if (m_shape == Shape::Circle) {
        const qreal r = qMax(m_len1, m_len2);
        return QRectF(m_center.x() - r, m_center.y() - r, r * 2.0, r * 2.0);
    }
    const QPointF p0 = localToScene(-m_len1, -m_len2);
    const QPointF p1 = localToScene(m_len1, -m_len2);
    const QPointF p2 = localToScene(m_len1, m_len2);
    const QPointF p3 = localToScene(-m_len1, m_len2);
    const qreal minX = qMin(qMin(p0.x(), p1.x()), qMin(p2.x(), p3.x()));
    const qreal maxX = qMax(qMax(p0.x(), p1.x()), qMax(p2.x(), p3.x()));
    const qreal minY = qMin(qMin(p0.y(), p1.y()), qMin(p2.y(), p3.y()));
    const qreal maxY = qMax(qMax(p0.y(), p1.y()), qMax(p2.y(), p3.y()));
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void RoiRectItem::applyGeometry(const QPointF &center, qreal phi, qreal len1, qreal len2)
{
    prepareGeometryChange();
    m_center = center;
    m_phi = (m_shape == Shape::Circle) ? 0.0 : phi;
    if (m_shape == Shape::Circle) {
        const qreal side = qMax(kMinHalf, qMax(qAbs(len1), qAbs(len2)));
        m_len1 = side;
        m_len2 = side;
    } else {
        m_len1 = qMax(kMinHalf, qAbs(len1));
        m_len2 = qMax(kMinHalf, qAbs(len2));
    }
    clampInPlace();
    update();
    emitChanged();
}

void RoiRectItem::clampInPlace()
{
    if (m_imageBounds.isEmpty()) {
        return;
    }
    // 先保证中心在图内，再必要时整体平移使 AABB 落入边界
    m_center.setX(qBound(m_imageBounds.left(), m_center.x(), m_imageBounds.right()));
    m_center.setY(qBound(m_imageBounds.top(), m_center.y(), m_imageBounds.bottom()));

    QRectF aabb = axisAlignedBounds();
    if (aabb.isEmpty()) {
        return;
    }
    qreal dx = 0.0;
    qreal dy = 0.0;
    if (aabb.left() < m_imageBounds.left()) {
        dx = m_imageBounds.left() - aabb.left();
    } else if (aabb.right() > m_imageBounds.right()) {
        dx = m_imageBounds.right() - aabb.right();
    }
    if (aabb.top() < m_imageBounds.top()) {
        dy = m_imageBounds.top() - aabb.top();
    } else if (aabb.bottom() > m_imageBounds.bottom()) {
        dy = m_imageBounds.bottom() - aabb.bottom();
    }
    m_center += QPointF(dx, dy);

    // 仍越界则等比缩小（旋转矩形）或缩半径（圆）
    aabb = axisAlignedBounds();
    if (aabb.width() > m_imageBounds.width() || aabb.height() > m_imageBounds.height()) {
        const qreal sx = m_imageBounds.width() / qMax(1.0, aabb.width());
        const qreal sy = m_imageBounds.height() / qMax(1.0, aabb.height());
        const qreal scale = qMin(1.0, qMin(sx, sy));
        m_len1 = qMax(kMinHalf, m_len1 * scale);
        m_len2 = qMax(kMinHalf, m_len2 * scale);
        if (m_shape == Shape::Circle) {
            m_len1 = m_len2 = qMax(m_len1, m_len2);
        }
        m_center.setX(qBound(m_imageBounds.left(), m_center.x(), m_imageBounds.right()));
        m_center.setY(qBound(m_imageBounds.top(), m_center.y(), m_imageBounds.bottom()));
    }
}

void RoiRectItem::emitChanged()
{
    emit roiChanged(axisAlignedBounds());
}

// ---------------------------------------------------------------------------
// 绘制 / 命中
// ---------------------------------------------------------------------------

QRectF RoiRectItem::boundingRect() const
{
    const QRectF aabb = axisAlignedBounds();
    if (aabb.isEmpty()) {
        return {};
    }
    const qreal pad = handleSceneSize(Rotate) + rotateStickScene() + 4.0;
    return aabb.adjusted(-pad, -pad, pad, pad);
}

QPainterPath RoiRectItem::shape() const
{
    QPainterPath path;
    if (m_len1 <= 0.0 && m_len2 <= 0.0) {
        return path;
    }
    if (m_shape == Shape::Circle) {
        path.addEllipse(axisAlignedBounds());
        return path;
    }
    QPolygonF poly;
    poly << localToScene(-m_len1, -m_len2)
         << localToScene(m_len1, -m_len2)
         << localToScene(m_len1, m_len2)
         << localToScene(-m_len1, m_len2);
    path.addPolygon(poly);
    path.closeSubpath();
    // 含旋转手柄命中区
    path.addEllipse(handleRect(Rotate));
    return path;
}

void RoiRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!m_active || (m_len1 <= 0.0 && m_len2 <= 0.0)) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = isSelected();
    QPen pen(m_color, selected ? 1.4 : 1.0);
    pen.setCosmetic(true);
    painter->setPen(pen);
    QColor fill = m_color;
    fill.setAlpha(selected ? 45 : 28);
    painter->setBrush(fill);

    if (m_shape == Shape::Circle) {
        painter->drawEllipse(axisAlignedBounds());
        const qreal arm = qMin(8.0, qMin(m_len1, m_len2) * 0.08);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(m_center.x() - arm, m_center.y()),
                          QPointF(m_center.x() + arm, m_center.y()));
        painter->drawLine(QPointF(m_center.x(), m_center.y() - arm),
                          QPointF(m_center.x(), m_center.y() + arm));
    } else {
        QPolygonF poly;
        poly << localToScene(-m_len1, -m_len2)
             << localToScene(m_len1, -m_len2)
             << localToScene(m_len1, m_len2)
             << localToScene(-m_len1, m_len2);
        painter->drawPolygon(poly);

        // 方向指示：中心 → +length1（phi 方向），类 HALCON
        QPen axisPen(m_color, 1.0, Qt::DashLine);
        axisPen.setCosmetic(true);
        painter->setPen(axisPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(m_center, localToScene(m_len1, 0.0));
    }

    const bool showHandles = m_active
        && (m_shape == Shape::Circle
            || selected || m_activeHandle != None || m_hoverHandle != None);
    if (showHandles) {
        // 旋转杆
        if (m_shape == Shape::Rectangle) {
            QPen stickPen(m_color, 1.0);
            stickPen.setCosmetic(true);
            painter->setPen(stickPen);
            painter->drawLine(localToScene(m_len1, 0.0), handleCenter(Rotate));
            painter->setBrush(QColor(255, 200, 60));
            painter->setPen(QPen(Qt::white, 0.8));
            painter->drawEllipse(handleRect(Rotate));
        }

        painter->setBrush(m_color);
        painter->setPen(QPen(Qt::white, 0.8));
        const int last = (m_shape == Shape::Circle) ? static_cast<int>(Left)
                                                    : static_cast<int>(Left);
        for (int h = TopLeft; h <= last; ++h) {
            painter->drawRect(handleRect(static_cast<Handle>(h)));
        }
    }

    painter->setPen(QPen(Qt::white, 1.0));
    const QPointF labelPos = (m_shape == Shape::Circle)
        ? (axisAlignedBounds().topLeft() + QPointF(4, -4))
        : (localToScene(-m_len1, -m_len2) + QPointF(4, -4));
    painter->drawText(labelPos, m_label);
}

QPointF RoiRectItem::handleCenter(Handle h) const
{
    if (m_shape == Shape::Circle) {
        const qreal rad = qMax(m_len1, m_len2);
        const qreal d = rad * 0.7071067811865476;
        switch (h) {
        case Top:         return QPointF(m_center.x(), m_center.y() - rad);
        case Bottom:      return QPointF(m_center.x(), m_center.y() + rad);
        case Left:        return QPointF(m_center.x() - rad, m_center.y());
        case Right:       return QPointF(m_center.x() + rad, m_center.y());
        case TopLeft:     return QPointF(m_center.x() - d, m_center.y() - d);
        case TopRight:    return QPointF(m_center.x() + d, m_center.y() - d);
        case BottomLeft:  return QPointF(m_center.x() - d, m_center.y() + d);
        case BottomRight: return QPointF(m_center.x() + d, m_center.y() + d);
        default:          return m_center;
        }
    }

    switch (h) {
    case TopLeft:     return localToScene(-m_len1, -m_len2);
    case Top:         return localToScene(0.0, -m_len2);
    case TopRight:    return localToScene(m_len1, -m_len2);
    case Right:       return localToScene(m_len1, 0.0);
    case BottomRight: return localToScene(m_len1, m_len2);
    case Bottom:      return localToScene(0.0, m_len2);
    case BottomLeft:  return localToScene(-m_len1, m_len2);
    case Left:        return localToScene(-m_len1, 0.0);
    case Rotate:      return localToScene(m_len1 + rotateStickScene(), 0.0);
    default:          return m_center;
    }
}

qreal RoiRectItem::viewScale() const
{
    if (!scene()) {
        return 1.0;
    }
    const auto views = scene()->views();
    if (views.isEmpty() || !views.first()) {
        return 1.0;
    }
    return qMax(0.05, qAbs(views.first()->transform().m11()));
}

qreal RoiRectItem::handleSceneSize(Handle h) const
{
    const qreal screenPx = (h == Rotate) ? m_rotateScreenPx : m_handleScreenPx;
    return screenPx / viewScale();
}

qreal RoiRectItem::rotateStickScene() const
{
    return m_rotateStickPx / viewScale();
}

QRectF RoiRectItem::handleRect(Handle h) const
{
    const qreal s = handleSceneSize(h);
    const QPointF c = handleCenter(h);
    return QRectF(c.x() - s * 0.5, c.y() - s * 0.5, s, s);
}

RoiRectItem::Handle RoiRectItem::hitTest(const QPointF &pos) const
{
    const qreal pad = 2.0 / viewScale();
    if (m_shape == Shape::Rectangle
        && handleRect(Rotate).adjusted(-pad, -pad, pad, pad).contains(pos)) {
        return Rotate;
    }
    for (int h = TopLeft; h <= Left; ++h) {
        if (handleRect(static_cast<Handle>(h)).adjusted(-pad, -pad, pad, pad).contains(pos)) {
            return static_cast<Handle>(h);
        }
    }
    if (shape().contains(pos)) {
        return MoveBody;
    }
    return None;
}

void RoiRectItem::updateCursor(Handle h)
{
    switch (h) {
    case Rotate:
        setCursor(Qt::CrossCursor);
        break;
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
    m_pressCenter = m_center;
    m_pressPhi = m_phi;
    m_pressLen1 = m_len1;
    m_pressLen2 = m_len2;
    const QPointF local = sceneToLocal(event->scenePos());
    m_pressLocalX = local.x();
    m_pressLocalY = local.y();
    event->accept();
}

void RoiRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    prepareGeometryChange();

    if (m_activeHandle == MoveBody) {
        const QPointF delta = event->scenePos() - m_pressScenePos;
        m_center = m_pressCenter + delta;
        clampInPlace();
        update();
        emitChanged();
        event->accept();
        return;
    }

    if (m_activeHandle == Rotate && m_shape == Shape::Rectangle) {
        const QPointF d = event->scenePos() - m_pressCenter;
        m_center = m_pressCenter;
        m_len1 = m_pressLen1;
        m_len2 = m_pressLen2;
        m_phi = qAtan2(d.y(), d.x());
        clampInPlace();
        update();
        emitChanged();
        event->accept();
        return;
    }

    if (m_shape == Shape::Circle) {
        const double rad = qMax(kMinHalf, QLineF(m_pressCenter, event->scenePos()).length());
        m_center = m_pressCenter;
        m_phi = 0.0;
        m_len1 = rad;
        m_len2 = rad;
        clampInPlace();
        update();
        emitChanged();
        event->accept();
        return;
    }

    // 矩形缩放：在按下时的局部坐标系中改尺寸，对边固定
    // 将当前鼠标投到「按下时」的局部系（固定 phi / 中心快照）
    const qreal c0 = qCos(m_pressPhi);
    const qreal s0 = qSin(m_pressPhi);
    const qreal dx = event->scenePos().x() - m_pressCenter.x();
    const qreal dy = event->scenePos().y() - m_pressCenter.y();
    const qreal lx = dx * c0 + dy * s0;
    const qreal ly = -dx * s0 + dy * c0;

    qreal left = -m_pressLen1;
    qreal right = m_pressLen1;
    qreal top = -m_pressLen2;
    qreal bottom = m_pressLen2;

    switch (m_activeHandle) {
    case TopLeft:
        left = qMin(lx, right - 2.0 * kMinHalf);
        top = qMin(ly, bottom - 2.0 * kMinHalf);
        break;
    case Top:
        top = qMin(ly, bottom - 2.0 * kMinHalf);
        break;
    case TopRight:
        right = qMax(lx, left + 2.0 * kMinHalf);
        top = qMin(ly, bottom - 2.0 * kMinHalf);
        break;
    case Right:
        right = qMax(lx, left + 2.0 * kMinHalf);
        break;
    case BottomRight:
        right = qMax(lx, left + 2.0 * kMinHalf);
        bottom = qMax(ly, top + 2.0 * kMinHalf);
        break;
    case Bottom:
        bottom = qMax(ly, top + 2.0 * kMinHalf);
        break;
    case BottomLeft:
        left = qMin(lx, right - 2.0 * kMinHalf);
        bottom = qMax(ly, top + 2.0 * kMinHalf);
        break;
    case Left:
        left = qMin(lx, right - 2.0 * kMinHalf);
        break;
    default:
        break;
    }

    const qreal newLen1 = qMax(kMinHalf, (right - left) * 0.5);
    const qreal newLen2 = qMax(kMinHalf, (bottom - top) * 0.5);
    const qreal midX = (left + right) * 0.5;
    const qreal midY = (top + bottom) * 0.5;
    // 局部中心偏移 → 场景
    m_center = QPointF(m_pressCenter.x() + midX * c0 - midY * s0,
                       m_pressCenter.y() + midX * s0 + midY * c0);
    m_phi = m_pressPhi;
    m_len1 = newLen1;
    m_len2 = newLen2;
    clampInPlace();
    update();
    emitChanged();
    event->accept();
}

void RoiRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_activeHandle = None;
    emitChanged();
    QGraphicsObject::mouseReleaseEvent(event);
}

void RoiRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hoverHandle = hitTest(event->pos());
    updateCursor(m_hoverHandle);
    update();
    QGraphicsObject::hoverMoveEvent(event);
}

void RoiRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hoverHandle = None;
    unsetCursor();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
