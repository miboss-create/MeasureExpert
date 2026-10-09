#ifndef CAMERATYPES_H
#define CAMERATYPES_H

/**
 * @file CameraTypes.h
 * @brief 相机设备描述与后端标识（与具体 SDK 解耦）。
 */

#include <QString>

namespace Me {

/** 后端实现 ID：当前仅 HALCON Image Acquisition */
inline constexpr const char *kCameraBackendHalcon = "Halcon";
/** @deprecated 旧工程可能仍写 OpenCV；打开时映射到 Halcon */
inline constexpr const char *kCameraBackendOpenCV = "OpenCV";

struct CameraDeviceInfo {
    QString backendId = QString::fromUtf8(kCameraBackendHalcon);
    QString deviceId;    ///< 后端内唯一，Halcon 为 "接口>>Device"
    QString displayName; ///< UI 展示名
};

} // namespace Me

#endif // CAMERATYPES_H
