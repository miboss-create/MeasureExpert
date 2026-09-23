#include "CustomGraphicsView.h"

#include "CustomImageItem.h"
#include "RoiRectItem.h"

#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QToolButton>
#include <QWheelEvent>

namespace {
constexpr double kZoomMax = 50.0;
constexpr double kZoomMin = 0.1;
/// 无图像时的默认场景留白（支持负坐标平移）
constexpr qreal kDefaultScenePad = 2000.0;

/** 底部工具栏：选择工具图标（矢量绘制，不依赖资源文件） */
QIcon makePointerIcon()
{
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPolygonF poly;
    poly << QPointF(3, 2) << QPointF(3, 16) << QPointF(7.2, 12.2)
         << QPointF(10.5, 17.5) << QPointF(13.2, 16.2) << QPointF(9.6, 11)
         << QPointF(15, 11);
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(Qt::white);
    p.drawPolygon(poly);
    return QIcon(pm);
}

/** 平移工具图标 */
QIcon makeHandIcon()
{
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::black, 1.2));
    p.setBrush(QColor(245, 245, 245));
    p.drawRoundedRect(QRectF(6, 8, 9, 8), 2, 2);
    p.drawRoundedRect(QRectF(4, 6, 3.2, 7), 1.2, 1.2);
    p.drawRoundedRect(QRectF(7.2, 3, 3.2, 8), 1.2, 1.2);
    p.drawRoundedRect(QRectF(10.6, 4, 3.2, 7), 1.2, 1.2);
    p.drawRoundedRect(QRectF(13.8, 6, 3.2, 6), 1.2, 1.2);
    return QIcon(pm);
}

/** 放大镜工具图标 */
QIcon makeZoomIcon()
{
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::white, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(3, 3, 10, 10));
    p.drawLine(QPointF(11.5, 11.5), QPointF(17, 17));
    return QIcon(pm);
}

} // namespace

// ---------------------------------------------------------------------------
// 构造 / 初始化
// ---------------------------------------------------------------------------

CustomGraphicsView::CustomGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setBackground();
    initWidget();
}

CustomGraphicsView::~CustomGraphicsView()
{
    m_loupeActive = false;
    hideLoupe();
    // m_loupe / m_scene 均为子对象，交由 Qt 父子树释放，避免与 viewport/scene 析构顺序冲突
    m_loupe = nullptr;
    m_scene = nullptr;
}

bool CustomGraphicsView::initWidget()
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-kDefaultScenePad, -kDefaultScenePad,
                          kDefaultScenePad * 2.0, kDefaultScenePad * 2.0);
    setScene(m_scene);

    m_imageItem = new CustomImageItem();
    m_scene->addItem(m_imageItem);

    m_posInfoWidget = new QWidget(this);
    m_posInfoLabel = new QLabel(m_posInfoWidget);
    m_posInfoWidget->setObjectName(QStringLiteral("posBar"));
    m_posInfoWidget->setFixedHeight(28);
    m_posInfoWidget->setStyleSheet(QStringLiteral(
        "QWidget#posBar { background-color: rgba(30,30,30,200); }"
        "QLabel { color: rgb(200,255,200); background: transparent;"
        " font: Microsoft YaHei; font-size: 14px; }"
        "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 2px; }"
        "QToolButton:checked { background: rgba(30,110,190,230); border: 1px solid #8ec8ff; }"
        "QToolButton:hover { background: rgba(255,255,255,30); }"));
    m_posInfoLabel->setText(QStringLiteral(" W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0"));

    auto *infoLayout = new QHBoxLayout(m_posInfoWidget);
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(6, 1, 4, 1);
    infoLayout->addWidget(m_posInfoLabel, 1);

    auto makeBtn = [this](const QIcon &icon, const QString &tip) {
        auto *btn = new QToolButton(m_posInfoWidget);
        btn->setCheckable(true);
        btn->setIcon(icon);
        btn->setIconSize(QSize(16, 16));
        btn->setFixedSize(24, 22);
        btn->setToolTip(tip);
        btn->setAutoRaise(true);
        return btn;
    };
    m_selectToolBtn = makeBtn(makePointerIcon(), tr("选择"));
    m_zoomToolBtn = makeBtn(makeZoomIcon(), tr("放大镜：按住左键看局部放大，松开恢复"));
    m_panToolBtn = makeBtn(makeHandIcon(), tr("平移"));
    infoLayout->addWidget(m_selectToolBtn);
    infoLayout->addWidget(m_panToolBtn);
    infoLayout->addWidget(m_zoomToolBtn);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(m_selectToolBtn, static_cast<int>(ViewTool::Select));
    group->addButton(m_panToolBtn, static_cast<int>(ViewTool::Pan));
    group->addButton(m_zoomToolBtn, static_cast<int>(ViewTool::Zoom));
    m_selectToolBtn->setChecked(true);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        m_viewTool = static_cast<ViewTool>(id);
        applyViewTool();
    });
    m_posInfoWidget->raise();
    connect(m_imageItem, &CustomImageItem::RGBValue, this, [this](const QString &info) {
        m_posInfoLabel->setText(info);
    });
    applyViewTool();
    return true;
}

// ---------------------------------------------------------------------------
// ROI 辅助
// ---------------------------------------------------------------------------

QRectF CustomGraphicsView::imageBounds() const
{
    if (m_image.isNull()) {
        return {};
    }
    return QRectF(0, 0, m_image.width(), m_image.height());
}

QRectF CustomGraphicsView::defaultRoiRect() const
{
    const QRectF b = imageBounds();
    if (b.isEmpty()) {
        return {};
    }
    return QRectF(b.width() * 0.25, b.height() * 0.25, b.width() * 0.5, b.height() * 0.5);
}

void CustomGraphicsView::ensureRoiItem()
{
    if (m_roiItem) {
        m_roiItem->setStyle(m_roiColor, m_roiLabel);
        m_roiItem->setShape(m_roiShape);
        return;
    }
    m_roiItem = new RoiRectItem();
    m_roiItem->setStyle(m_roiColor, m_roiLabel);
    m_roiItem->setShape(m_roiShape);
    m_scene->addItem(m_roiItem);
    connect(m_roiItem, &RoiRectItem::roiChanged, this, [this](const QRectF &r) {
        emit roiChanged(r);
    });
}

void CustomGraphicsView::setRoiStyle(const QColor &color, const QString &label)
{
    m_roiColor = color;
    m_roiLabel = label;
    if (m_roiItem) {
        m_roiItem->setStyle(color, label);
    }
}

void CustomGraphicsView::setRoiShape(RoiRectItem::Shape shape)
{
    m_roiShape = shape;
    ensureRoiItem();
    m_roiItem->setShape(shape);
}

RoiRectItem::Shape CustomGraphicsView::roiShape() const
{
    return m_roiItem ? m_roiItem->shapeMode() : m_roiShape;
}

bool CustomGraphicsView::hitRoi(const QPointF &scenePos) const
{
    if (!m_roiItem || !m_roiItem->isVisible() || m_roiItem->roiRect().isEmpty()) {
        return false;
    }
    // 必须含手柄外扩区：圆的 shape() 是椭圆，方框角点手柄在圆外，否则确认后拖手柄会被当成拖图
    return m_roiItem->sceneBoundingRect().contains(scenePos);
}

void CustomGraphicsView::enterRoiDrawing()
{
    m_roiDrawing = true;
    m_drawingRoi = false;
    setDragMode(QGraphicsView::NoDrag);
    unsetCursor();
}

void CustomGraphicsView::finishRoiDrawing()
{
    if (!m_roiEditEnabled) {
        return;
    }
    m_drawingRoi = false;
    if (m_roiItem && (m_roiItem->roiRect().width() < 4.0 || m_roiItem->roiRect().height() < 4.0)) {
        m_roiItem->setRoiRect(defaultRoiRect());
    }
    if (m_roiDrawing) {
        m_roiDrawing = false;
        setDragMode(QGraphicsView::NoDrag);
        applyViewTool();
        emit roiDrawConfirmed();
    }
}

void CustomGraphicsView::deleteRoiFromMenu()
{
    clearRoi();
    emit roiDeleted();
    if (m_roiEditEnabled) {
        // 删除后回到绘制态，便于重新画
        enterRoiDrawing();
    }
}

// ---------------------------------------------------------------------------
// 图像加载
// ---------------------------------------------------------------------------

void CustomGraphicsView::setImage(const QImage &image)
{
    if (image.isNull() || !m_imageItem) {
        return;
    }

    m_qImage = image.copy();
    m_image = QPixmap::fromImage(m_qImage);
    m_imageItem->w = m_image.width();
    m_imageItem->h = m_image.height();
    m_imageItem->setPixmap(m_image);

    const qreal pad = qMax(500.0, qMax(m_image.width(), m_image.height()) * 0.5);
    m_scene->setSceneRect(-pad, -pad,
                          m_image.width() + pad * 2.0,
                          m_image.height() + pad * 2.0);

    if (m_roiItem) {
        m_roiItem->setImageBounds(imageBounds());
        if (m_roiEditEnabled) {
            const QRectF cur = m_roiItem->roiRect();
            if (cur.isEmpty() || !imageBounds().contains(cur.center())) {
                m_roiItem->setRoiRect(defaultRoiRect());
            } else {
                m_roiItem->setRoiRect(cur);
            }
        }
    }

    m_zoomValue = 1.0;
    resetTransform();
    fitFrame();
    onCenter();
}

void CustomGraphicsView::setRoiEditEnabled(bool enabled)
{
    if (m_roiEditEnabled == enabled) {
        return;
    }
    m_roiEditEnabled = enabled;
    m_drawingRoi = false;

    if (enabled) {
        m_savedDragMode = dragMode();
        ensureRoiItem();
        m_roiItem->setImageBounds(imageBounds());
        m_roiItem->setActive(true);
        if (m_roiItem->roiRect().isEmpty()) {
            m_roiItem->setRoiRect(defaultRoiRect());
        }
        m_roiItem->setVisible(true);
        m_roiItem->setSelected(true);
        // 进入编辑后先处于“绘制态”：左键空白可重画，右键确认后才能拖图
        enterRoiDrawing();
    } else {
        m_roiDrawing = false;
        applyViewTool();
        if (m_roiItem) {
            m_roiItem->setActive(false);
            m_roiItem->setSelected(false);
        }
    }
    emit roiEditEnabledChanged(enabled);
}

QRectF CustomGraphicsView::roiImageRect() const
{
    if (!m_roiItem) {
        return {};
    }
    return m_roiItem->roiRect();
}

void CustomGraphicsView::setRoiImageRect(const QRectF &rect)
{
    ensureRoiItem();
    m_roiItem->setImageBounds(imageBounds());
    m_roiItem->setRoiRect(rect);
    m_roiItem->setActive(m_roiEditEnabled);
    if (m_roiEditEnabled && hasRoi()) {
        m_roiItem->setVisible(true);
        m_roiItem->setSelected(true);
    }
}

void CustomGraphicsView::clearRoi()
{
    if (m_roiItem) {
        m_roiItem->setRoiRect({});
        m_roiItem->setVisible(false);
        m_roiItem->setSelected(false);
    }
}

bool CustomGraphicsView::hasRoi() const
{
    return m_roiItem && m_roiItem->roiRect().isValid() && m_roiItem->roiRect().width() >= 4.0
           && m_roiItem->roiRect().height() >= 4.0;
}

// ---------------------------------------------------------------------------
// 鼠标 / 滚轮 / 菜单事件
// ---------------------------------------------------------------------------

void CustomGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) {
        return;
    }
    const QPoint scrollAmount = event->angleDelta();
    if (scrollAmount.y() > 0 && m_zoomValue >= kZoomMax) {
        return;
    }
    if (scrollAmount.y() < 0 && m_zoomValue <= kZoomMin) {
        return;
    }
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scrollAmount.y() > 0 ? onZoom(1.1f) : onZoom(0.9f);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
}

void CustomGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull()) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    const QPoint viewPos = event->position().toPoint();
    const QPointF scenePos = mapToScene(viewPos);
    // 事件优先级：ROI 命中 > 绘制态左键 > 放大镜 > 视图工具默认
    const bool onRoi = m_roiEditEnabled && hitRoi(scenePos);

    if (m_roiEditEnabled && event->button() == Qt::LeftButton && onRoi) {
        setDragMode(QGraphicsView::NoDrag);
        if (m_roiItem) {
            m_roiItem->setSelected(true);
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (m_roiEditEnabled && m_roiDrawing && event->button() == Qt::LeftButton) {
        m_drawingRoi = true;
        m_drawStartScene = scenePos;
        ensureRoiItem();
        m_roiItem->setImageBounds(imageBounds());
        m_roiItem->setActive(true);
        m_roiItem->setVisible(true);
        m_roiItem->setSelected(true);
        m_roiItem->setRoiRect(QRectF(scenePos, QSizeF(1, 1)));
        event->accept();
        return;
    }

    if (!m_roiDrawing && m_viewTool == ViewTool::Zoom && event->button() == Qt::LeftButton) {
        m_loupeActive = true;
        viewport()->setCursor(Qt::BlankCursor);
        updateLoupe(viewPos);
        viewport()->grabMouse();
        event->accept();
        return;
    }

    if (m_roiItem && event->button() == Qt::LeftButton) {
        m_roiItem->setSelected(false);
    }
    applyViewTool();
    QGraphicsView::mousePressEvent(event);
}

void CustomGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // ROI 编辑 / 放大镜模式下不响应双击复位，避免误触
    if (event->button() == Qt::LeftButton && !m_image.isNull() && !m_roiEditEnabled
        && m_viewTool != ViewTool::Zoom) {
        fitFrame();
        onCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CustomGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_loupeActive) {
        updateLoupe(event->position().toPoint());
        event->accept();
        return;
    }
    if (m_drawingRoi && m_roiItem) {
        const QPointF scenePos = mapToScene(event->pos());
        QRectF r;
        if (m_roiShape == RoiRectItem::Shape::Circle) {
            // 从起点拖出正方形，保证正圆
            const QPointF d = scenePos - m_drawStartScene;
            const double side = qMax(4.0, qMax(qAbs(d.x()), qAbs(d.y())));
            const double sx = (d.x() >= 0.0) ? 1.0 : -1.0;
            const double sy = (d.y() >= 0.0) ? 1.0 : -1.0;
            r = QRectF(m_drawStartScene, m_drawStartScene + QPointF(sx * side, sy * side)).normalized();
        } else {
            r = QRectF(m_drawStartScene, scenePos).normalized();
        }
        m_roiItem->setRoiRect(r);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CustomGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_loupeActive && event->button() == Qt::LeftButton) {
        m_loupeActive = false;
        hideLoupe();
        viewport()->releaseMouse();
        applyViewTool();
        event->accept();
        return;
    }

    if (m_drawingRoi && event->button() == Qt::LeftButton) {
        m_drawingRoi = false;
        if (m_roiItem && (m_roiItem->roiRect().width() < 4.0 || m_roiItem->roiRect().height() < 4.0)) {
            m_roiItem->setRoiRect(defaultRoiRect());
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        applyViewTool();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CustomGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_roiEditEnabled) {
        event->ignore();
        return;
    }

    // 绘制态右键 = 确认（类 HALCON），不弹菜单
    if (m_roiDrawing) {
        finishRoiDrawing();
        event->accept();
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    if (!hitRoi(scenePos) || !hasRoi()) {
        event->ignore();
        return;
    }

    if (m_roiItem) {
        m_roiItem->setSelected(true);
    }

    QMenu menu(this);
    QAction *del = menu.addAction(tr("删除 ROI"));
    QAction *redraw = menu.addAction(tr("重新绘制 ROI"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == del) {
        deleteRoiFromMenu();
    } else if (chosen == redraw) {
        clearRoi();
        enterRoiDrawing();
        ensureRoiItem();
        m_roiItem->setImageBounds(imageBounds());
        m_roiItem->setActive(true);
        m_roiItem->setVisible(true);
        m_roiItem->setRoiRect(defaultRoiRect());
        m_roiItem->setSelected(true);
        emit roiDeleted();
    }
    event->accept();
}

// ---------------------------------------------------------------------------
// 绘制 / 布局
// ---------------------------------------------------------------------------

void CustomGraphicsView::paintEvent(QPaintEvent *event)
{
    QPainter paint(viewport());
    if (!m_tilePixmap.isNull()) {
        paint.drawTiledPixmap(viewport()->rect(), m_tilePixmap);
    }
    QGraphicsView::paintEvent(event);
}

void CustomGraphicsView::resizeEvent(QResizeEvent *event)
{
    if (m_posInfoWidget) {
        constexpr int kBar = 28;
        m_posInfoWidget->setGeometry(0, height() - kBar, width(), kBar);
        m_posInfoWidget->raise();
    }
    if (!m_image.isNull()) {
        fitFrame();
        onCenter();
    }
    QGraphicsView::resizeEvent(event);
}

// ---------------------------------------------------------------------------
// 视图工具 / 放大镜
// ---------------------------------------------------------------------------

void CustomGraphicsView::applyViewTool()
{
    if (m_loupeActive) {
        m_loupeActive = false;
        hideLoupe();
        viewport()->releaseMouse();
    }
    if (m_roiDrawing) {
        setDragMode(QGraphicsView::NoDrag);
        unsetCursor();
        viewport()->unsetCursor();
        return;
    }
    switch (m_viewTool) {
    case ViewTool::Pan:
        setDragMode(QGraphicsView::ScrollHandDrag);
        unsetCursor();
        viewport()->unsetCursor();
        break;
    case ViewTool::Zoom:
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::CrossCursor);
        viewport()->setCursor(Qt::CrossCursor);
        break;
    case ViewTool::Select:
    default:
        setDragMode(QGraphicsView::NoDrag);
        unsetCursor();
        viewport()->unsetCursor();
        break;
    }
}

void CustomGraphicsView::updateLoupe(const QPoint &viewPos)
{
    if (m_image.isNull()) {
        return;
    }

    constexpr int kLoupe = 220;
    constexpr double kMag = 4.0;
    // 在当前视图倍率基础上再放大 kMag 倍（不是相对原图像素固定倍率）
    const double viewScale = qMax(1e-6, transform().m11());
    const int srcSide = qMax(1, qRound(kLoupe / (kMag * viewScale)));
    const QPointF scene = mapToScene(viewPos);
    const int cx = qRound(scene.x());
    const int cy = qRound(scene.y());
    const QRect src(cx - srcSide / 2, cy - srcSide / 2, srcSide, srcSide);

    QPixmap tile(srcSide, srcSide);
    tile.fill(QColor(24, 24, 24));
    {
        QPainter tp(&tile);
        const QRect imgRect(0, 0, m_image.width(), m_image.height());
        const QRect visible = src.intersected(imgRect);
        if (!visible.isEmpty()) {
            tp.drawPixmap(visible.topLeft() - src.topLeft(), m_image, visible);
        }
    }

    QPixmap scaled = tile.scaled(kLoupe, kLoupe, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    {
        QPainter sp(&scaled);
        sp.setRenderHint(QPainter::Antialiasing, true);
        sp.setPen(QPen(QColor(255, 210, 40), 2));
        sp.drawRect(1, 1, kLoupe - 3, kLoupe - 3);
        // 中心红点：对准鼠标所在像素
        sp.setPen(Qt::NoPen);
        sp.setBrush(QColor(230, 30, 30));
        sp.drawEllipse(QPointF(kLoupe / 2.0, kLoupe / 2.0), 1.5, 1.5);
    }

    if (!m_loupe) {
        m_loupe = new QLabel(viewport());
        m_loupe->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    m_loupe->setPixmap(scaled);
    m_loupe->resize(scaled.size());
    // 放大镜以鼠标为中心，随移动跟手
    m_loupe->move(viewPos - QPoint(kLoupe / 2, kLoupe / 2));
    m_loupe->show();
    m_loupe->raise();
}

void CustomGraphicsView::hideLoupe()
{
    if (m_loupe) {
        m_loupe->hide();
    }
}

// ---------------------------------------------------------------------------
// 缩放 / 居中 / 背景
// ---------------------------------------------------------------------------

void CustomGraphicsView::onCenter()
{
    if (!m_imageItem || m_image.isNull()) {
        return;
    }
    m_imageItem->setPos(0, 0);
    centerOn(m_image.width() / 2.0, m_image.height() / 2.0);
}

void CustomGraphicsView::onZoom(float scaleFactor)
{
    if (scaleFactor <= 0.0f) {
        return;
    }
    m_zoomValue *= scaleFactor;
    scale(scaleFactor, scaleFactor);
}

void CustomGraphicsView::fitFrame()
{
    if (width() < 1 || height() < 1 || m_image.width() < 1 || m_image.height() < 1) {
        return;
    }
    // 取宽/高缩放比中较大者，保证整图完整落入视口（letterbox 效果）
    const double winWidth = width();
    const double winHeight = height();
    const double scaleWidth = (m_image.width() + 1.0) / winWidth;
    const double scaleHeight = (m_image.height() + 1.0) / winHeight;
    const double sTemp = scaleWidth >= scaleHeight ? 1.0 / scaleWidth : 1.0 / scaleHeight;
    if (m_zoomValue < 1e-12) {
        m_zoomValue = 1.0;
    }
    const double scaleFactor = sTemp / m_zoomValue;
    onZoom(static_cast<float>(scaleFactor));
    m_zoomValue = sTemp;
}

void CustomGraphicsView::setBackground(bool enabled, bool invertColor)
{
    if (!enabled) {
        return;
    }
    if (m_tilePixmap.isNull()) {
        m_tilePixmap = QPixmap(36, 36);
    }
    m_tilePixmap.fill(invertColor ? QColor(220, 220, 220) : QColor(35, 35, 35));
    QPainter tilePainter(&m_tilePixmap);
    const QColor color(50, 50, 50, 255);
    const QColor invertedColor(210, 210, 210, 255);
    tilePainter.fillRect(0, 0, 18, 18, invertColor ? invertedColor : color);
    tilePainter.fillRect(18, 18, 18, 18, invertColor ? invertedColor : color);
}
