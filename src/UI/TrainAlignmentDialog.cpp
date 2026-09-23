// TrainAlignmentDialog — 确认定位训练参数（ROI 已在主图绘制）

#include "UI/TrainAlignmentDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Me {

TrainAlignmentDialog::TrainAlignmentDialog(const AlignmentConfig &config,
                                           const QRectF &roiImageRect,
                                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("确认创建定位模板"));
    setMinimumWidth(380);

    m_roi.row1 = roiImageRect.top();
    m_roi.col1 = roiImageRect.left();
    m_roi.row2 = roiImageRect.bottom();
    m_roi.col2 = roiImageRect.right();

    auto makeSpin = [this](double minV, double maxV, double value, int decimals = 3) {
        auto *s = new QDoubleSpinBox(this);
        s->setDecimals(decimals);
        s->setRange(minV, maxV);
        s->setValue(value);
        return s;
    };

    m_minScore = makeSpin(0.1, 1.0, config.minScore > 0.0 ? config.minScore : 0.5);
    m_minScore->setSingleStep(0.05);
    m_angleStart = makeSpin(-180, 180, config.angleStartDeg, 1);
    m_angleExtent = makeSpin(0, 360, config.angleExtentDeg > 0.0 ? config.angleExtentDeg : 360.0, 1);

    auto *roiLabel = new QLabel(
        tr("ROI: row [%1, %2], col [%3, %4]")
            .arg(m_roi.row1, 0, 'f', 1)
            .arg(m_roi.row2, 0, 'f', 1)
            .arg(m_roi.col1, 0, 'f', 1)
            .arg(m_roi.col2, 0, 'f', 1),
        this);

    auto *form = new QFormLayout();
    form->addRow(tr("当前 ROI"), roiLabel);
    form->addRow(tr("minScore"), m_minScore);
    form->addRow(tr("angleStart(°)"), m_angleStart);
    form->addRow(tr("angleExtent(°)"), m_angleExtent);

    auto *hint = new QLabel(
        tr("角度范围会同时用于训练与查找。board 等有旋转的序列请保持 360°。"),
        this);
    hint->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("训练"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("将使用主图上的绿色 ROI 训练形状模型。"), this));
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(buttons);
}

AlignmentConfig TrainAlignmentDialog::alignmentParams() const
{
    AlignmentConfig cfg;
    cfg.minScore = m_minScore->value();
    cfg.angleStartDeg = m_angleStart->value();
    cfg.angleExtentDeg = m_angleExtent->value();
    return cfg;
}

} // namespace Me
