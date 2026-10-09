/**
 * @file CameraHub.cpp
 */

#include "Core/CameraHub.h"

#include "Core/HalconCameraBackend.h"
#include "Core/ICameraBackend.h"

#include <QTimer>

namespace Me {

CameraHub::CameraHub(QObject *parent)
    : QObject(parent)
    , m_previewTimer(new QTimer(this))
{
    m_previewTimer->setTimerType(Qt::CoarseTimer);
    connect(m_previewTimer, &QTimer::timeout, this, &CameraHub::onPreviewTick);
}

CameraHub::~CameraHub()
{
    shutdown();
}

std::unique_ptr<ICameraBackend> CameraHub::createBackend(const QString &backendId)
{
    Q_UNUSED(backendId);
    // 仅 HALCON；旧工程 cameraBackend=OpenCV 也走 HALCON，需重新连接设备
    return std::make_unique<HalconCameraBackend>();
}

QVector<CameraDeviceInfo> CameraHub::enumerate() const
{
    HalconCameraBackend probe;
    return probe.enumerate();
}

bool CameraHub::openDevice(const CameraDeviceInfo &device, QString *error)
{
    stopPreview();
    closeDevice();

    auto backend = createBackend(device.backendId);
    if (!backend->open(device.deviceId, error)) {
        return false;
    }

    m_backend = std::move(backend);
    m_current = device;
    m_current.backendId = QString::fromUtf8(kCameraBackendHalcon);
    if (m_current.displayName.isEmpty()) {
        m_current.displayName = tr("相机 %1").arg(device.deviceId);
    }
    emit openedChanged(true);
    return true;
}

void CameraHub::closeDevice()
{
    stopPreview();
    if (!m_backend) {
        return;
    }
    m_backend->close();
    m_backend.reset();
    m_current = CameraDeviceInfo{};
    if (!m_shuttingDown) {
        emit openedChanged(false);
    }
}

void CameraHub::shutdown()
{
    if (m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    if (m_previewTimer) {
        disconnect(m_previewTimer, nullptr, this, nullptr);
        m_previewTimer->stop();
    }
    blockSignals(true);
    if (m_backend) {
        m_backend->close();
        m_backend.reset();
        m_current = CameraDeviceInfo{};
    }
}

bool CameraHub::isOpen() const
{
    return m_backend && m_backend->isOpen();
}

bool CameraHub::grabOnce(QImage *out, QString *error)
{
    if (m_shuttingDown || !m_backend) {
        if (error) {
            *error = QStringLiteral("相机未连接");
        }
        return false;
    }
    return m_backend->grab(out, error);
}

void CameraHub::startPreview(int intervalMs)
{
    if (m_shuttingDown || !isOpen()) {
        emit errorOccurred(tr("请先连接相机"));
        return;
    }
    if (intervalMs < 33) {
        intervalMs = 33;
    }
    QString err;
    if (m_backend && !m_backend->beginStreaming(&err)) {
        emit errorOccurred(err);
        return;
    }
    m_previewFailStreak = 0;
    m_previewTimer->start(intervalMs);
    emit previewChanged(true);
}

void CameraHub::stopPreview()
{
    const bool wasActive = m_previewTimer && m_previewTimer->isActive();
    if (m_previewTimer) {
        m_previewTimer->stop();
    }
    if (m_backend) {
        m_backend->endStreaming();
    }
    m_previewFailStreak = 0;
    if (wasActive && !m_shuttingDown) {
        emit previewChanged(false);
    }
}

bool CameraHub::isPreviewing() const
{
    return m_previewTimer && m_previewTimer->isActive();
}

void CameraHub::onPreviewTick()
{
    if (m_shuttingDown || !m_backend) {
        return;
    }
    QImage frame;
    QString err;
    if (!grabOnce(&frame, &err)) {
        ++m_previewFailStreak;
        // 偶发超时不立刻停预览（本机摄像头常见）；连续失败再停
        if (m_previewFailStreak >= 8) {
            emit errorOccurred(err);
            stopPreview();
        }
        return;
    }
    m_previewFailStreak = 0;
    if (m_shuttingDown) {
        return;
    }
    emit frameReady(frame);
}

} // namespace Me
