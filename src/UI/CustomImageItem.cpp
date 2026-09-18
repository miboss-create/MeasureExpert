#include "CustomImageItem.h"

#include <QGraphicsSceneHoverEvent>

CustomImageItem::CustomImageItem(QObject *parent)
    : QObject(parent)
    , QGraphicsPixmapItem(nullptr)
{
    setAcceptHoverEvents(true);
}

void CustomImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QPointF pos = event->pos();
    int x = qMax(0, static_cast<int>(pos.x()));
    int y = qMax(0, static_cast<int>(pos.y()));

    int r = 0;
    int g = 0;
    int b = 0;
    pixmap().toImage().pixelColor(x, y).getRgb(&r, &g, &b);

    const QString info = QStringLiteral(" W:%1,H:%2 | X:%3,Y:%4 | R:%5,G:%6,B:%7")
                             .arg(w)
                             .arg(h)
                             .arg(x)
                             .arg(y)
                             .arg(r)
                             .arg(g)
                             .arg(b);
    emit RGBValue(info);
}
