/**
 * @file HalconCameraBackend.cpp
 * @brief 基于 HALCON open_framegrabber / grab_image（参见官方 image_acquisition 例程）。
 */

#include "Core/HalconCameraBackend.h"

#include "Alg/HalconUtils.h"
#include "ImageConvert.h"

#include <QDebug>

#include "HalconCpp.h"

namespace Me {
namespace {

/**
 * Windows 本机摄像头：OpenCV 用 MSMF；HALCON 对应 MediaFoundation / DirectShow
 *（需安装对应采集接口 DLL，不是 opencv_world 自带的）。
 * 工业相机：GenICamTL / GigEVision2 / USB3Vision。
 */
const char *const kIfacePrefer[] = {
#if defined(_WIN32)
    "MediaFoundation",
    "DirectShow",
#endif
    "GenICamTL",
    "GigEVision2",
    "USB3Vision",
};

QString htupleString(const HalconCpp::HTuple &t, Hlong i)
{
    try {
        const HalconCpp::HString s = t[i].S();
        const char *utf8 = s.ToUtf8();
        if (utf8 && utf8[0] != '\0') {
            return QString::fromUtf8(utf8);
        }
        return QString::fromLocal8Bit(s.Text());
    } catch (...) {
        return {};
    }
}

QString extractField(const QString &device, const QString &key)
{
    // "... | user_name:MyCam | device:xxx | ..."
    const QString needle = key + QLatin1Char(':');
    const int pos = device.indexOf(needle, 0, Qt::CaseInsensitive);
    if (pos < 0) {
        return {};
    }
    int begin = pos + needle.size();
    while (begin < device.size() && device.at(begin).isSpace()) {
        ++begin;
    }
    int end = device.indexOf(QStringLiteral(" | "), begin);
    if (end < 0) {
        end = device.size();
    }
    return device.mid(begin, end - begin).trimmed();
}

/** 同一物理相机在 device / info_boards 下字符串可能略有差别，用稳定键去重 */
QString deviceDedupeKey(const QString &iface, const QString &device)
{
    QString key = extractField(device, QStringLiteral("device"));
    if (key.isEmpty()) {
        key = extractField(device, QStringLiteral("unique_name"));
    }
    if (key.isEmpty()) {
        key = device.trimmed();
    }
    return iface + QLatin1Char('\x1f') + key;
}

bool listContainsDedupeKey(const QVector<CameraDeviceInfo> &list, const QString &key)
{
    for (const CameraDeviceInfo &d : list) {
        const int sep = d.deviceId.indexOf(QStringLiteral(">>"));
        if (sep <= 0) {
            continue;
        }
        if (deviceDedupeKey(d.deviceId.left(sep), d.deviceId.mid(sep + 2)) == key) {
            return true;
        }
    }
    return false;
}

/** 从 info_framegrabber 的 ValueList 追加设备，返回新增数量 */
int appendFromInfo(const char *ifaceC,
                   const QString &iface,
                   const char *query,
                   const QString &backendId,
                   QVector<CameraDeviceInfo> *list)
{
    using namespace HalconCpp;
    HTuple information;
    HTuple valueList;
    InfoFramegrabber(ifaceC, query, &information, &valueList);
    int added = 0;
    const Hlong n = valueList.Length();
    for (Hlong i = 0; i < n; ++i) {
        const QString device = htupleString(valueList, i);
        if (device.isEmpty()) {
            continue;
        }
        const QString key = deviceDedupeKey(iface, device);
        if (listContainsDedupeKey(*list, key)) {
            continue;
        }
        CameraDeviceInfo info;
        info.backendId = backendId;
        info.deviceId = iface + QStringLiteral(">>") + device;
        QString name = extractField(device, QStringLiteral("user_name"));
        if (name.isEmpty()) {
            name = extractField(device, QStringLiteral("device"));
        }
        if (name.isEmpty()) {
            name = device;
            if (name.size() > 64) {
                name = name.left(61) + QStringLiteral("...");
            }
        }
        info.displayName = QStringLiteral("%1 · %2").arg(iface, name);
        list->push_back(info);
        ++added;
    }
    return added;
}

} // namespace

HalconCameraBackend::HalconCameraBackend() = default;

HalconCameraBackend::~HalconCameraBackend()
{
    close();
}

QString HalconCameraBackend::backendId() const
{
    return QString::fromUtf8(kCameraBackendHalcon);
}

QString HalconCameraBackend::makeDeviceId(const QString &iface, const QString &device)
{
    return iface + QStringLiteral(">>") + device;
}

bool HalconCameraBackend::parseDeviceId(const QString &deviceId, QString *ifaceOut,
                                        QString *deviceOut, QString *error)
{
    if (!ifaceOut || !deviceOut) {
        return false;
    }
    const int sep = deviceId.indexOf(QStringLiteral(">>"));
    if (sep <= 0 || sep + 2 >= deviceId.size()) {
        if (error) {
            *error = QStringLiteral("无效的相机 ID: %1（期望 接口>>设备）").arg(deviceId);
        }
        return false;
    }
    *ifaceOut = deviceId.left(sep);
    *deviceOut = deviceId.mid(sep + 2);
    return true;
}

QString HalconCameraBackend::displayNameFor(const QString &iface, const QString &device)
{
    QString name = extractField(device, QStringLiteral("user_name"));
    if (name.isEmpty()) {
        name = extractField(device, QStringLiteral("device"));
    }
    if (name.isEmpty()) {
        name = extractField(device, QStringLiteral("unique_name"));
    }
    if (name.isEmpty()) {
        name = device;
        if (name.size() > 64) {
            name = name.left(61) + QStringLiteral("...");
        }
    }
    return QStringLiteral("%1 · %2").arg(iface, name);
}

QVector<CameraDeviceInfo> HalconCameraBackend::enumerate()
{
    using namespace HalconCpp;
    QVector<CameraDeviceInfo> list;
    const QString be = backendId();

    for (const char *ifaceC : kIfacePrefer) {
        const QString iface = QString::fromLatin1(ifaceC);
        bool ifaceOk = false;
        const int before = list.size();
        int added = 0;

        // 优先 'device'（可打开）；仅当为空时再用 'info_boards'，避免同一摄像头列两次
        try {
            added = appendFromInfo(ifaceC, iface, "device", be, &list);
            ifaceOk = true;
        } catch (const HException &ex) {
            qInfo().noquote()
                << QStringLiteral("[相机] 接口 %1 (device) 不可用: %2")
                       .arg(iface, fromHException(ex));
        }
        if (added == 0) {
            try {
                added = appendFromInfo(ifaceC, iface, "info_boards", be, &list);
                ifaceOk = true;
            } catch (const HException &ex) {
                qInfo().noquote()
                    << QStringLiteral("[相机] 接口 %1 (info_boards) 不可用: %2")
                           .arg(iface, fromHException(ex));
            }
        }

        // 本机接口已加载但列表为空时，仍可尝试 Device=default
        if (ifaceOk && list.size() == before
            && (iface == QLatin1String("MediaFoundation")
                || iface == QLatin1String("DirectShow"))) {
            CameraDeviceInfo info;
            info.backendId = be;
            info.deviceId = makeDeviceId(iface, QStringLiteral("default"));
            info.displayName = QStringLiteral("%1 · 默认摄像头").arg(iface);
            list.push_back(info);
            qInfo().noquote()
                << QStringLiteral("[相机] %1 无设备列表，加入 default 项").arg(iface);
        }
    }

    if (list.isEmpty()) {
        qWarning().noquote() << QStringLiteral(
            "[相机] HALCON 未枚举到设备。本机摄像头需要已安装的 MediaFoundation/"
            "DirectShow 采集接口；工业相机需要 GenICamTL 等，并关闭 MVS 占用。");
    } else {
        qInfo().noquote() << QStringLiteral("[相机] HALCON 枚举到 %1 台").arg(list.size());
    }
    return list;
}

bool HalconCameraBackend::open(const QString &deviceId, QString *error)
{
    using namespace HalconCpp;
    close();

    QString iface;
    QString device;
    if (!parseDeviceId(deviceId, &iface, &device, error)) {
        return false;
    }

    try {
        auto acq = std::make_unique<HFramegrabber>();
        // 参数与官方 first_example / GenICam 例程默认打开一致；不改曝光增益，沿用相机内 UserSet
        acq->OpenFramegrabber(iface.toLocal8Bit().constData(),
                              1, 1, 0, 0, 0, 0,
                              "default", -1, "default", -1.0, "false",
                              "default", device.toLocal8Bit().constData(),
                              0, -1);
        // 软超时：单帧失败时尽快返回，避免预览长时间卡住像“断开”
        try {
            acq->SetFramegrabberParam(HTuple("grab_timeout"), HTuple(800));
        } catch (const HException &) {
            // 部分接口无此参数
        }
        m_acq = std::move(acq);
        m_deviceId = deviceId;
        m_streaming = false;
        qInfo().noquote() << QStringLiteral("[相机] 已打开 %1").arg(m_deviceId);
        return true;
    } catch (const HException &ex) {
        if (error) {
            *error = QStringLiteral("无法打开相机（%1）：%2")
                         .arg(iface, fromHException(ex));
        }
        m_acq.reset();
        m_deviceId.clear();
        return false;
    }
}

void HalconCameraBackend::close()
{
    endStreaming();
    if (m_acq) {
        try {
            if (m_acq->IsInitialized()) {
                m_acq->CloseFramegrabber();
            }
        } catch (const HalconCpp::HException &ex) {
            qWarning().noquote()
                << QStringLiteral("[相机] 关闭异常: %1").arg(fromHException(ex));
        }
        m_acq.reset();
    }
    m_deviceId.clear();
}

bool HalconCameraBackend::isOpen() const
{
    return m_acq && m_acq->IsInitialized();
}

bool HalconCameraBackend::beginStreaming(QString *error)
{
    using namespace HalconCpp;
    if (!isOpen()) {
        if (error) {
            *error = QStringLiteral("相机未打开");
        }
        return false;
    }
    if (m_streaming) {
        return true;
    }
    try {
        // 连续抓取：本机 MF/DShow 用同步 GrabImage 会像反复开关摄像头
        m_acq->GrabImageStart(-1.0);
        m_streaming = true;
        qInfo().noquote() << QStringLiteral("[相机] 已开始连续采集");
        return true;
    } catch (const HException &ex) {
        m_streaming = false;
        // 接口不支持连续时退回同步 grab（仍可预览，只是更顿）
        qInfo().noquote() << QStringLiteral("[相机] 连续采集不可用，改用同步: %1")
                                 .arg(fromHException(ex));
        return true;
    }
}

void HalconCameraBackend::endStreaming()
{
    if (!m_streaming) {
        return;
    }
    m_streaming = false;
    qInfo().noquote() << QStringLiteral("[相机] 已结束连续采集");
}

bool HalconCameraBackend::grab(QImage *out, QString *error)
{
    using namespace HalconCpp;
    if (!out) {
        return false;
    }
    if (!isOpen()) {
        if (error) {
            *error = QStringLiteral("相机未打开");
        }
        return false;
    }

    try {
        HImage image;
        if (m_streaming) {
            image = m_acq->GrabImageAsync(-1.0);
        } else {
            image = m_acq->GrabImage();
        }
        if (!HImage2QImage(image, *out) || out->isNull()) {
            if (error) {
                *error = QStringLiteral("帧格式转换失败（需 byte 灰度或 RGB）");
            }
            return false;
        }
        return true;
    } catch (const HException &ex) {
        if (error) {
            *error = QStringLiteral("采图失败: %1").arg(fromHException(ex));
        }
        return false;
    }
}

} // namespace Me
