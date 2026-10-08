#ifndef THUMBNAILSTRIP_H
#define THUMBNAILSTRIP_H

#include <QImage>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QListWidget;

/**
 * 缩略图条：文件夹原图，或内存中的可视化图（如标定角点结果）
 *
 * 支持竖向/横向布局；选中项发出 currentImageChanged。
 * 缩略图固定 96×96 等比缩放。
 */
class ThumbnailStrip : public QWidget
{
    Q_OBJECT
public:
    enum class Orientation {
        Vertical,
        Horizontal
    };

    explicit ThumbnailStrip(Orientation orientation = Orientation::Vertical,
                            QWidget *parent = nullptr);

    void clear();
    /** 扫描文件夹内常见图像格式，按文件名排序填充列表（磁盘原图） */
    bool loadFolder(const QString &folderPath);
    bool openFolderDialog();

    /**
     * 用内存图像填充列表（如标定可视化结果）。
     * labels 与 images 等长；点选时直接发出缓存图，不再读盘。
     */
    bool loadImages(const QStringList &labels, const QVector<QImage> &images);

    int count() const;
    int currentIndex() const;
    QString currentPath() const;
    QString folderPath() const { return m_folderPath; }
    Orientation orientation() const { return m_orientation; }
    /** true：当前为 loadImages 缓存模式 */
    bool hasCachedImages() const { return !m_images.isEmpty(); }

public slots:
    void selectNext();
    void selectPrevious();
    void setCurrentIndex(int index);

signals:
    void currentImageChanged(const QImage &image);
    void currentPathChanged(const QString &path);
    void currentIndexChanged(int index);

private slots:
    void onCurrentRowChanged(int row);

private:
    bool initWidget();
    void addImagePath(const QString &path);
    void addCachedImage(const QString &label, const QImage &full);
    static QStringList imageNameFilters();

    Orientation m_orientation = Orientation::Vertical;
    QListWidget *m_list = nullptr;
    QStringList m_paths;     ///< 磁盘路径，或缓存模式下的显示名
    QVector<QImage> m_images; ///< 非空则点选用缓存图
    QString m_folderPath;
};

#endif // THUMBNAILSTRIP_H
