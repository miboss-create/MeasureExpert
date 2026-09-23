#ifndef TRAINALIGNMENTDIALOG_H
#define TRAINALIGNMENTDIALOG_H

#include "Core/ProjectTypes.h"

#include <QDialog>
#include <QRectF>

class QDoubleSpinBox;
class QLabel;

namespace Me {

/**
 * 定位模板训练参数确认对话框。
 *
 * ROI 已在主窗口主图上绘制完毕；本对话框仅确认
 * minScore、角度搜索范围，并将图像 ROI 转为 SearchRegion。
 */
class TrainAlignmentDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TrainAlignmentDialog(const AlignmentConfig &config,
                                  const QRectF &roiImageRect,
                                  QWidget *parent = nullptr);

    /** 用户确认后的定位参数（不含 modelFile，由 MainWindow 写入） */
    AlignmentConfig alignmentParams() const;
    /** 主图像素 ROI → SearchRegion（row/col 顺序） */
    SearchRegion modelRoi() const { return m_roi; }

private:
    SearchRegion m_roi;
    QDoubleSpinBox *m_minScore = nullptr;
    QDoubleSpinBox *m_angleStart = nullptr;
    QDoubleSpinBox *m_angleExtent = nullptr;
};

} // namespace Me

#endif // TRAINALIGNMENTDIALOG_H
