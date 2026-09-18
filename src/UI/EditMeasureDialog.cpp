#include "UI/EditMeasureDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Me {

EditMeasureDialog::EditMeasureDialog(const MeasureItem &item, QWidget *parent)
    : QDialog(parent)
    , m_id(item.id)
{
    setWindowTitle(tr("编辑测量项"));
    setMinimumWidth(420);

    m_name = new QLineEdit(item.name, this);

    m_type = new QComboBox(this);
    m_type->addItem(tr("卡尺宽度 CaliperWidth"), static_cast<int>(MeasureType::CaliperWidth));
    m_type->addItem(tr("卡尺直径 CaliperDiameter"), static_cast<int>(MeasureType::CaliperDiameter));
    m_type->addItem(tr("点距 PointDistance（预留）"), static_cast<int>(MeasureType::PointDistance));
    m_type->setCurrentIndex(m_type->findData(static_cast<int>(item.type)));

    auto makeSpin = [this](double minV, double maxV, double v, int decimals = 3) {
        auto *s = new QDoubleSpinBox(this);
        s->setDecimals(decimals);
        s->setRange(minV, maxV);
        s->setValue(v);
        return s;
    };

    m_row = makeSpin(-1e6, 1e6, item.geometry.row);
    m_col = makeSpin(-1e6, 1e6, item.geometry.column);
    m_phi = makeSpin(-6.3, 6.3, item.geometry.phi, 4);
    m_len1 = makeSpin(0.1, 1e6, item.geometry.length1);
    m_len2 = makeSpin(0.1, 1e6, item.geometry.length2);
    m_nominal = makeSpin(-1e9, 1e9, item.nominal);
    m_upper = makeSpin(-1e9, 1e9, item.upper);
    m_lower = makeSpin(-1e9, 1e9, item.lower);

    m_decimals = new QSpinBox(this);
    m_decimals->setRange(0, 6);
    m_decimals->setValue(item.decimals);

    auto *form = new QFormLayout();
    form->addRow(tr("名称"), m_name);
    form->addRow(tr("类型"), m_type);
    form->addRow(tr("相对模板 row"), m_row);
    form->addRow(tr("相对模板 col"), m_col);
    form->addRow(tr("相对模板 phi(rad)"), m_phi);
    form->addRow(tr("length1"), m_len1);
    form->addRow(tr("length2"), m_len2);
    form->addRow(tr("名义值"), m_nominal);
    form->addRow(tr("上公差"), m_upper);
    form->addRow(tr("下公差"), m_lower);
    form->addRow(tr("小数位"), m_decimals);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

MeasureItem EditMeasureDialog::result() const
{
    MeasureItem item;
    item.id = m_id;
    item.name = m_name->text().trimmed();
    item.type = static_cast<MeasureType>(m_type->currentData().toInt());
    item.geometry.row = m_row->value();
    item.geometry.column = m_col->value();
    item.geometry.phi = m_phi->value();
    item.geometry.length1 = m_len1->value();
    item.geometry.length2 = m_len2->value();
    item.nominal = m_nominal->value();
    item.upper = m_upper->value();
    item.lower = m_lower->value();
    item.decimals = m_decimals->value();
    item.unit = QStringLiteral("px");
    return item;
}

} // namespace Me
