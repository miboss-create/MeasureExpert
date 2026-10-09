#include "UI/CameraConnectDialog.h"

#include "Core/CameraHub.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Me {

CameraConnectDialog::CameraConnectDialog(CameraHub *hub, QWidget *parent)
    : QDialog(parent)
    , m_hub(hub)
{
    setWindowTitle(tr("连接相机"));
    setMinimumSize(420, 320);

    m_list = new QListWidget(this);
    m_hint = new QLabel(tr("正在枚举设备…"), this);
    m_hint->setWordWrap(true);

    auto *refreshBtn = new QPushButton(tr("刷新"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &CameraConnectDialog::refreshList);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("连接"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_list->currentRow() < 0 || m_list->currentRow() >= m_devices.size()) {
            QMessageBox::information(this, tr("提示"), tr("请先选择一台相机。"));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *top = new QHBoxLayout();
    top->addWidget(m_hint, 1);
    top->addWidget(refreshBtn);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);

    refreshList();
}

CameraDeviceInfo CameraConnectDialog::selectedDevice() const
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_devices.size()) {
        return {};
    }
    return m_devices.at(row);
}

void CameraConnectDialog::refreshList()
{
    m_list->clear();
    m_devices.clear();
    if (!m_hub) {
        m_hint->setText(tr("相机服务不可用。"));
        return;
    }

    m_hint->setText(tr("正在枚举（HALCON：本机 MF/DShow + GenICam…）…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    m_devices = m_hub->enumerate();
    QApplication::restoreOverrideCursor();

    for (const CameraDeviceInfo &d : m_devices) {
        m_list->addItem(QStringLiteral("%1  [%2]")
                            .arg(d.displayName, d.backendId));
    }

    if (m_devices.isEmpty()) {
        m_hint->setText(tr(
            "未发现可用相机。本机摄像头需 HALCON 的 MediaFoundation/DirectShow 接口；"
            "工业相机需 GenICamTL，并关闭 MVS 占用。"));
    } else {
        m_hint->setText(tr("发现 %1 台（HALCON），选择后点「连接」。")
                            .arg(m_devices.size()));
        m_list->setCurrentRow(0);
    }
}

} // namespace Me
