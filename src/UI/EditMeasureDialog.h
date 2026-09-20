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
    explicit EditMeasureDialog(const MeasureItem &item,
                               const QVector<MeasureItem> &siblings = {},
                               QWidget *parent = nullptr);

    /// 仅在 Accepted 后有效；在点 OK 时已缓存，避免关窗过程中读控件
    MeasureItem result() const { return m_acceptedItem; }

private:
    void onTypeChanged();
    MeasureItem buildItem() const;
    void onAccepted();

    QString m_id;
    QVector<MeasureItem> m_siblings;
    MeasureItem m_acceptedItem;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QDoubleSpinBox *m_row = nullptr;
    QDoubleSpinBox *m_col = nullptr;
    QDoubleSpinBox *m_phi = nullptr;
    QDoubleSpinBox *m_len1 = nullptr;
    QDoubleSpinBox *m_len2 = nullptr;
    QDoubleSpinBox *m_angleExtent = nullptr;
    QComboBox *m_refA = nullptr;
    QComboBox *m_refB = nullptr;
    QDoubleSpinBox *m_nominal = nullptr;
    QDoubleSpinBox *m_upper = nullptr;
    QDoubleSpinBox *m_lower = nullptr;
    QSpinBox *m_decimals = nullptr;
};

} // namespace Me

#endif // EDITMEASUREDIALOG_H
