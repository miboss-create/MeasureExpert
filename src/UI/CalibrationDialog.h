#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;

namespace Me {

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
