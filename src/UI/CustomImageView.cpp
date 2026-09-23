#include "CustomImageView.h"

#include "CustomGraphicsView.h"

#include <QHBoxLayout>

// ---------------------------------------------------------------------------
// 构造：创建 CustomGraphicsView 并透传 ROI 相关信号
// ---------------------------------------------------------------------------

CustomImageView::CustomImageView(QWidget *parent)
    : QWidget(parent)
{
    initWidget();
}

CustomImageView::~CustomImageView() = default;

bool CustomImageView::initWidget()
{
    m_graphicsView = new CustomGraphicsView(this);
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphicsView);

    connect(m_graphicsView, &CustomGraphicsView::roiChanged, this, &CustomImageView::roiChanged);
    connect(m_graphicsView, &CustomGraphicsView::roiEditEnabledChanged,
            this, &CustomImageView::roiEditEnabledChanged);
    connect(m_graphicsView, &CustomGraphicsView::roiDrawConfirmed,
            this, &CustomImageView::roiDrawConfirmed);
    connect(m_graphicsView, &CustomGraphicsView::roiDeleted,
            this, &CustomImageView::roiDeleted);
    return true;
}

void CustomImageView::setImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    m_image = image;
    m_graphicsView->setImage(image);
}

void CustomImageView::setImagePath(const QString &path)
{
    QImage img(path);
    setImage(img);
}

void CustomImageView::setRoiEditEnabled(bool enabled)
{
    m_graphicsView->setRoiEditEnabled(enabled);
}

bool CustomImageView::isRoiEditEnabled() const
{
    return m_graphicsView->isRoiEditEnabled();
}

bool CustomImageView::isRoiDrawing() const
{
    return m_graphicsView->isRoiDrawing();
}

void CustomImageView::setRoiStyle(const QColor &color, const QString &label)
{
    m_graphicsView->setRoiStyle(color, label);
}

void CustomImageView::setRoiShape(RoiRectItem::Shape shape)
{
    m_graphicsView->setRoiShape(shape);
}

RoiRectItem::Shape CustomImageView::roiShape() const
{
    return m_graphicsView->roiShape();
}

QRectF CustomImageView::roiImageRect() const
{
    return m_graphicsView->roiImageRect();
}

void CustomImageView::setRoiImageRect(const QRectF &rect)
{
    m_graphicsView->setRoiImageRect(rect);
}

void CustomImageView::clearRoi()
{
    m_graphicsView->clearRoi();
}

bool CustomImageView::hasRoi() const
{
    return m_graphicsView->hasRoi();
}
