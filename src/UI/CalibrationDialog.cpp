// CalibrationDialog — 像素当量标定

#include "UI/CalibrationDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Me {

CalibrationDialog::CalibrationDialog(const CalibrationConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("像素当量标定"));
    setMinimumWidth(360);

    m_enabled = new QCheckBox(tr("启用标定（换算为物理单位）"), this);
    m_enabled->setChecked(config.calibrated);

    m_pixelSize = new QDoubleSpinBox(this);
    m_pixelSize->setDecimals(6);
    m_pixelSize->setRange(1e-9, 1e6);
    m_pixelSize->setValue(config.pixelSizeMm > 0.0 ? config.pixelSizeMm : 1.0);
    m_pixelSize->setSuffix(QStringLiteral(" mm/px"));

    m_unit = new QComboBox(this);
    m_unit->addItem(QStringLiteral("mm"), QStringLiteral("mm"));
    m_unit->addItem(QStringLiteral("px"), QStringLiteral("px"));
    const int unitIdx = m_unit->findData(config.unit);
    m_unit->setCurrentIndex(unitIdx >= 0 ? unitIdx : 0);

    m_knownPx = new QDoubleSpinBox(this);
    m_knownPx->setDecimals(3);
    m_knownPx->setRange(0.001, 1e9);
    m_knownPx->setValue(100.0);
    m_knownPx->setSuffix(QStringLiteral(" px"));

    m_knownMm = new QDoubleSpinBox(this);
    m_knownMm->setDecimals(6);
    m_knownMm->setRange(1e-9, 1e9);
    m_knownMm->setValue(1.0);
    m_knownMm->setSuffix(QStringLiteral(" mm"));

    auto *calcBtn = new QPushButton(tr("由已知长度计算"), this);
    connect(calcBtn, &QPushButton::clicked, this, [this]() {
        if (m_knownPx->value() > 0.0) {
            m_pixelSize->setValue(m_knownMm->value() / m_knownPx->value());
            m_enabled->setChecked(true);
            m_unit->setCurrentIndex(m_unit->findData(QStringLiteral("mm")));
        }
    });

    auto *form = new QFormLayout();
    form->addRow(m_enabled);
    form->addRow(tr("像素当量"), m_pixelSize);
    form->addRow(tr("单位"), m_unit);
    form->addRow(new QLabel(tr("已知长度换算："), this));
    form->addRow(tr("图像长度"), m_knownPx);
    form->addRow(tr("实际长度"), m_knownMm);
    form->addRow(calcBtn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

CalibrationConfig CalibrationDialog::result() const
{
    CalibrationConfig cfg;
    cfg.calibrated = m_enabled->isChecked();
    cfg.pixelSizeMm = m_pixelSize->value();
    cfg.unit = m_unit->currentData().toString();
    if (!cfg.calibrated) {
        cfg.unit = QStringLiteral("px");
    }
    return cfg;
}

} // namespace Me
