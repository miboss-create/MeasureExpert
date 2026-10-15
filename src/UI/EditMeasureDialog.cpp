// EditMeasureDialog — 测量项属性编辑（类型驱动字段可见性）

#include "UI/EditMeasureDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVariant>
#include <QVBoxLayout>

namespace Me {
namespace {

/** 需要 ROI 几何字段的测量类型 */
bool needsRoi(MeasureType t)
{
    return t == MeasureType::CaliperWidth
        || t == MeasureType::CaliperDiameter
        || t == MeasureType::FindLine
        || t == MeasureType::FindCircle;
}

/** 需要引用 A/B 的派生测量类型 */
bool needsRefs(MeasureType t)
{
    return t == MeasureType::PointToLine
        || t == MeasureType::LineIntersection
        || t == MeasureType::Angle
        || t == MeasureType::PointDistance;
}

/** 找圆专用：圆弧扫描范围 */
bool needsAngleExtent(MeasureType t)
{
    return t == MeasureType::FindCircle;
}

/** 卡尺/找线/找圆：边缘检测高级参数 */
bool needsEdgeParams(MeasureType t)
{
    return needsRoi(t);
}

double finiteOr(double v, double fallback)
{
    return qIsFinite(v) ? v : fallback;
}

MeasureType typeFromCombo(const QComboBox *box)
{
    if (!box) {
        return MeasureType::CaliperWidth;
    }
    const QString key = box->currentData().toString();
    if (key.isEmpty()) {
        return MeasureType::CaliperWidth;
    }
    return measureTypeFromString(key);
}

} // namespace

EditMeasureDialog::EditMeasureDialog(const MeasureItem &item,
                                     const QVector<MeasureItem> &siblings,
                                     QWidget *parent)
    : QDialog(parent)
    , m_id(item.id)
    , m_roiDefined(item.roiDefined)
    , m_siblings(siblings)
    , m_acceptedItem(item)
{
    setWindowTitle(tr("编辑测量项"));
    setMinimumWidth(460);

    m_name = new QLineEdit(item.name, this);

    m_type = new QComboBox(this);
    m_type->addItem(tr("卡尺宽度 CaliperWidth"), QStringLiteral("CaliperWidth"));
    m_type->addItem(tr("卡尺直径 CaliperDiameter"), QStringLiteral("CaliperDiameter"));
    m_type->addItem(tr("找线 FindLine"), QStringLiteral("FindLine"));
    m_type->addItem(tr("找圆 FindCircle"), QStringLiteral("FindCircle"));
    m_type->addItem(tr("点线距 PointToLine"), QStringLiteral("PointToLine"));
    m_type->addItem(tr("两线交点 LineIntersection"), QStringLiteral("LineIntersection"));
    m_type->addItem(tr("夹角 Angle"), QStringLiteral("Angle"));
    m_type->addItem(tr("点距 PointDistance"), QStringLiteral("PointDistance"));

    auto makeSpin = [this](double minV, double maxV, double v, int decimals = 3) {
        auto *s = new QDoubleSpinBox(this);
        s->setDecimals(decimals);
        s->setRange(minV, maxV);
        s->setValue(finiteOr(v, minV));
        return s;
    };

    m_row = makeSpin(-1e6, 1e6, item.geometry.row);
    m_col = makeSpin(-1e6, 1e6, item.geometry.column);
    m_phi = makeSpin(-6.3, 6.3, item.geometry.phi, 4);
    m_len1 = makeSpin(0.1, 1e6, item.geometry.length1);
    m_len2 = makeSpin(0.1, 1e6, item.geometry.length2);
    m_angleExtent = makeSpin(0.01, 12.6,
                             finiteOr(item.geometry.angleExtent, 6.283185307179586), 4);
    m_edgeSigma = makeSpin(0.4, 8.0, finiteOr(item.edgeSigma, 1.5), 2);
    m_edgeThreshold = makeSpin(1.0, 255.0, finiteOr(item.edgeThreshold, 20.0), 1);
    m_samplePitch = makeSpin(2.0, 40.0, finiteOr(item.samplePitchPx, 6.0), 1);

    auto fillRefs = [this](QComboBox *box, const QString &selected) {
        const QSignalBlocker blocker(box);
        box->clear();
        box->addItem(tr("(无)"), QVariant()); // 显式无效 QVariant，勿用空 QString
        for (const MeasureItem &s : m_siblings) {
            if (s.id == m_id) {
                continue;
            }
            const QString label = s.name + QStringLiteral(" [")
                + measureTypeToString(s.type) + QLatin1Char(']');
            box->addItem(label, QVariant(s.id));
        }
        const int idx = selected.isEmpty() ? 0 : box->findData(QVariant(selected));
        box->setCurrentIndex(idx >= 0 ? idx : 0);
    };

    m_refA = new QComboBox(this);
    m_refB = new QComboBox(this);
    fillRefs(m_refA, item.refIdA);
    fillRefs(m_refB, item.refIdB);

    m_nominal = makeSpin(-1e9, 1e9, item.nominal);
    m_upper = makeSpin(-1e9, 1e9, item.upper);
    m_lower = makeSpin(-1e9, 1e9, item.lower);

    m_decimals = new QSpinBox(this);
    m_decimals->setRange(0, 6);
    m_decimals->setValue(qBound(0, item.decimals, 6));

    auto *form = new QFormLayout();
    form->addRow(tr("名称"), m_name);
    form->addRow(tr("类型"), m_type);
    form->addRow(tr("相对模板 row / 圆心"), m_row);
    form->addRow(tr("相对模板 col / 圆心"), m_col);
    form->addRow(tr("相对模板 phi(rad)"), m_phi);
    form->addRow(tr("length1 / 半径"), m_len1);
    form->addRow(tr("length2 / 环宽"), m_len2);
    form->addRow(tr("圆弧 angleExtent(rad)"), m_angleExtent);
    form->addRow(tr("边缘平滑 Sigma"), m_edgeSigma);
    form->addRow(tr("边缘阈值 Threshold"), m_edgeThreshold);
    form->addRow(tr("采样步长(px)"), m_samplePitch);
    form->addRow(tr("引用 A"), m_refA);
    form->addRow(tr("引用 B"), m_refB);
    form->addRow(tr("名义值"), m_nominal);
    form->addRow(tr("上公差"), m_upper);
    form->addRow(tr("下公差"), m_lower);
    form->addRow(tr("小数位"), m_decimals);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EditMeasureDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onTypeChanged(); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    {
        const QSignalBlocker blocker(m_type);
        const int typeIdx = m_type->findData(measureTypeToString(item.type));
        m_type->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);
    }
    onTypeChanged();
}

void EditMeasureDialog::onTypeChanged()
{
    if (!m_type || !m_row || !m_col || !m_phi || !m_len1 || !m_len2
        || !m_angleExtent || !m_edgeSigma || !m_edgeThreshold || !m_samplePitch
        || !m_refA || !m_refB) {
        return;
    }

    const MeasureType t = typeFromCombo(m_type);
    const bool roi = needsRoi(t);
    const bool refs = needsRefs(t);
    const bool arc = needsAngleExtent(t);
    const bool edge = needsEdgeParams(t);

    m_row->setEnabled(roi);
    m_col->setEnabled(roi);
    m_phi->setEnabled(roi);
    m_len1->setEnabled(roi);
    m_len2->setEnabled(roi);
    m_angleExtent->setEnabled(arc);
    m_edgeSigma->setEnabled(edge);
    m_edgeThreshold->setEnabled(edge);
    // 采样步长仅找线/找圆有意义；卡尺为单剖面
    m_samplePitch->setEnabled(t == MeasureType::FindLine || t == MeasureType::FindCircle);
    m_refA->setEnabled(refs);
    m_refB->setEnabled(refs);

    if (t == MeasureType::FindCircle) {
        const double r = m_len1->value();
        if (m_len2->value() >= r) {
            const QSignalBlocker blocker(m_len2);
            m_len2->setValue(qBound(m_len2->minimum(), r * 0.15, r * 0.9));
        }
    }

    // 切换类型时，若阈值仍是另一类工具的默认值，则换成当前类默认
    if (t == MeasureType::FindLine || t == MeasureType::FindCircle) {
        if (qAbs(m_edgeThreshold->value() - 20.0) < 0.05) {
            const QSignalBlocker blocker(m_edgeThreshold);
            m_edgeThreshold->setValue(10.0);
        }
    } else if (edge && qAbs(m_edgeThreshold->value() - 10.0) < 0.05) {
        const QSignalBlocker blocker(m_edgeThreshold);
        m_edgeThreshold->setValue(20.0);
    }
}

MeasureItem EditMeasureDialog::buildItem() const
{
    MeasureItem item;
    item.id = m_id;
    item.name = m_name ? m_name->text().trimmed() : QString();
    item.type = typeFromCombo(m_type);
    item.geometry.row = m_row ? m_row->value() : 0.0;
    item.geometry.column = m_col ? m_col->value() : 0.0;
    item.geometry.phi = m_phi ? m_phi->value() : 0.0;
    item.geometry.length1 = m_len1 ? m_len1->value() : 40.0;
    item.geometry.length2 = m_len2 ? m_len2->value() : 8.0;
    item.geometry.angleExtent = m_angleExtent ? m_angleExtent->value() : 6.283185307179586;
    if (item.type == MeasureType::FindCircle
        && item.geometry.length2 >= item.geometry.length1) {
        item.geometry.length2 = qMax(1.0, item.geometry.length1 * 0.15);
    }

    item.refIdA.clear();
    item.refIdB.clear();
    if (needsRefs(item.type)) {
        if (m_refA && m_refA->currentData().isValid()) {
            item.refIdA = m_refA->currentData().toString();
        }
        if (m_refB && m_refB->currentData().isValid()) {
            item.refIdB = m_refB->currentData().toString();
        }
    }

    item.nominal = m_nominal ? m_nominal->value() : 0.0;
    item.upper = m_upper ? m_upper->value() : 0.0;
    item.lower = m_lower ? m_lower->value() : 0.0;
    item.decimals = m_decimals ? m_decimals->value() : 3;
    item.unit = (item.type == MeasureType::Angle) ? QStringLiteral("deg")
                                                  : QStringLiteral("px");
    item.roiDefined = m_roiDefined;
    item.edgeSigma = m_edgeSigma ? m_edgeSigma->value() : 1.5;
    item.edgeThreshold = m_edgeThreshold ? m_edgeThreshold->value() : 20.0;
    item.samplePitchPx = m_samplePitch ? m_samplePitch->value() : 6.0;
    return item;
}

void EditMeasureDialog::onAccepted()
{
    // 先收起下拉，再缓存结果，最后 accept，避免关窗时读半销毁控件
    if (m_type) {
        m_type->hidePopup();
    }
    if (m_refA) {
        m_refA->hidePopup();
    }
    if (m_refB) {
        m_refB->hidePopup();
    }
    m_acceptedItem = buildItem();
    accept();
}

} // namespace Me
