#ifndef CAMERAHUB_H
#define CAMERAHUB_H

/**
 * @file CameraHub.h
 * @brief 采集侧相机门面：枚举/连接/单帧/预览；内部按后端工厂创建适配器。
 *
 * 采图线程策略（MVP）：预览用 UI 定时器软触发 grab，算法仍走 RunEngine 工作线程。
 */

#include "Core/CameraTypes.h"

#include <QImage>
#include <QObject>
#include <QVector>

#include <memory>

class QTimer;

namespace Me {

class ICameraBackend;

class CameraHub : public QObject
{
    Q_OBJECT
public:
    explicit CameraHub(QObject *parent = nullptr);
    ~CameraHub() override;

    QVector<CameraDeviceInfo> enumerate() const;

    bool openDevice(const CameraDeviceInfo &device, QString *error = nullptr);
    void closeDevice();
    /** 退出前调用：停预览、断开定时器、释放设备（避免 MSMF 析构期异常） */
    void shutdown();
    bool isOpen() const;

    CameraDeviceInfo currentDevice() const { return m_current; }

    bool grabOnce(QImage *out, QString *error = nullptr);

    void startPreview(int intervalMs = 50); ///< 默认约 20fps（连续采集时）
    void stopPreview();
    bool isPreviewing() const;

signals:
    void frameReady(const QImage &image);
    void openedChanged(bool open);
    void previewChanged(bool on);
    void errorOccurred(const QString &message);

private slots:
    void onPreviewTick();

private:
    static std::unique_ptr<ICameraBackend> createBackend(const QString &backendId);

    std::unique_ptr<ICameraBackend> m_backend;
    CameraDeviceInfo m_current;
    QTimer *m_previewTimer = nullptr;
    bool m_shuttingDown = false;
    int m_previewFailStreak = 0;
};

} // namespace Me

#endif // CAMERAHUB_H
