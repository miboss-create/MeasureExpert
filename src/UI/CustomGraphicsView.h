#ifndef CUSTOMGRAPHICSVIEW_H
#define CUSTOMGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QImage>
#include <QPixmap>

class QLabel;
class CustomImageItem;

// 视觉窗口核心：缩放 / 拖拽 / 双击复位 / 棋盘背景
class CustomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomGraphicsView(QWidget *parent = nullptr);
    ~CustomGraphicsView() override;

    bool initWidget();
    void setImage(const QImage &image);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void onCenter();
    void onZoom(float scaleFactor);

private:
    void fitFrame();
    void setBackground(bool enabled = true, bool invertColor = false);

    double m_zoomValue = 1.0;
    QGraphicsScene *m_scene = nullptr;
    CustomImageItem *m_imageItem = nullptr;
    QWidget *m_posInfoWidget = nullptr;
    QLabel *m_posInfoLabel = nullptr;
    QPixmap m_image;
    QImage m_qImage;
    QPixmap m_tilePixmap = QPixmap(36, 36);
};

#endif // CUSTOMGRAPHICSVIEW_H
