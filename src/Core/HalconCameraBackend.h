#ifndef HALCONCAMERABACKEND_H
#define HALCONCAMERABACKEND_H

/**
 * @file HalconCameraBackend.h
 * @brief HALCON Image Acquisition：枚举 / 打开 / 采帧（替代 OpenCV VideoCapture）。
 *
 * 优先 GenICamTL / GigEVision2 / USB3Vision；Windows 另扫 MediaFoundation（普通 UVC）。
 * 设备 ID：`接口名>>Device`（Device 为 info_framegrabber 返回值，可直接 open）。
 */

#include "Core/ICameraBackend.h"

#include <memory>

namespace HalconCpp {
class HFramegrabber;
}

namespace Me {

class HalconCameraBackend : public ICameraBackend
{
public:
    HalconCameraBackend();
    ~HalconCameraBackend() override;

    QString backendId() const override;
    QVector<CameraDeviceInfo> enumerate() override;
    bool open(const QString &deviceId, QString *error = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool beginStreaming(QString *error = nullptr) override;
    void endStreaming() override;
    bool grab(QImage *out, QString *error = nullptr) override;

private:
    static bool parseDeviceId(const QString &deviceId, QString *ifaceOut, QString *deviceOut,
                              QString *error);
    static QString makeDeviceId(const QString &iface, const QString &device);
    static QString displayNameFor(const QString &iface, const QString &device);

    std::unique_ptr<HalconCpp::HFramegrabber> m_acq;
    QString m_deviceId;
    bool m_streaming = false;
};

} // namespace Me

#endif // HALCONCAMERABACKEND_H
