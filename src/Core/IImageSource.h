#ifndef IIMAGESOURCE_H
#define IIMAGESOURCE_H

#include <QImage>
#include <QString>

namespace Me {

// 图像源抽象（MVP：文件夹；相机仅预留）
class IImageSource
{
public:
    virtual ~IImageSource() = default;

    virtual QString name() const = 0;
    virtual bool open(QString *error = nullptr) = 0;
    virtual void close() = 0;
    virtual bool grab(QImage *out, QString *error = nullptr) = 0;
};

// 文件夹序列取图（预留扩展点；当前主界面仍用 ThumbnailStrip）
class FolderImageSource : public IImageSource
{
public:
    explicit FolderImageSource(QString folderPath = {})
        : m_folderPath(std::move(folderPath))
    {
    }

    QString name() const override { return QStringLiteral("Folder"); }
    void setFolderPath(const QString &path) { m_folderPath = path; }
    QString folderPath() const { return m_folderPath; }

    bool open(QString *error = nullptr) override
    {
        if (m_folderPath.isEmpty()) {
            if (error) {
                *error = QStringLiteral("文件夹路径为空");
            }
            return false;
        }
        m_opened = true;
        return true;
    }

    void close() override { m_opened = false; }

    bool grab(QImage *out, QString *error = nullptr) override
    {
        Q_UNUSED(out);
        if (error) {
            *error = QStringLiteral("FolderImageSource::grab 尚未接入主流程，请使用缩略图条取图");
        }
        return false;
    }

private:
    QString m_folderPath;
    bool m_opened = false;
};

// 相机取图空实现（V2+）
class CameraImageSource : public IImageSource
{
public:
    QString name() const override { return QStringLiteral("Camera"); }

    bool open(QString *error = nullptr) override
    {
        if (error) {
            *error = QStringLiteral("相机图像源尚未实现");
        }
        return false;
    }

    void close() override {}

    bool grab(QImage *out, QString *error = nullptr) override
    {
        Q_UNUSED(out);
        if (error) {
            *error = QStringLiteral("相机图像源尚未实现");
        }
        return false;
    }
};

} // namespace Me

#endif // IIMAGESOURCE_H
