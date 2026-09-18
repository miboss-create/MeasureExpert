#ifndef CUSTOMIMAGEVIEW_H
#define CUSTOMIMAGEVIEW_H

#include <QImage>
#include <QWidget>

class CustomGraphicsView;

// 对外图像窗口（烛照方案）：显示 QImage，支持缩放拖拽
class CustomImageView : public QWidget
{
    Q_OBJECT
public:
    explicit CustomImageView(QWidget *parent = nullptr);
    ~CustomImageView() override;

    bool initWidget();
    const QImage &image() const { return m_image; }

public slots:
    void setImage(const QImage &image);
    void setImagePath(const QString &path);

private:
    CustomGraphicsView *m_graphicsView = nullptr;
    QImage m_image;
};

#endif // CUSTOMIMAGEVIEW_H
