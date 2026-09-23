#ifndef CUSTOMIMAGEITEM_H
#define CUSTOMIMAGEITEM_H

#include <QGraphicsPixmapItem>
#include <QObject>

/**
 * 场景中的图像图元（QGraphicsPixmapItem + 悬停探针）
 *
 * 鼠标悬停时读取当前像素 RGB，通过 RGBValue 信号更新底部信息栏。
 * 坐标系：图元局部坐标 = 图像像素坐标（左上角为原点）。
 *
 * 注意：QObject 不设 parent（由 QGraphicsScene 管理图元生命周期）。
 */
class CustomImageItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit CustomImageItem(QGraphicsItem *parentItem = nullptr);

    int w = 0; ///< 图像宽（像素），供信息栏显示
    int h = 0; ///< 图像高（像素）

signals:
    /** 格式化字符串，如 " W:640,H:480 | X:120,Y:80 | R:200,G:150,B:100" */
    void RGBValue(const QString &info);

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
};

#endif // CUSTOMIMAGEITEM_H
