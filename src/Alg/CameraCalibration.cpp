/**
 * @file CameraCalibration.cpp
 * @brief 棋盘格标定实现（逻辑对齐 GdCamExpert_Li::CCameraCalibration）。
 */

#include "Alg/CameraCalibration.h"

#include <QDebug>
#include <QString>

namespace Me {

void CameraCalibration::setParams(const CalibrateParams &params)
{
    m_params = params;
}

void CameraCalibration::clearData()
{
    m_intrinsicsResult = CalibrateResult();
    m_extrinsicsResult = ExtrinsicsResult();
}

std::vector<cv::Point3f> CameraCalibration::generateObjectPoints() const
{
    std::vector<cv::Point3f> worldCorners;
    worldCorners.reserve(static_cast<size_t>(m_params.boardSize.width * m_params.boardSize.height));
    for (int i = 0; i < m_params.boardSize.height; ++i) {
        for (int j = 0; j < m_params.boardSize.width; ++j) {
            worldCorners.emplace_back(j * m_params.squareSize, i * m_params.squareSize, 0.0f);
        }
    }
    return worldCorners;
}

cv::Mat CameraCalibration::preprocessImage(const cv::Mat &gray) const
{
    cv::Mat processed;
    if (std::max(gray.rows, gray.cols) > m_params.maxProcessSize) {
        const double scale = m_params.maxProcessSize / static_cast<double>(std::max(gray.rows, gray.cols));
        cv::resize(gray, processed, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        processed = gray.clone();
    }

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(processed, processed);
    return processed;
}

bool CameraCalibration::detectChessboardCorners(const cv::Mat &gray, std::vector<cv::Point2f> *corners)
{
    if (!corners) {
        return false;
    }
    corners->clear();

    cv::Mat grayFull;
    if (gray.channels() == 3) {
        cv::cvtColor(gray, grayFull, cv::COLOR_BGR2GRAY);
    } else if (gray.channels() == 4) {
        cv::cvtColor(gray, grayFull, cv::COLOR_BGRA2GRAY);
    } else {
        grayFull = gray;
    }

    const cv::Mat processed = preprocessImage(grayFull);

    bool found = cv::findChessboardCorners(
        processed,
        m_params.boardSize,
        *corners,
        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FAST_CHECK | cv::CALIB_CB_NORMALIZE_IMAGE);

    if (!found) {
        found = cv::findChessboardCorners(
            processed,
            m_params.boardSize,
            *corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
    }

    if (!found) {
        return false;
    }

    const double scale = static_cast<double>(grayFull.cols) / processed.cols;
    if (std::abs(scale - 1.0) > 1e-9) {
        for (auto &corner : *corners) {
            corner.x = static_cast<float>(corner.x * scale);
            corner.y = static_cast<float>(corner.y * scale);
        }
    }

    const cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001);
    cv::cornerSubPix(grayFull, *corners, cv::Size(11, 11), cv::Size(-1, -1), criteria);
    return true;
}

bool CameraCalibration::detectCornersFromImages(const std::vector<cv::Mat> &images,
                                                std::vector<std::vector<cv::Point2f>> *imagePoints,
                                                cv::Size *imageSize)
{
    if (!imagePoints || !imageSize) {
        return false;
    }
    imagePoints->clear();
    *imageSize = cv::Size(0, 0);

    qInfo().noquote() << QStringLiteral("[角点检测] 棋盘 %1×%2, 方格 %3 mm, 图像 %4 张")
                             .arg(m_params.boardSize.width)
                             .arg(m_params.boardSize.height)
                             .arg(m_params.squareSize)
                             .arg(images.size());

    int successCount = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        const cv::Mat &colorImg = images[i];
        if (colorImg.empty()) {
            imagePoints->push_back({});
            continue;
        }

        if (imageSize->width == 0) {
            *imageSize = colorImg.size();
        }

        cv::Mat grayFull;
        if (colorImg.channels() == 1) {
            grayFull = colorImg;
        } else if (colorImg.channels() == 4) {
            cv::cvtColor(colorImg, grayFull, cv::COLOR_BGRA2GRAY);
        } else {
            cv::cvtColor(colorImg, grayFull, cv::COLOR_BGR2GRAY);
        }

        std::vector<cv::Point2f> corners;
        if (detectChessboardCorners(grayFull, &corners)) {
            cv::Mat displayImg;
            if (colorImg.channels() == 1) {
                cv::cvtColor(colorImg, displayImg, cv::COLOR_GRAY2BGR);
            } else {
                displayImg = colorImg.clone();
            }
            cv::drawChessboardCorners(displayImg, m_params.boardSize, corners, true);
            if (m_cornerCallback) {
                m_cornerCallback(static_cast<int>(i + 1), static_cast<int>(images.size()), displayImg);
            }
            imagePoints->push_back(corners);
            ++successCount;
            qInfo().noquote() << QStringLiteral("[角点检测] %1/%2 成功")
                                     .arg(i + 1)
                                     .arg(images.size());
        } else {
            imagePoints->push_back({});
            qWarning().noquote() << QStringLiteral("[角点检测] %1/%2 失败")
                                        .arg(i + 1)
                                        .arg(images.size());
        }
    }

    qInfo().noquote() << QStringLiteral("[角点检测] 成功 %1 / %2")
                             .arg(successCount)
                             .arg(images.size());
    return successCount >= 3;
}

bool CameraCalibration::detectCorners(const std::vector<cv::String> &imagePaths,
                                      std::vector<std::vector<cv::Point2f>> *imagePoints,
                                      cv::Size *imageSize)
{
    std::vector<cv::Mat> images;
    images.reserve(imagePaths.size());
    for (const auto &path : imagePaths) {
        images.push_back(cv::imread(path, cv::IMREAD_COLOR));
    }
    return detectCornersFromImages(images, imagePoints, imageSize);
}

bool CameraCalibration::calibrateIntrinsics(const std::vector<std::vector<cv::Point3f>> &objectPoints,
                                            const std::vector<std::vector<cv::Point2f>> &imagePoints,
                                            const cv::Size &imageSize,
                                            CalibrateResult *result)
{
    if (!result || imageSize.width <= 0 || objectPoints.empty() || imagePoints.empty()) {
        qWarning().noquote() << QStringLiteral("[内参标定] 输入无效");
        return false;
    }

    std::vector<std::vector<cv::Point2f>> validImagePoints;
    std::vector<std::vector<cv::Point3f>> validObjectPoints;
    for (size_t i = 0; i < imagePoints.size(); ++i) {
        if (imagePoints[i].empty()) {
            continue;
        }
        validImagePoints.push_back(imagePoints[i]);
        validObjectPoints.push_back(objectPoints[0]);
    }

    if (validImagePoints.size() < 3) {
        qWarning().noquote() << QStringLiteral("[内参标定] 有效图像不足: %1").arg(validImagePoints.size());
        return false;
    }

    std::vector<cv::Mat> rvecs;
    std::vector<cv::Mat> tvecs;
    int flags = 0;
    flags |= cv::CALIB_FIX_K3;
    flags |= cv::CALIB_FIX_K4;
    flags |= cv::CALIB_FIX_K5;

    result->reprojectionError = cv::calibrateCamera(validObjectPoints,
                                                    validImagePoints,
                                                    imageSize,
                                                    result->cameraMatrix,
                                                    result->distCoeffs,
                                                    rvecs,
                                                    tvecs,
                                                    flags);
    result->imageSize = imageSize;
    result->numImagesUsed = static_cast<int>(validImagePoints.size());
    result->isValid = true;
    m_intrinsicsResult = *result;

    qInfo().noquote() << QStringLiteral("[内参标定] 重投影误差=%1 px, 图像数=%2, 质量=%3")
                             .arg(result->reprojectionError, 0, 'f', 4)
                             .arg(result->numImagesUsed)
                             .arg(QString::fromStdString(qualityDescription(result->reprojectionError)));
    return true;
}

bool CameraCalibration::calibrateExtrinsics(const cv::Mat &image, ExtrinsicsResult *result)
{
    if (!result) {
        return false;
    }
    *result = ExtrinsicsResult();

    if (!m_intrinsicsResult.isValid) {
        qWarning().noquote() << QStringLiteral("[外参标定] 内参未标定");
        return false;
    }

    std::vector<cv::Point2f> corners;
    if (!detectChessboardCorners(image, &corners)) {
        qWarning().noquote() << QStringLiteral("[外参标定] 角点检测失败");
        return false;
    }

    const std::vector<cv::Point3f> objectPoints = generateObjectPoints();
    if (!cv::solvePnP(objectPoints,
                      corners,
                      m_intrinsicsResult.cameraMatrix,
                      m_intrinsicsResult.distCoeffs,
                      result->rvec,
                      result->tvec)) {
        qWarning().noquote() << QStringLiteral("[外参标定] solvePnP 失败");
        return false;
    }

    result->distance = cv::norm(result->tvec);
    const double fx = m_intrinsicsResult.cameraMatrix.at<double>(0, 0);
    if (fx <= 1e-9) {
        return false;
    }
    result->mmPerPixel = result->distance / fx;
    result->isValid = true;
    m_extrinsicsResult = *result;

    qInfo().noquote() << QStringLiteral("[外参标定] 距离=%1 mm, mm/px=%2")
                             .arg(result->distance, 0, 'f', 3)
                             .arg(result->mmPerPixel, 0, 'g', 6);
    return true;
}

cv::Mat CameraCalibration::undistort(const cv::Mat &src) const
{
    cv::Mat dst;
    undistort(src, &dst);
    return dst;
}

void CameraCalibration::undistort(const cv::Mat &src, cv::Mat *dst) const
{
    if (!dst) {
        return;
    }
    if (!m_intrinsicsResult.isValid) {
        *dst = src;
        return;
    }
    cv::undistort(src, *dst, m_intrinsicsResult.cameraMatrix, m_intrinsicsResult.distCoeffs);
}

bool CameraCalibration::saveIntrinsics(const std::string &filePath, const CalibrateResult &result) const
{
    if (!result.isValid) {
        return false;
    }
    cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        return false;
    }
    fs << "cameraMatrix" << result.cameraMatrix;
    fs << "distCoeffs" << result.distCoeffs;
    fs << "imageWidth" << result.imageSize.width;
    fs << "imageHeight" << result.imageSize.height;
    fs << "reprojectionError" << result.reprojectionError;
    fs << "boardWidth" << m_params.boardSize.width;
    fs << "boardHeight" << m_params.boardSize.height;
    fs << "squareSize" << m_params.squareSize;
    fs << "numImagesUsed" << result.numImagesUsed;
    fs.release();
    return true;
}

bool CameraCalibration::loadIntrinsics(const std::string &filePath, CalibrateResult *result)
{
    if (!result) {
        return false;
    }
    cv::FileStorage fs(filePath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }
    fs["cameraMatrix"] >> result->cameraMatrix;
    fs["distCoeffs"] >> result->distCoeffs;
    int w = 0;
    int h = 0;
    fs["imageWidth"] >> w;
    fs["imageHeight"] >> h;
    result->imageSize = cv::Size(w, h);
    fs["reprojectionError"] >> result->reprojectionError;
    fs["numImagesUsed"] >> result->numImagesUsed;
    int bw = 0;
    int bh = 0;
    fs["boardWidth"] >> bw;
    fs["boardHeight"] >> bh;
    m_params.boardSize = cv::Size(bw, bh);
    float ss = 0.0f;
    fs["squareSize"] >> ss;
    m_params.squareSize = ss;
    fs.release();

    result->isValid = !result->cameraMatrix.empty() && !result->distCoeffs.empty();
    if (result->isValid) {
        m_intrinsicsResult = *result;
    }
    return result->isValid;
}

bool CameraCalibration::saveExtrinsics(const std::string &filePath, const ExtrinsicsResult &result) const
{
    if (!result.isValid) {
        return false;
    }
    cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        return false;
    }
    fs << "rvec" << result.rvec;
    fs << "tvec" << result.tvec;
    fs << "distance" << result.distance;
    fs << "mmPerPixel" << result.mmPerPixel;
    fs.release();
    return true;
}

bool CameraCalibration::loadExtrinsics(const std::string &filePath, ExtrinsicsResult *result)
{
    if (!result) {
        return false;
    }
    cv::FileStorage fs(filePath, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        return false;
    }
    fs["rvec"] >> result->rvec;
    fs["tvec"] >> result->tvec;
    fs["distance"] >> result->distance;
    fs["mmPerPixel"] >> result->mmPerPixel;
    fs.release();
    result->isValid = !result->rvec.empty() && !result->tvec.empty();
    if (result->isValid) {
        m_extrinsicsResult = *result;
    }
    return result->isValid;
}

std::string CameraCalibration::qualityDescription(double reprojectionError)
{
    if (reprojectionError < 0.5) {
        return "优秀 (误差 < 0.5像素)";
    }
    if (reprojectionError < 1.0) {
        return "良好 (误差 < 1.0像素)";
    }
    if (reprojectionError < 2.0) {
        return "一般 (误差 < 2.0像素)";
    }
    return "较差 (误差 > 2.0像素)";
}

} // namespace Me
