#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;

namespace Me {

/**
 * 像素当量标定对话框。
 *
 * 支持直接输入 mm/px，或通过「已知图像长度 + 实际长度」换算。
 * 未启用标定时单位强制为 px。
 */
class CalibrationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalibrationDialog(const CalibrationConfig &config, QWidget *parent = nullptr);

    CalibrationConfig result() const;

private:
    QCheckBox *m_enabled = nullptr;
    QDoubleSpinBox *m_pixelSize = nullptr;
    QComboBox *m_unit = nullptr;
    QDoubleSpinBox *m_knownPx = nullptr;
    QDoubleSpinBox *m_knownMm = nullptr;
};

} // namespace Me

#endif // CALIBRATIONDIALOG_H
