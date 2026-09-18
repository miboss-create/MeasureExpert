#ifndef TRAINALIGNMENTDIALOG_H
#define TRAINALIGNMENTDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>
#include <QRectF>

class QDoubleSpinBox;
class QLabel;

namespace Me {

// 确认训练参数（ROI 已在主图上画好）
class TrainAlignmentDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TrainAlignmentDialog(const AlignmentConfig &config,
                                  const QRectF &roiImageRect,
                                  QWidget *parent = nullptr);

    AlignmentConfig alignmentParams() const;
    SearchRegion modelRoi() const { return m_roi; }

private:
    SearchRegion m_roi;
    QDoubleSpinBox *m_minScore = nullptr;
    QDoubleSpinBox *m_angleStart = nullptr;
    QDoubleSpinBox *m_angleExtent = nullptr;
};

} // namespace Me

#endif // TRAINALIGNMENTDIALOG_H
