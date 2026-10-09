#ifndef IIMAGESOURCE_H
#define IIMAGESOURCE_H

/**
 * @file IImageSource.h
 * @brief 图像源抽象：统一 open / grab / close 接口。
 *
 * - 文件夹：主界面仍多用 ThumbnailStrip；FolderImageSource 保留兼容。
 * - 相机：请使用 CameraHub + ICameraBackend（HALCON Image Acquisition），
 *   不再在本文件内堆厂商逻辑。
 */

#include <QImage>
#include <QString>

namespace Me {

/**
 * 图像采集源。
 * grab 每次返回一帧；文件夹模式可顺序或随机（由具体实现决定）。
 */
class IImageSource
{
public:
    virtual ~IImageSource() = default;

    virtual QString name() const = 0;
    virtual bool open(QString *error = nullptr) = 0;
    virtual void close() = 0;
    virtual bool grab(QImage *out, QString *error = nullptr) = 0;
};

/**
 * 文件夹图像源（路径由 setFolderPath 设置）。
 * 当前 grab 未实现，主流程请使用缩略图条选图。
 */
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

} // namespace Me

#endif // IIMAGESOURCE_H
