#include "CustomImageItem.h"

#include <QGraphicsSceneHoverEvent>
#include <QtGlobal>

CustomImageItem::CustomImageItem(QGraphicsItem *parentItem)
    : QObject(nullptr)
    , QGraphicsPixmapItem(parentItem)
{
    setAcceptHoverEvents(true);
}

void CustomImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QPixmap pm = pixmap();
    if (pm.isNull() || pm.width() < 1 || pm.height() < 1) {
        emit RGBValue(QStringLiteral(" W:0,H:0 | X:-,Y:- | R:-,G:-,B:-"));
        return;
    }

    const QPointF pos = event->pos();
    const int x = qBound(0, static_cast<int>(pos.x()), pm.width() - 1);
    const int y = qBound(0, static_cast<int>(pos.y()), pm.height() - 1);

    const QImage img = pm.toImage();
    if (img.isNull() || x >= img.width() || y >= img.height()) {
        return;
    }

    int r = 0;
    int g = 0;
    int b = 0;
    img.pixelColor(x, y).getRgb(&r, &g, &b);

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
