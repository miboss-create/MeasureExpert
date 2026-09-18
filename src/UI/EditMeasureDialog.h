#ifndef EDITMEASUREDIALOG_H
#define EDITMEASUREDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace Me {

class EditMeasureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditMeasureDialog(const MeasureItem &item, QWidget *parent = nullptr);

    MeasureItem result() const;

private:
    QString m_id;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QDoubleSpinBox *m_row = nullptr;
    QDoubleSpinBox *m_col = nullptr;
    QDoubleSpinBox *m_phi = nullptr;
    QDoubleSpinBox *m_len1 = nullptr;
    QDoubleSpinBox *m_len2 = nullptr;
    QDoubleSpinBox *m_nominal = nullptr;
    QDoubleSpinBox *m_upper = nullptr;
    QDoubleSpinBox *m_lower = nullptr;
    QSpinBox *m_decimals = nullptr;
};

} // namespace Me

#endif // EDITMEASUREDIALOG_H
