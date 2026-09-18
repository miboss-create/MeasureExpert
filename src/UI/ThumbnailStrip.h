#ifndef THUMBNAILSTRIP_H
#define THUMBNAILSTRIP_H

#include <QStringList>
#include <QWidget>

class QListWidget;
class QImage;

// 缩略图条：竖条源图 / 横条结果（暂可复用原图）
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
    bool loadFolder(const QString &folderPath);
    bool openFolderDialog();

    int count() const;
    int currentIndex() const;
    QString currentPath() const;
    QString folderPath() const { return m_folderPath; }
    Orientation orientation() const { return m_orientation; }

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
    static QStringList imageNameFilters();

    Orientation m_orientation = Orientation::Vertical;
    QListWidget *m_list = nullptr;
    QStringList m_paths;
    QString m_folderPath;
};

#endif // THUMBNAILSTRIP_H
