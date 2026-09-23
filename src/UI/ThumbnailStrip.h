#ifndef THUMBNAILSTRIP_H
#define THUMBNAILSTRIP_H

#include <QStringList>
#include <QWidget>

class QListWidget;
class QImage;

/**
 * 缩略图条：文件夹内图像的快速浏览与点选
 *
 * 支持竖向/横向布局；选中项时读取原图并发出 currentImageChanged。
 * 缩略图固定 96×96 等比缩放，原图按需加载（非预加载全文件夹）。
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
    /** 扫描文件夹内常见图像格式，按文件名排序填充列表 */
    bool loadFolder(const QString &folderPath);
    bool openFolderDialog();

    int count() const;
    int currentIndex() const;
    QString currentPath() const;
    QString folderPath() const { return m_folderPath; }
    Orientation orientation() const { return m_orientation; }

public slots:
    /** 循环选择下一项（末项回绕到首项） */
    void selectNext();
    /** 循环选择上一项 */
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
    static QStringList imageNameFilters();

    Orientation m_orientation = Orientation::Vertical;
    QListWidget *m_list = nullptr;
    QStringList m_paths;       ///< 与列表行号一一对应的全路径
    QString m_folderPath;
};

#endif // THUMBNAILSTRIP_H
