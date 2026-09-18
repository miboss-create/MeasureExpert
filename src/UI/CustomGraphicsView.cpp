#include "CustomGraphicsView.h"

#include "CustomImageItem.h"
#include "RoiRectItem.h"

#include <QContextMenuEvent>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace {
constexpr double kZoomMax = 50.0;
constexpr double kZoomMin = 0.1;
constexpr qreal kDefaultScenePad = 2000.0;
}

CustomGraphicsView::CustomGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setBackground();
    initWidget();
}

CustomGraphicsView::~CustomGraphicsView() = default;

bool CustomGraphicsView::initWidget()
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-kDefaultScenePad, -kDefaultScenePad,
                          kDefaultScenePad * 2.0, kDefaultScenePad * 2.0);
    setScene(m_scene);

    m_imageItem = new CustomImageItem();
    m_scene->addItem(m_imageItem);

    m_posInfoLabel = new QLabel(this);
    m_posInfoWidget = new QWidget(this);
    m_posInfoLabel->setStyleSheet(QStringLiteral(
        "color:rgb(200,255,200); "
        "background-color:rgba(50,50,50,160); "
        "font: Microsoft YaHei;"
        "font-size: 15px;"));
    m_posInfoLabel->setText(QStringLiteral(" W:0,H:0 | X:0,Y:0 | R:0,G:0,B:0"));
    m_posInfoWidget->setFixedHeight(25);
    m_posInfoWidget->setGeometry(0, height() - 25, width(), 25);
    m_posInfoWidget->setStyleSheet(QStringLiteral("background-color:rgba(0,0,0,0);"));

    auto *infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(0);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->addWidget(m_posInfoLabel);
    m_posInfoWidget->setLayout(infoLayout);

    connect(m_imageItem, &CustomImageItem::RGBValue, this, [this](const QString &info) {
        m_posInfoLabel->setText(info);
    });
    return true;
}

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
        return;
    }
    m_roiItem = new RoiRectItem();
    m_roiItem->setStyle(m_roiColor, m_roiLabel);
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

bool CustomGraphicsView::hitRoi(const QPointF &scenePos) const
{
    if (!m_roiItem || !m_roiItem->isVisible() || m_roiItem->roiRect().isEmpty()) {
        return false;
    }
    return m_roiItem->contains(m_roiItem->mapFromScene(scenePos));
}

void CustomGraphicsView::enterRoiDrawing()
{
    m_roiDrawing = true;
    m_drawingRoi = false;
    setDragMode(QGraphicsView::NoDrag);
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
        setDragMode(QGraphicsView::ScrollHandDrag);
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
        setDragMode(m_savedDragMode);
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
    scrollAmount.y() > 0 ? onZoom(1.1f) : onZoom(0.9f);
}

void CustomGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_image.isNull() && !m_roiEditEnabled) {
        fitFrame();
        onCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CustomGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (!m_roiEditEnabled || m_image.isNull()) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());

    if (event->button() == Qt::LeftButton) {
        const bool onRoi = hitRoi(scenePos);
        if (onRoi) {
            // 交给 RoiRectItem 拖动/改大小；临时关掉手势拖图，避免抢事件
            if (!m_roiDrawing) {
                setDragMode(QGraphicsView::NoDrag);
            }
            if (m_roiItem) {
                m_roiItem->setSelected(true);
            }
            QGraphicsView::mousePressEvent(event);
            return;
        }

        // 绘制态：空白处左键拖出新 ROI
        if (m_roiDrawing) {
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

        // 已确认：空白处拖动画布
        setDragMode(QGraphicsView::ScrollHandDrag);
        if (m_roiItem) {
            m_roiItem->setSelected(false);
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void CustomGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawingRoi && m_roiItem) {
        const QPointF scenePos = mapToScene(event->pos());
        QRectF r = QRectF(m_drawStartScene, scenePos).normalized();
        m_roiItem->setRoiRect(r);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CustomGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drawingRoi && event->button() == Qt::LeftButton) {
        m_drawingRoi = false;
        if (m_roiItem && (m_roiItem->roiRect().width() < 4.0 || m_roiItem->roiRect().height() < 4.0)) {
            m_roiItem->setRoiRect(defaultRoiRect());
        }
        event->accept();
        return;
    }

    // 调整态：松开后恢复可拖图（若仍在编辑且非绘制态）
    if (m_roiEditEnabled && !m_roiDrawing && event->button() == Qt::LeftButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
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
        m_posInfoWidget->setGeometry(0, height() - 25, width(), 25);
    }
    if (!m_image.isNull()) {
        fitFrame();
        onCenter();
    }
    QGraphicsView::resizeEvent(event);
}

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
