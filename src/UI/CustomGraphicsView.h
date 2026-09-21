#ifndef CUSTOMGRAPHICSVIEW_H
#define CUSTOMGRAPHICSVIEW_H

#include <QColor>
#include <QGraphicsView>
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <QString>

#include "UI/RoiRectItem.h"

class QLabel;
class QToolButton;
class CustomImageItem;

// 视觉窗口核心：缩放 / 拖拽 / 双击复位 / ROI 编辑
// ROI 交互（类 HALCON）：
//   1) 左键空白拖拽绘制
//   2) 右键确认绘制 → 可拖动画布，仍可拖/改 ROI
//   3) 选中 ROI 右键 → 删除
class CustomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomGraphicsView(QWidget *parent = nullptr);
    ~CustomGraphicsView() override;

    bool initWidget();
    void setImage(const QImage &image);

    void setRoiEditEnabled(bool enabled);
    bool isRoiEditEnabled() const { return m_roiEditEnabled; }
    bool isRoiDrawing() const { return m_roiEditEnabled && m_roiDrawing; }
    void setRoiStyle(const QColor &color, const QString &label);

    // Rectangle=矩形卡尺；Circle=找圆（正圆）
    void setRoiShape(RoiRectItem::Shape shape);
    RoiRectItem::Shape roiShape() const;

    // 图像像素坐标矩形 (x=col, y=row, width, height)
    QRectF roiImageRect() const;
    void setRoiImageRect(const QRectF &rect);
    void clearRoi();
    bool hasRoi() const;

signals:
    void roiChanged(const QRectF &imageRect);
    void roiEditEnabledChanged(bool enabled);
    void roiDrawConfirmed(); // 右键确认绘制结束
    void roiDeleted();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void onCenter();
    void onZoom(float scaleFactor);

private:
    void fitFrame();
    void setBackground(bool enabled = true, bool invertColor = false);
    void ensureRoiItem();
    QRectF imageBounds() const;
    QRectF defaultRoiRect() const;
    bool hitRoi(const QPointF &scenePos) const;
    void finishRoiDrawing();
    void enterRoiDrawing();
    void deleteRoiFromMenu();
    void applyViewTool();
    void updateLoupe(const QPoint &viewPos);
    void hideLoupe();

    enum class ViewTool {
        Select,
        Pan,
        Zoom
    };

    double m_zoomValue = 1.0;
    QGraphicsScene *m_scene = nullptr;
    CustomImageItem *m_imageItem = nullptr;
    RoiRectItem *m_roiItem = nullptr;
    QWidget *m_posInfoWidget = nullptr;
    QLabel *m_posInfoLabel = nullptr;
    QToolButton *m_selectToolBtn = nullptr;
    QToolButton *m_panToolBtn = nullptr;
    QToolButton *m_zoomToolBtn = nullptr;
    QLabel *m_loupe = nullptr;
    ViewTool m_viewTool = ViewTool::Select;
    bool m_loupeActive = false;
    QPixmap m_image;
    QImage m_qImage;
    QPixmap m_tilePixmap = QPixmap(36, 36);

    bool m_roiEditEnabled = false;
    bool m_roiDrawing = false;   // true=绘制中(禁拖图)；false=已确认(可拖图+改ROI)
    bool m_drawingRoi = false;   // 正在左键拖出新矩形
    QPointF m_drawStartScene;
    DragMode m_savedDragMode = ScrollHandDrag;
    QColor m_roiColor = QColor(0, 220, 120);
    QString m_roiLabel = QStringLiteral("ROI");
    RoiRectItem::Shape m_roiShape = RoiRectItem::Shape::Rectangle;
};

#endif // CUSTOMGRAPHICSVIEW_H
