#include "CustomImageView.h"

#include "CustomGraphicsView.h"

#include <QHBoxLayout>

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
