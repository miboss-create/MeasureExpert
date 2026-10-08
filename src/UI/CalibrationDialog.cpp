// CalibrationDialog — 像素当量 + 棋盘格标定

#include "UI/CalibrationDialog.h"

#include "ImageConvert.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVector>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace Me {
namespace {

QStringList listCalibImages(const QString &folder)
{
    QDir dir(folder);
    const QStringList filters{QStringLiteral("*.bmp"),
                              QStringLiteral("*.jpg"),
                              QStringLiteral("*.jpeg"),
                              QStringLiteral("*.png"),
                              QStringLiteral("*.tif"),
                              QStringLiteral("*.tiff")};
    QStringList out;
    for (const QFileInfo &info : dir.entryInfoList(filters, QDir::Files, QDir::Name)) {
        out.push_back(info.absoluteFilePath());
    }
    return out;
}

} // namespace

CalibrationDialog::CalibrationDialog(const CalibrationConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("标定"));
    setMinimumWidth(420);

    // ---- 手动像素当量 ----
    m_enabled = new QCheckBox(tr("启用标定（换算为物理单位）"), this);
    m_enabled->setChecked(config.calibrated);

    m_pixelSize = new QDoubleSpinBox(this);
    m_pixelSize->setDecimals(6);
    m_pixelSize->setRange(1e-9, 1e6);
    m_pixelSize->setValue(config.pixelSizeMm > 0.0 ? config.pixelSizeMm : 1.0);
    m_pixelSize->setSuffix(QStringLiteral(" mm/px"));

    m_unit = new QComboBox(this);
    m_unit->addItem(QStringLiteral("mm"), QStringLiteral("mm"));
    m_unit->addItem(QStringLiteral("px"), QStringLiteral("px"));
    const int unitIdx = m_unit->findData(config.unit);
    m_unit->setCurrentIndex(unitIdx >= 0 ? unitIdx : 0);

    m_knownPx = new QDoubleSpinBox(this);
    m_knownPx->setDecimals(3);
    m_knownPx->setRange(0.001, 1e9);
    m_knownPx->setValue(100.0);
    m_knownPx->setSuffix(QStringLiteral(" px"));

    m_knownMm = new QDoubleSpinBox(this);
    m_knownMm->setDecimals(6);
    m_knownMm->setRange(1e-9, 1e9);
    m_knownMm->setValue(1.0);
    m_knownMm->setSuffix(QStringLiteral(" mm"));

    auto *calcBtn = new QPushButton(tr("由已知长度计算"), this);
    connect(calcBtn, &QPushButton::clicked, this, [this]() {
        if (m_knownPx->value() > 0.0) {
            m_pixelSize->setValue(m_knownMm->value() / m_knownPx->value());
            m_enabled->setChecked(true);
            m_unit->setCurrentIndex(m_unit->findData(QStringLiteral("mm")));
        }
    });

    auto *manualForm = new QFormLayout();
    manualForm->addRow(m_enabled);
    manualForm->addRow(tr("像素当量"), m_pixelSize);
    manualForm->addRow(tr("单位"), m_unit);
    manualForm->addRow(tr("图像长度"), m_knownPx);
    manualForm->addRow(tr("实际长度"), m_knownMm);
    manualForm->addRow(calcBtn);

    auto *manualGroup = new QGroupBox(tr("手动像素当量"), this);
    manualGroup->setLayout(manualForm);

    // ---- 棋盘格标定 ----
    m_boardCols = new QSpinBox(this);
    m_boardCols->setRange(3, 50);
    m_boardCols->setValue(11);
    m_boardRows = new QSpinBox(this);
    m_boardRows->setRange(3, 50);
    m_boardRows->setValue(8);
    m_squareMm = new QDoubleSpinBox(this);
    m_squareMm->setDecimals(3);
    m_squareMm->setRange(0.001, 1e6);
    m_squareMm->setValue(25.6);
    m_squareMm->setSuffix(QStringLiteral(" mm"));

    m_folderLabel = new QLabel(tr("未选择文件夹"), this);
    m_folderLabel->setWordWrap(true);
    m_statusLabel = new QLabel(tr("请选择含棋盘格图像的文件夹（≥3 张），再开始标定。"), this);
    m_statusLabel->setWordWrap(true);

    auto *folderBtn = new QPushButton(tr("选择图像文件夹..."), this);
    connect(folderBtn, &QPushButton::clicked, this, &CalibrationDialog::chooseImageFolder);

    m_runBtn = new QPushButton(tr("开始棋盘格标定"), this);
    connect(m_runBtn, &QPushButton::clicked, this, &CalibrationDialog::runChessboardCalibration);

    auto *saveBtn = new QPushButton(tr("保存内参 XML..."), this);
    connect(saveBtn, &QPushButton::clicked, this, &CalibrationDialog::saveIntrinsicsXml);
    auto *loadBtn = new QPushButton(tr("加载内参 XML..."), this);
    connect(loadBtn, &QPushButton::clicked, this, &CalibrationDialog::loadIntrinsicsXml);

    auto *boardForm = new QFormLayout();
    boardForm->addRow(tr("内角点列数"), m_boardCols);
    boardForm->addRow(tr("内角点行数"), m_boardRows);
    boardForm->addRow(tr("方格边长"), m_squareMm);
    boardForm->addRow(folderBtn);
    boardForm->addRow(m_folderLabel);
    boardForm->addRow(m_runBtn);
    boardForm->addRow(saveBtn);
    boardForm->addRow(loadBtn);
    boardForm->addRow(m_statusLabel);

    auto *boardGroup = new QGroupBox(tr("棋盘格标定（OpenCV）"), this);
    boardGroup->setLayout(boardForm);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(manualGroup);
    layout->addWidget(boardGroup);
    layout->addWidget(buttons);
}

CalibrationConfig CalibrationDialog::result() const
{
    CalibrationConfig cfg;
    cfg.calibrated = m_enabled->isChecked();
    cfg.pixelSizeMm = m_pixelSize->value();
    cfg.unit = m_unit->currentData().toString();
    if (!cfg.calibrated) {
        cfg.unit = QStringLiteral("px");
    }
    return cfg;
}

void CalibrationDialog::chooseImageFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("选择标定图像文件夹"));
    if (folder.isEmpty()) {
        return;
    }
    m_imagePaths = listCalibImages(folder);
    m_folderLabel->setText(tr("%1\n（%2 张图像）").arg(folder).arg(m_imagePaths.size()));
    if (m_imagePaths.isEmpty()) {
        m_statusLabel->setText(tr("文件夹内未找到图像。"));
    } else {
        m_statusLabel->setText(
            tr("已选 %1 张。开始标定后，左侧列表将显示角点可视化结果（非原图）。")
                .arg(m_imagePaths.size()));
    }
}

void CalibrationDialog::emitPreview(const cv::Mat &bgr, const QString &caption)
{
    if (bgr.empty()) {
        return;
    }
    cv::Mat view = bgr;
    if (view.channels() == 1) {
        cv::cvtColor(bgr, view, cv::COLOR_GRAY2BGR);
    }
    QImage qimg = cvMat2QImage(view, true, true);
    if (qimg.isNull()) {
        return;
    }
    emit previewRequested(qimg, caption);
    QApplication::processEvents();
}

void CalibrationDialog::publishVisualList()
{
    if (m_vizImages.isEmpty()) {
        return;
    }
    emit calibrationVisualsReady(m_vizLabels, m_vizImages);
}

void CalibrationDialog::publishCornerVisuals(const std::vector<std::vector<cv::Point2f>> &imagePoints)
{
    m_vizLabels.clear();
    m_vizImages.clear();
    m_vizLabels.reserve(m_imagePaths.size() + 1);
    m_vizImages.reserve(m_imagePaths.size() + 1);

    const cv::Size board = m_calib.params().boardSize;
    for (int i = 0; i < m_imagePaths.size(); ++i) {
        const QString &path = m_imagePaths.at(i);
        cv::Mat bgr = cv::imread(path.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (bgr.empty()) {
            continue;
        }

        const bool ok = (i < static_cast<int>(imagePoints.size()) && !imagePoints[static_cast<size_t>(i)].empty());
        if (ok) {
            cv::drawChessboardCorners(bgr, board, imagePoints[static_cast<size_t>(i)], true);
            cv::putText(bgr,
                        "OK",
                        cv::Point(20, 40),
                        cv::FONT_HERSHEY_SIMPLEX,
                        1.2,
                        cv::Scalar(0, 220, 0),
                        2);
        } else {
            cv::putText(bgr,
                        "FAIL",
                        cv::Point(20, 40),
                        cv::FONT_HERSHEY_SIMPLEX,
                        1.2,
                        cv::Scalar(0, 0, 255),
                        2);
        }

        QImage qimg = cvMat2QImage(bgr, true, true);
        if (qimg.isNull()) {
            continue;
        }
        const QString name = QFileInfo(path).fileName();
        m_vizLabels.push_back(ok ? tr("%1 [OK]").arg(name) : tr("%1 [FAIL]").arg(name));
        m_vizImages.push_back(qimg);
    }

    publishVisualList();
}

QImage CalibrationDialog::makeUndistortCompareImage(const cv::Mat &src) const
{
    if (src.empty() || !m_calib.lastIntrinsics().isValid) {
        return {};
    }
    cv::Mat undist;
    m_calib.undistort(src, &undist);

    cv::Mat left = src.clone();
    cv::Mat right = undist.clone();
    if (left.channels() == 1) {
        cv::cvtColor(left, left, cv::COLOR_GRAY2BGR);
    }
    if (right.channels() == 1) {
        cv::cvtColor(right, right, cv::COLOR_GRAY2BGR);
    }

    const int font = cv::FONT_HERSHEY_SIMPLEX;
    cv::putText(left, "Original", cv::Point(20, 40), font, 1.2, cv::Scalar(0, 255, 0), 2);
    cv::putText(right, "Undistorted", cv::Point(20, 40), font, 1.2, cv::Scalar(0, 255, 0), 2);

    const int maxH = 900;
    if (left.rows > maxH) {
        const double s = static_cast<double>(maxH) / left.rows;
        cv::resize(left, left, cv::Size(), s, s, cv::INTER_AREA);
        cv::resize(right, right, cv::Size(), s, s, cv::INTER_AREA);
    }

    cv::Mat combined;
    cv::hconcat(left, right, combined);
    return cvMat2QImage(combined, true, true);
}

void CalibrationDialog::showUndistortCompare(const cv::Mat &src, const QString &caption, bool pushToList)
{
    const QImage compare = makeUndistortCompareImage(src);
    if (compare.isNull()) {
        return;
    }

    emit previewRequested(compare, caption);
    QApplication::processEvents();

    if (pushToList) {
        // 去掉旧的对比项（若重复标定）
        for (int i = m_vizLabels.size() - 1; i >= 0; --i) {
            if (m_vizLabels.at(i).contains(tr("畸变矫正对比"))) {
                m_vizLabels.removeAt(i);
                m_vizImages.removeAt(i);
            }
        }
        m_vizLabels.push_back(tr("畸变矫正对比（左原图/右矫正）"));
        m_vizImages.push_back(compare);
        publishVisualList();
    }
}

void CalibrationDialog::runChessboardCalibration()
{
    if (m_imagePaths.size() < 3) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择至少包含 3 张图像的文件夹。"));
        return;
    }

    CalibrateParams params;
    params.boardSize = cv::Size(m_boardCols->value(), m_boardRows->value());
    params.squareSize = static_cast<float>(m_squareMm->value());
    m_calib.clearData();
    m_calib.setParams(params);

    m_calib.setCornerDetectedCallback([this](int current, int total, const cv::Mat &image) {
        emitPreview(image, tr("角点检测 %1 / %2（成功帧）").arg(current).arg(total));
        QThread::msleep(150);
    });

    m_runBtn->setEnabled(false);
    m_statusLabel->setText(tr("正在检测角点，主图同步显示…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    std::vector<cv::String> paths;
    paths.reserve(static_cast<size_t>(m_imagePaths.size()));
    for (const QString &p : m_imagePaths) {
        paths.push_back(p.toLocal8Bit().constData());
    }

    std::vector<std::vector<cv::Point2f>> imagePoints;
    cv::Size imageSize;
    const bool cornersOk = m_calib.detectCorners(paths, &imagePoints, &imageSize);

    // 无论成败，左侧列表都换成角点可视化（非原图），便于核对
    publishCornerVisuals(imagePoints);

    if (!cornersOk) {
        QApplication::restoreOverrideCursor();
        m_runBtn->setEnabled(true);
        m_statusLabel->setText(tr("角点检测失败：有效图像不足 3 张，请检查棋盘尺寸与图像。"));
        QMessageBox::warning(this, tr("标定失败"), m_statusLabel->text());
        return;
    }

    m_statusLabel->setText(tr("角点结果已填入左侧列表，正在计算内参…"));
    QApplication::processEvents();

    const auto objSingle = m_calib.generateObjectPoints();
    std::vector<std::vector<cv::Point3f>> objectPoints(imagePoints.size(), objSingle);
    CalibrateResult intrinsics;
    if (!m_calib.calibrateIntrinsics(objectPoints, imagePoints, imageSize, &intrinsics)) {
        QApplication::restoreOverrideCursor();
        m_runBtn->setEnabled(true);
        m_statusLabel->setText(tr("内参标定失败。"));
        QMessageBox::warning(this, tr("标定失败"), m_statusLabel->text());
        return;
    }
    m_intrinsics = intrinsics;

    // 用第一张有效图做外参，得到 mm/px，并在主图显示去畸变对比
    ExtrinsicsResult extrinsics;
    bool extrinsicsOk = false;
    cv::Mat previewSrc;
    for (const QString &path : m_imagePaths) {
        const cv::Mat img = cv::imread(path.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (img.empty()) {
            continue;
        }
        if (previewSrc.empty()) {
            previewSrc = img;
        }
        if (m_calib.calibrateExtrinsics(img, &extrinsics) && extrinsics.isValid) {
            extrinsicsOk = true;
            previewSrc = img;
            break;
        }
    }

    if (!previewSrc.empty()) {
        showUndistortCompare(
            previewSrc,
            tr("标定结果预览：左原图 / 右去畸变 | 重投影误差 %1 px")
                .arg(intrinsics.reprojectionError, 0, 'f', 4),
            true);
    }

    QApplication::restoreOverrideCursor();
    m_runBtn->setEnabled(true);

    QString msg = tr("内参标定成功。\n重投影误差: %1 px\n使用图像: %2\n质量: %3")
                      .arg(intrinsics.reprojectionError, 0, 'f', 4)
                      .arg(intrinsics.numImagesUsed)
                      .arg(QString::fromStdString(
                          CameraCalibration::qualityDescription(intrinsics.reprojectionError)));

    if (extrinsicsOk) {
        applyExtrinsicsToPixelSize();
        msg += tr("\n\n外参成功：距离 %1 mm\n已写入像素当量 %2 mm/px")
                   .arg(extrinsics.distance, 0, 'f', 3)
                   .arg(extrinsics.mmPerPixel, 0, 'g', 6);
        m_statusLabel->setText(tr("标定完成，主图为去畸变对比，像素当量已更新。"));
    } else {
        msg += tr("\n\n外参未成功（未写入 mm/px）。可稍后加载内参后对单张棋盘图重试，或手动填写像素当量。");
        m_statusLabel->setText(tr("内参完成，外参未成功；主图已显示去畸变对比。"));
    }

    QMessageBox::information(this, tr("标定结果"), msg);
}

void CalibrationDialog::applyExtrinsicsToPixelSize()
{
    const ExtrinsicsResult ex = m_calib.lastExtrinsics();
    if (!ex.isValid || ex.mmPerPixel <= 0.0) {
        return;
    }
    m_pixelSize->setValue(ex.mmPerPixel);
    m_enabled->setChecked(true);
    m_unit->setCurrentIndex(m_unit->findData(QStringLiteral("mm")));
}

void CalibrationDialog::saveIntrinsicsXml()
{
    if (!m_intrinsics.isValid && !m_calib.lastIntrinsics().isValid) {
        QMessageBox::warning(this, tr("提示"), tr("没有可保存的内参结果。"));
        return;
    }
    const CalibrateResult &r = m_intrinsics.isValid ? m_intrinsics : m_calib.lastIntrinsics();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存内参"), QStringLiteral("camera_calib.xml"), tr("XML (*.xml)"));
    if (path.isEmpty()) {
        return;
    }
    if (m_calib.saveIntrinsics(path.toLocal8Bit().constData(), r)) {
        QMessageBox::information(this, tr("完成"), tr("内参已保存。"));
    } else {
        QMessageBox::warning(this, tr("失败"), tr("保存内参失败。"));
    }
}

void CalibrationDialog::loadIntrinsicsXml()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("加载内参"), QString(), tr("XML (*.xml)"));
    if (path.isEmpty()) {
        return;
    }
    CalibrateResult r;
    if (!m_calib.loadIntrinsics(path.toLocal8Bit().constData(), &r)) {
        QMessageBox::warning(this, tr("失败"), tr("加载内参失败。"));
        return;
    }
    m_intrinsics = r;
    m_boardCols->setValue(m_calib.params().boardSize.width);
    m_boardRows->setValue(m_calib.params().boardSize.height);
    m_squareMm->setValue(m_calib.params().squareSize);

    // 可选：选一张图算外参并写入当量，同时主图显示去畸变对比
    const QString imgPath = QFileDialog::getOpenFileName(
        this,
        tr("选择一张棋盘格图像以计算像素当量（可取消）"),
        QString(),
        tr("图像 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (!imgPath.isEmpty()) {
        const cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (!img.empty()) {
            m_vizLabels.clear();
            m_vizImages.clear();
            showUndistortCompare(img, tr("已加载内参 — 左原图 / 右去畸变"), true);
        }
        ExtrinsicsResult ex;
        if (m_calib.calibrateExtrinsics(img, &ex) && ex.isValid) {
            applyExtrinsicsToPixelSize();
            m_statusLabel->setText(tr("已加载内参并更新像素当量 %1 mm/px")
                                       .arg(ex.mmPerPixel, 0, 'g', 6));
            QMessageBox::information(this,
                                     tr("完成"),
                                     tr("内参已加载。\n像素当量: %1 mm/px").arg(ex.mmPerPixel, 0, 'g', 6));
            return;
        }
        QMessageBox::warning(this, tr("提示"), tr("内参已加载，但外参计算失败（未更新 mm/px）。"));
    } else {
        m_statusLabel->setText(tr("内参已加载（未计算外参）。"));
        QMessageBox::information(this, tr("完成"), tr("内参已加载。"));
    }
}

} // namespace Me
