#ifndef CUSTOMIMAGEITEM_H
#define CUSTOMIMAGEITEM_H

#include <QGraphicsPixmapItem>
#include <QObject>

// 图像像素项：悬停时回报坐标与 RGB
class CustomImageItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit CustomImageItem(QObject *parent = nullptr);

    int w = 0;
    int h = 0;

signals:
    void RGBValue(const QString &info);

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
};

#endif // CUSTOMIMAGEITEM_H
