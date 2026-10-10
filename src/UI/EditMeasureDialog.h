#ifndef EDITMEASUREDIALOG_H
#define EDITMEASUREDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace Me {

/**
 * 测量项属性编辑对话框。
 *
 * 按测量类型动态启用/禁用字段：
 * - 卡尺/找线/找圆：几何 ROI（row/col/phi/length1/length2）
 * - 派生项（点线距、交点、夹角、点距）：引用 A/B
 * - 找圆：额外 angleExtent
 *
 * 确认时先缓存结果再 accept，避免关窗过程中读半销毁控件。
 */
class EditMeasureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditMeasureDialog(const MeasureItem &item,
                               const QVector<MeasureItem> &siblings = {},
                               QWidget *parent = nullptr);

    /** 仅在 Accepted 后有效；在点 OK 时已缓存，避免关窗过程中读控件 */
    MeasureItem result() const { return m_acceptedItem; }

private:
    /** 类型切换时启用/禁用 ROI、引用、圆弧范围等控件 */
    void onTypeChanged();
    /** 从当前控件值组装 MeasureItem（不修改 m_acceptedItem） */
    MeasureItem buildItem() const;
    /** OK：收起下拉 → buildItem → accept */
    void onAccepted();

    QString m_id;
    bool m_roiDefined = false;
    QVector<MeasureItem> m_siblings;  ///< 同配方其他测量项，供引用下拉填充
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
