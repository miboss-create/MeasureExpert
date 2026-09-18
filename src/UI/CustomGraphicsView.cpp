#include "CustomGraphicsView.h"

#include "CustomImageItem.h"

#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QWheelEvent>

namespace {
constexpr double kZoomMax = 50.0;
constexpr double kZoomMin = 0.1;
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
    setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
    setBackground();
    centerOn(0, 0);
    initWidget();
}

CustomGraphicsView::~CustomGraphicsView() = default;

bool CustomGraphicsView::initWidget()
{
    m_scene = new QGraphicsScene(this);
    m_imageItem = new CustomImageItem(this);
    setScene(m_scene);
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

void CustomGraphicsView::setImage(const QImage &image)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    m_qImage = image.copy();
    m_image = QPixmap::fromImage(image);
    m_imageItem->w = m_image.width();
    m_imageItem->h = m_image.height();
    m_imageItem->setPixmap(m_image);
    fitFrame();
    onCenter();
    show();
}

void CustomGraphicsView::wheelEvent(QWheelEvent *event)
{
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
    if (event->button() == Qt::LeftButton) {
        fitFrame();
        onCenter();
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CustomGraphicsView::paintEvent(QPaintEvent *event)
{
    QPainter paint(viewport());
    paint.drawTiledPixmap(QRect(QPoint(0, 0), QPoint(width(), height())), m_tilePixmap);
    QGraphicsView::paintEvent(event);
}

void CustomGraphicsView::resizeEvent(QResizeEvent *event)
{
    fitFrame();
    onCenter();
    m_posInfoWidget->setGeometry(0, height() - 25, width(), 25);
    QGraphicsView::resizeEvent(event);
}

void CustomGraphicsView::onCenter()
{
    centerOn(m_imageItem->pixmap().width() / 2.0, m_imageItem->pixmap().height() / 2.0);
    m_imageItem->setPos(0, 0);
}

void CustomGraphicsView::onZoom(float scaleFactor)
{
    m_zoomValue *= scaleFactor;
    scale(scaleFactor, scaleFactor);
}

void CustomGraphicsView::fitFrame()
{
    if (width() < 1 || m_image.width() < 1) {
        return;
    }
    const double winWidth = width();
    const double winHeight = height();
    const double scaleWidth = (m_image.width() + 1) / winWidth;
    const double scaleHeight = (m_image.height() + 1) / winHeight;
    const double sTemp = scaleWidth >= scaleHeight ? 1.0 / scaleWidth : 1.0 / scaleHeight;
    const double scaleFactor = sTemp / m_zoomValue;
    onZoom(static_cast<float>(scaleFactor));
    m_zoomValue = sTemp;
}

void CustomGraphicsView::setBackground(bool enabled, bool invertColor)
{
    if (!enabled) {
        return;
    }
    m_tilePixmap.fill(invertColor ? QColor(220, 220, 220) : QColor(35, 35, 35));
    QPainter tilePainter(&m_tilePixmap);
    const QColor color(50, 50, 50, 255);
    const QColor invertedColor(210, 210, 210, 255);
    tilePainter.fillRect(0, 0, 18, 18, invertColor ? invertedColor : color);
    tilePainter.fillRect(18, 18, 18, 18, invertColor ? invertedColor : color);
}
