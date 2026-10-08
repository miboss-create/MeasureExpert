#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include "Alg/CameraCalibration.h"
#include "Core/ProjectTypes.h"

#include <QDialog>
#include <QImage>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QPushButton;

namespace Me {

/**
 * 标定对话框：
 * 1) 手动像素当量（原有）
 * 2) 棋盘格内参/外参标定；过程图通过 previewRequested 推到主图窗口
 */
class CalibrationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalibrationDialog(const CalibrationConfig &config, QWidget *parent = nullptr);

    CalibrationConfig result() const;

signals:
    /** 标定过程/结果预览（BGR 已转 QImage），供主图窗口显示 */
    void previewRequested(const QImage &image, const QString &caption);
    /**
     * 角点检测可视化结果（非原图），供左侧缩略图列表展示。
     * labels 与 images 等长。
     */
    void calibrationVisualsReady(const QStringList &labels, const QVector<QImage> &images);

private:
    void chooseImageFolder();
    void runChessboardCalibration();
    void saveIntrinsicsXml();
    void loadIntrinsicsXml();
    void applyExtrinsicsToPixelSize();
    void emitPreview(const cv::Mat &bgr, const QString &caption);
    /** 生成左右对比图；pushToList=true 时追加进左侧列表并刷新 */
    QImage makeUndistortCompareImage(const cv::Mat &src) const;
    void showUndistortCompare(const cv::Mat &src, const QString &caption, bool pushToList);
    /** 按角点结果生成可视化图并通知主窗刷新列表 */
    void publishCornerVisuals(const std::vector<std::vector<cv::Point2f>> &imagePoints);
    void publishVisualList();

    // 手动
    QCheckBox *m_enabled = nullptr;
    QDoubleSpinBox *m_pixelSize = nullptr;
    QComboBox *m_unit = nullptr;
    QDoubleSpinBox *m_knownPx = nullptr;
    QDoubleSpinBox *m_knownMm = nullptr;

    // 棋盘格
    QSpinBox *m_boardCols = nullptr;
    QSpinBox *m_boardRows = nullptr;
    QDoubleSpinBox *m_squareMm = nullptr;
    QLabel *m_folderLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_runBtn = nullptr;
    QStringList m_imagePaths;

    CameraCalibration m_calib;
    CalibrateResult m_intrinsics;

    /** 当前要推到左侧列表的可视化图（角点 + 可选畸变对比） */
    QStringList m_vizLabels;
    QVector<QImage> m_vizImages;
};

} // namespace Me

#endif // CALIBRATIONDIALOG_H
