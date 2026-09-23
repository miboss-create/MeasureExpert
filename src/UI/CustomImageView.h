#ifndef CUSTOMIMAGEVIEW_H
#define CUSTOMIMAGEVIEW_H

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QWidget>

#include "UI/RoiRectItem.h"

class CustomGraphicsView;

/**
 * 对外图像窗口（QWidget 薄封装）
 *
 * 内部持有 CustomGraphicsView，将 ROI / 图像 / 视图工具能力以统一 API 暴露给 MainWindow。
 * 信号与槽均透传，调用方无需直接操作 QGraphicsView。
 */
class CustomImageView : public QWidget
{
    Q_OBJECT
public:
    explicit CustomImageView(QWidget *parent = nullptr);
    ~CustomImageView() override;

    bool initWidget();
    const QImage &image() const { return m_image; }

    /** 以下 ROI 接口均转发至 CustomGraphicsView */
    void setRoiEditEnabled(bool enabled);
    bool isRoiEditEnabled() const;
    bool isRoiDrawing() const;
    void setRoiStyle(const QColor &color, const QString &label);
    void setRoiShape(RoiRectItem::Shape shape);
    RoiRectItem::Shape roiShape() const;
    /** ROI 外接 AABB，图像像素坐标 (x=col, y=row) */
    QRectF roiImageRect() const;
    void setRoiImageRect(const QRectF &rect);
    void setRoiOriented(const QPointF &center, qreal phiRad, qreal length1, qreal length2);
    QPointF roiCenter() const;
    qreal roiPhi() const;
    qreal roiLength1() const;
    qreal roiLength2() const;
    void clearRoi();
    bool hasRoi() const;

public slots:
    void setImage(const QImage &image);
    void setImagePath(const QString &path);

signals:
    void roiChanged(const QRectF &imageRect);
    void roiEditEnabledChanged(bool enabled);
    void roiDrawConfirmed();
    void roiDeleted();

private:
    CustomGraphicsView *m_graphicsView = nullptr;
    QImage m_image; ///< 本地缓存，供 image() 只读访问
};

#endif // CUSTOMIMAGEVIEW_H
