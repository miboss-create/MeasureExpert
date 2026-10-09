#ifndef ICAMERABACKEND_H
#define ICAMERABACKEND_H

/**
 * @file ICameraBackend.h
 * @brief 相机厂商适配器接口：枚举 / 打开 / 采帧 / 关闭。
 *
 * 每个 SDK 实现一类；上层只依赖本接口与 CameraTypes。
 */

#include "Core/CameraTypes.h"

#include <QImage>
#include <QVector>

namespace Me {

class ICameraBackend
{
public:
    virtual ~ICameraBackend() = default;

    virtual QString backendId() const = 0;
    virtual QVector<CameraDeviceInfo> enumerate() = 0;

    virtual bool open(const QString &deviceId, QString *error = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /**
     * 预览连续采集（可选）。默认空操作。
     * HALCON：GrabImageStart，后续 grab 走 GrabImageAsync，避免本机摄像头每帧重新开关。
     */
    virtual bool beginStreaming(QString *error = nullptr)
    {
        (void)error;
        return true;
    }
    virtual void endStreaming() {}

    /** 采一帧，输出 QImage（灰度或 RGB） */
    virtual bool grab(QImage *out, QString *error = nullptr) = 0;
};

} // namespace Me

#endif // ICAMERABACKEND_H
