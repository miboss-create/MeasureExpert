#include "ThumbnailStrip.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QListWidget>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {
constexpr int kThumbSize = 96;
constexpr int kVerticalWidth = 120;
constexpr int kHorizontalHeight = 100;
}

ThumbnailStrip::ThumbnailStrip(Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    if (m_orientation == Orientation::Vertical) {
        setFixedWidth(kVerticalWidth);
    } else {
        setFixedHeight(kHorizontalHeight);
    }
    initWidget();
}

bool ThumbnailStrip::initWidget()
{
    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::ListMode);
    m_list->setIconSize(QSize(kThumbSize, kThumbSize));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setDragEnabled(false);
    m_list->setSpacing(4);
    m_list->setUniformItemSizes(true);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget{background:#2b2b2b;border:none;outline:none;}"
        "QListWidget::item{padding:2px;margin:0px;}"
        "QListWidget::item:selected{background:#3d5a80;border:1px solid #70a0d0;}"));

    if (m_orientation == Orientation::Vertical) {
        m_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setFlow(QListView::TopToBottom);
        m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    } else {
        m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_list->setFlow(QListView::LeftToRight);
        m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged, this, &ThumbnailStrip::onCurrentRowChanged);
    return true;
}

QStringList ThumbnailStrip::imageNameFilters()
{
    return {QStringLiteral("*.png"),
            QStringLiteral("*.jpg"),
            QStringLiteral("*.jpeg"),
            QStringLiteral("*.bmp"),
            QStringLiteral("*.tif"),
            QStringLiteral("*.tiff"),
            QStringLiteral("*.gif"),
            QStringLiteral("*.webp")};
}

void ThumbnailStrip::clear()
{
    m_list->clear();
    m_paths.clear();
    m_folderPath.clear();
}

int ThumbnailStrip::count() const
{
    return m_paths.size();
}

int ThumbnailStrip::currentIndex() const
{
    return m_list->currentRow();
}

QString ThumbnailStrip::currentPath() const
{
    const int row = currentIndex();
    if (row < 0 || row >= m_paths.size()) {
        return {};
    }
    return m_paths.at(row);
}

void ThumbnailStrip::addImagePath(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage full = reader.read();
    if (full.isNull()) {
        return;
    }

    const QImage thumb = full.scaled(kThumbSize,
                                     kThumbSize,
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);

    auto *item = new QListWidgetItem(m_list);
    item->setIcon(QIcon(QPixmap::fromImage(thumb)));
    item->setToolTip(QFileInfo(path).fileName());
    if (m_orientation == Orientation::Vertical) {
        item->setSizeHint(QSize(width() - 8, kThumbSize + 8));
    } else {
        item->setSizeHint(QSize(kThumbSize + 8, height() - 8));
    }
    item->setData(Qt::UserRole, path);

    m_paths.push_back(path);
}

bool ThumbnailStrip::loadFolder(const QString &folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return false;
    }

    const QFileInfoList files = dir.entryInfoList(imageNameFilters(),
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name);
    clear();
    m_folderPath = dir.absolutePath();

    for (const QFileInfo &info : files) {
        addImagePath(info.absoluteFilePath());
    }

    if (m_paths.isEmpty()) {
        return false;
    }

    m_list->setCurrentRow(0);
    return true;
}

bool ThumbnailStrip::openFolderDialog()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this,
        tr("选择图像文件夹"),
        m_folderPath.isEmpty() ? QDir::homePath() : m_folderPath);
    if (folder.isEmpty()) {
        return false;
    }
    return loadFolder(folder);
}

void ThumbnailStrip::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_paths.size()) {
        return;
    }
    if (m_list->currentRow() == index) {
        return;
    }
    m_list->setCurrentRow(index);
}

void ThumbnailStrip::selectNext()
{
    if (m_paths.isEmpty()) {
        return;
    }
    int row = m_list->currentRow();
    if (row < 0) {
        row = 0;
    } else {
        row = (row + 1) % m_paths.size();
    }
    m_list->setCurrentRow(row);
}

void ThumbnailStrip::selectPrevious()
{
    if (m_paths.isEmpty()) {
        return;
    }
    int row = m_list->currentRow();
    if (row < 0) {
        row = 0;
    } else {
        row = (row - 1 + m_paths.size()) % m_paths.size();
    }
    m_list->setCurrentRow(row);
}

void ThumbnailStrip::onCurrentRowChanged(int row)
{
    if (row < 0 || row >= m_paths.size()) {
        return;
    }

    const QString path = m_paths.at(row);
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return;
    }

    emit currentIndexChanged(row);
    emit currentPathChanged(path);
    emit currentImageChanged(image);
}
