#ifndef CUSTOMIMAGEVIEW_H
#define CUSTOMIMAGEVIEW_H

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QWidget>

class CustomGraphicsView;

// 对外图像窗口：显示 QImage，支持缩放拖拽与 ROI 编辑
class CustomImageView : public QWidget
{
    Q_OBJECT
public:
    explicit CustomImageView(QWidget *parent = nullptr);
    ~CustomImageView() override;

    bool initWidget();
    const QImage &image() const { return m_image; }

    void setRoiEditEnabled(bool enabled);
    bool isRoiEditEnabled() const;
    bool isRoiDrawing() const;
    void setRoiStyle(const QColor &color, const QString &label);
    QRectF roiImageRect() const; // x=col, y=row
    void setRoiImageRect(const QRectF &rect);
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
    QImage m_image;
};

#endif // CUSTOMIMAGEVIEW_H
