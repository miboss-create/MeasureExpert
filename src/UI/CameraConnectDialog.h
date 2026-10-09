#ifndef CAMERACONNECTDIALOG_H
#define CAMERACONNECTDIALOG_H

#include "Core/CameraTypes.h"

#include <QDialog>
#include <QVector>

class QListWidget;
class QLabel;

namespace Me {

class CameraHub;

/**
 * 选择并连接相机（枚举当前已注册后端的设备）。
 */
class CameraConnectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CameraConnectDialog(CameraHub *hub, QWidget *parent = nullptr);

    CameraDeviceInfo selectedDevice() const;

private:
    void refreshList();

    CameraHub *m_hub = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
    QVector<CameraDeviceInfo> m_devices;
};

} // namespace Me

#endif // CAMERACONNECTDIALOG_H
