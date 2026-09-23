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

/**
 * 图像视图核心（QGraphicsView 封装）
 *
 * 视图工具：
 *   - Select：默认，滚轮缩放、双击复位
 *   - Pan：ScrollHandDrag 平移画布
 *   - Zoom：按住左键显示局部放大镜（Loupe），松开恢复
 *
 * ROI 编辑（类 HALCON 交互）：
 *   1) 进入编辑 → 绘制态：左键空白拖拽绘制 ROI，画布不可平移
 *   2) 绘制态右键 → 确认绘制，进入可平移态，仍可拖/改 ROI
 *   3) 已确认态右键命中 ROI → 弹出菜单（删除 / 重新绘制）
 *
 * 坐标约定：场景坐标 = 图像像素坐标（x=col, y=row）
 */
class CustomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomGraphicsView(QWidget *parent = nullptr);
    ~CustomGraphicsView() override;

    /** 初始化场景、图像项、底部信息栏与工具按钮 */
    bool initWidget();
    /** 加载图像并重置缩放/居中；同步 ROI 边界约束 */
    void setImage(const QImage &image);

    /** 开启/关闭 ROI 编辑模式；开启时默认进入绘制态 */
    void setRoiEditEnabled(bool enabled);
    bool isRoiEditEnabled() const { return m_roiEditEnabled; }
    /** true：编辑中且尚未右键确认（画布禁平移） */
    bool isRoiDrawing() const { return m_roiEditEnabled && m_roiDrawing; }
    /** 设置 ROI 描边颜色与标签文字 */
    void setRoiStyle(const QColor &color, const QString &label);

    /** Rectangle=矩形卡尺；Circle=找圆（外接正方形约束，绘制为正圆） */
    void setRoiShape(RoiRectItem::Shape shape);
    RoiRectItem::Shape roiShape() const;

    /** 当前 ROI 外接矩形，图像像素坐标 (x=col, y=row, width, height) */
    QRectF roiImageRect() const;
    void setRoiImageRect(const QRectF &rect);
    void clearRoi();
    /** 宽高均 ≥ 4px 视为有效 ROI */
    bool hasRoi() const;

signals:
    void roiChanged(const QRectF &imageRect);
    void roiEditEnabledChanged(bool enabled);
    void roiDrawConfirmed(); ///< 绘制态右键确认，进入可平移态
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
    /** 将图像居中到视口 */
    void onCenter();
    /** 相对当前倍率缩放（滚轮/ fitFrame 均经此入口） */
    void onZoom(float scaleFactor);

private:
    /** 按窗口尺寸自适应缩放，使整图可见 */
    void fitFrame();
    /** 棋盘格背景（透明 PNG 预览常用） */
    void setBackground(bool enabled = true, bool invertColor = false);
    /** 懒创建 ROI 图元并连接 roiChanged 信号 */
    void ensureRoiItem();
    QRectF imageBounds() const;
    /** 默认 ROI：图像中心 50% 区域 */
    QRectF defaultRoiRect() const;
    /** 命中测试：含手柄外扩区（圆模式角点手柄在椭圆外） */
    bool hitRoi(const QPointF &scenePos) const;
    /** 绘制态右键确认：过小 ROI 回退默认，恢复视图工具 */
    void finishRoiDrawing();
    /** 进入绘制态：禁画布拖拽 */
    void enterRoiDrawing();
    void deleteRoiFromMenu();
    /** 根据当前 ViewTool 设置 DragMode / 光标；ROI 绘制态优先 */
    void applyViewTool();
    /** 以鼠标为中心绘制局部放大窗（视口坐标） */
    void updateLoupe(const QPoint &viewPos);
    void hideLoupe();

    /** 底部工具栏互斥选项 */
    enum class ViewTool {
        Select,
        Pan,
        Zoom
    };

    double m_zoomValue = 1.0;
    QGraphicsScene *m_scene = nullptr;
    CustomImageItem *m_imageItem = nullptr;
    RoiRectItem *m_roiItem = nullptr;
    QWidget *m_posInfoWidget = nullptr;   ///< 底部信息栏容器
    QLabel *m_posInfoLabel = nullptr;     ///< 显示 W/H、像素坐标、RGB
    QToolButton *m_selectToolBtn = nullptr;
    QToolButton *m_panToolBtn = nullptr;
    QToolButton *m_zoomToolBtn = nullptr;
    QLabel *m_loupe = nullptr;            ///< 放大镜浮层（viewport 子控件）
    ViewTool m_viewTool = ViewTool::Select;
    bool m_loupeActive = false;
    QPixmap m_image;
    QImage m_qImage;
    QPixmap m_tilePixmap = QPixmap(36, 36);

    bool m_roiEditEnabled = false;
    bool m_roiDrawing = false;   ///< true=绘制态（禁拖图）；false=已确认（可拖图+改 ROI）
    bool m_drawingRoi = false;   ///< 左键拖拽绘制新 ROI 进行中
    QPointF m_drawStartScene;
    DragMode m_savedDragMode = ScrollHandDrag;
    QColor m_roiColor = QColor(0, 220, 120);
    QString m_roiLabel = QStringLiteral("ROI");
    RoiRectItem::Shape m_roiShape = RoiRectItem::Shape::Rectangle;
};

#endif // CUSTOMGRAPHICSVIEW_H
