#ifndef CAMERACALIBRATION_H
#define CAMERACALIBRATION_H

/**
 * @file CameraCalibration.h
 * @brief 棋盘格相机内参/外参标定（移植自 GdCamExpert_Li::CCameraCalibration）。
 *
 * 内参：多图 findChessboardCorners + calibrateCamera
 * 外参：单图 solvePnP → distance / fx → mmPerPixel（写入工程像素当量）
 */

#include <functional>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace Me {

struct CalibrateParams {
    cv::Size boardSize = cv::Size(11, 8); ///< 内角点列×行
    float squareSize = 25.6f;             ///< 方格边长 mm
    int maxProcessSize = 1200;            ///< 角点检测时最长边缩放上限
};

struct CalibrateResult {
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    cv::Size imageSize;
    double reprojectionError = 0.0;
    int numImagesUsed = 0;
    bool isValid = false;
};

struct ExtrinsicsResult {
    cv::Mat rvec;
    cv::Mat tvec;
    double distance = 0.0;   ///< 相机到标定板距离 mm
    double mmPerPixel = 0.0; ///< distance / fx
    bool isValid = false;
};

class CameraCalibration
{
public:
    using CornerDetectedCallback =
        std::function<void(int current, int total, const cv::Mat &image)>;

    void setParams(const CalibrateParams &params);
    CalibrateParams params() const { return m_params; }

    void clearData();

    std::vector<cv::Point3f> generateObjectPoints() const;

    /** 从文件路径检测角点；成功需至少 3 张有效图 */
    bool detectCorners(const std::vector<cv::String> &imagePaths,
                       std::vector<std::vector<cv::Point2f>> *imagePoints,
                       cv::Size *imageSize);

    /** 从内存图像检测角点（避免写临时文件） */
    bool detectCornersFromImages(const std::vector<cv::Mat> &images,
                                 std::vector<std::vector<cv::Point2f>> *imagePoints,
                                 cv::Size *imageSize);

    bool calibrateIntrinsics(const std::vector<std::vector<cv::Point3f>> &objectPoints,
                             const std::vector<std::vector<cv::Point2f>> &imagePoints,
                             const cv::Size &imageSize,
                             CalibrateResult *result);

    /** 需已有有效内参；输出 mmPerPixel */
    bool calibrateExtrinsics(const cv::Mat &image, ExtrinsicsResult *result);

    cv::Mat undistort(const cv::Mat &src) const;
    void undistort(const cv::Mat &src, cv::Mat *dst) const;

    bool saveIntrinsics(const std::string &filePath, const CalibrateResult &result) const;
    bool loadIntrinsics(const std::string &filePath, CalibrateResult *result);

    bool saveExtrinsics(const std::string &filePath, const ExtrinsicsResult &result) const;
    bool loadExtrinsics(const std::string &filePath, ExtrinsicsResult *result);

    CalibrateResult lastIntrinsics() const { return m_intrinsicsResult; }
    ExtrinsicsResult lastExtrinsics() const { return m_extrinsicsResult; }

    void setCornerDetectedCallback(CornerDetectedCallback cb) { m_cornerCallback = std::move(cb); }

    static std::string qualityDescription(double reprojectionError);

private:
    bool detectChessboardCorners(const cv::Mat &gray, std::vector<cv::Point2f> *corners);
    cv::Mat preprocessImage(const cv::Mat &gray) const;

    CalibrateParams m_params;
    CalibrateResult m_intrinsicsResult;
    ExtrinsicsResult m_extrinsicsResult;
    CornerDetectedCallback m_cornerCallback;
};

} // namespace Me

#endif // CAMERACALIBRATION_H
