/**
 * @file AlignmentService.cpp
 * @brief AlignmentService 实现：QImage→HImage、CreateShapeModel、FindShapeModel。
 */

#include "Alg/AlignmentService.h"

#include "Alg/HalconUtils.h"
#include "ImageConvert.h"

#include <QDir>
#include <QFileInfo>
#include <QtMath>

#include "HalconCpp.h"

using namespace HalconCpp;

namespace Me {
namespace {

constexpr Hlong kMaxContours = 256;
constexpr Hlong kMaxPointsPerContour = 20000;

/** 相对路径基于 projectDir 解析，绝对路径原样返回 */
QString resolveModelPath(const QString &modelFile, const QString &projectDir)
{
    if (modelFile.isEmpty()) {
        return {};
    }
    const QFileInfo info(modelFile);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    if (!projectDir.isEmpty()) {
        return QDir(projectDir).filePath(modelFile);
    }
    return QFileInfo(modelFile).absoluteFilePath();
}

/** 转灰度 HImage；彩色图经 Rgb1ToGray */
HImage toGrayHImage(QImage image, QString *error)
{
    if (image.isNull()) {
        if (error) {
            *error = QStringLiteral("图像为空");
        }
        return {};
    }

    if (image.format() != QImage::Format_Grayscale8
        && image.format() != QImage::Format_Indexed8) {
        image = image.convertToFormat(QImage::Format_RGB888);
    }

    HImage color;
    if (!QImage2HImage(image, color)) {
        if (error) {
            *error = QStringLiteral("QImage 转 HImage 失败");
        }
        return {};
    }

    if (color.CountChannels().I() == 1) {
        return color;
    }
    return color.Rgb1ToGray();
}

/** 失败或未匹配时 homMat 设为单位阵，便于下游跳过变换 */
void fillIdentity(AlignmentResult &r)
{
    r.homMat2D[0] = 1;
    r.homMat2D[1] = 0;
    r.homMat2D[2] = 0;
    r.homMat2D[3] = 0;
    r.homMat2D[4] = 1;
    r.homMat2D[5] = 0;
}

/**
 * 将形状模型轮廓仿射到 (row, column, angleRad) 姿态，供 UI 叠加。
 * 轮廓点数过多时按 step 降采样，并限制轮廓条数。
 */
QVector<Contour2D> extractContoursAtPose(const HTuple &modelId,
                                         double row,
                                         double column,
                                         double angleRad)
{
    QVector<Contour2D> out;
    try {
        HObject modelContours;
        GetShapeModelContours(&modelContours, modelId, 1);

        HTuple homMat;
        VectorAngleToRigid(0.0, 0.0, 0.0, row, column, angleRad, &homMat);

        HObject transContours;
        AffineTransContourXld(modelContours, &transContours, homMat);

        HTuple count;
        CountObj(transContours, &count);
        if (count.Length() < 1) {
            return out;
        }
        const Hlong nObj = count[0].L();
        if (nObj <= 0 || nObj > kMaxContours * 20) {
            return out;
        }

        const Hlong nUse = qMin(nObj, kMaxContours);
        out.reserve(static_cast<int>(nUse));

        for (Hlong i = 1; i <= nUse; ++i) {
            HObject one;
            SelectObj(transContours, &one, i);
            HTuple rows;
            HTuple cols;
            GetContourXld(one, &rows, &cols);

            const Hlong nPts = qMin(rows.Length(), cols.Length());
            if (nPts < 2 || nPts > 2000000) {
                continue;
            }

            Contour2D contour;
            const Hlong step = (nPts > kMaxPointsPerContour)
                ? (nPts / kMaxPointsPerContour)
                : 1;
            contour.points.reserve(static_cast<int>(nPts / step + 2));
            for (Hlong j = 0; j < nPts; j += step) {
                Point2D pt;
                pt.row = rows[j].D();
                pt.col = cols[j].D();
                if (!qIsFinite(pt.row) || !qIsFinite(pt.col)) {
                    continue;
                }
                contour.points.push_back(pt);
            }
            // 降采样时补最后一个点，避免轮廓末端被截断
            if (step > 1 && nPts > 1) {
                Point2D last;
                last.row = rows[nPts - 1].D();
                last.col = cols[nPts - 1].D();
                if (qIsFinite(last.row) && qIsFinite(last.col)) {
                    contour.points.push_back(last);
                }
            }
            if (contour.points.size() >= 2) {
                out.push_back(std::move(contour));
            }
        }
    } catch (HException &) {
        out.clear();
    } catch (...) {
        out.clear();
    }
    return out;
}

} // namespace

// --- createModel：ReduceDomain + CreateShapeModel + WriteShapeModel ---

bool AlignmentService::createModel(const QImage &image,
                                   const SearchRegion &modelRoi,
                                   const QString &modelPath,
                                   double angleStartDeg,
                                   double angleExtentDeg,
                                   QString *error,
                                   QVector<Contour2D> *previewContours)
{
    try {
        HImage gray = toGrayHImage(image, error);
        if (!gray.IsInitialized()) {
            return false;
        }

        Hlong width = 0;
        Hlong height = 0;
        gray.GetImageSize(&width, &height);

        double r1 = modelRoi.row1;
        double c1 = modelRoi.col1;
        double r2 = modelRoi.row2;
        double c2 = modelRoi.col2;
        // row2/col2<0 时使用图像中心 60% 区域作为默认模板 ROI
        if (r2 < 0 || c2 < 0) {
            r1 = height * 0.2;
            c1 = width * 0.2;
            r2 = height * 0.8;
            c2 = width * 0.8;
        }
        r1 = qBound(0.0, r1, static_cast<double>(height - 2));
        c1 = qBound(0.0, c1, static_cast<double>(width - 2));
        r2 = qBound(r1 + 1.0, r2, static_cast<double>(height - 1));
        c2 = qBound(c1 + 1.0, c2, static_cast<double>(width - 1));

        HRegion roi;
        roi.GenRectangle1(r1, c1, r2, c2);
        HImage reduced = gray.ReduceDomain(roi);

        // 训练角度范围必须覆盖序列中可能出现的姿态。
        // 旧实现写死 -45°~+45°，board_03 等大角度图会全部找不到。
        double extentDeg = angleExtentDeg;
        if (extentDeg < 1.0) {
            extentDeg = 360.0;
        }
        if (extentDeg > 360.0) {
            extentDeg = 360.0;
        }

        HTuple modelId;
        CreateShapeModel(reduced,
                         "auto",
                         HTuple(angleStartDeg).TupleRad(),
                         HTuple(extentDeg).TupleRad(),
                         "auto",
                         "auto",
                         "use_polarity",
                         "auto",
                         "auto",
                         &modelId);

        if (previewContours) {
            const double centerRow = 0.5 * (r1 + r2);
            const double centerCol = 0.5 * (c1 + c2);
            *previewContours = extractContoursAtPose(modelId, centerRow, centerCol, 0.0);
        }

        QDir().mkpath(QFileInfo(modelPath).absolutePath());
        const QByteArray pathLocal = modelPath.toLocal8Bit();
        WriteShapeModel(modelId, pathLocal.constData());
        ClearShapeModel(modelId);
        return true;
    } catch (HException &ex) {
        if (error) {
            *error = fromHException(ex);
        }
        return false;
    }
}

// --- find：可选搜索 ROI + FindShapeModel + HomMat2D ---

AlignmentResult AlignmentService::find(const QImage &image,
                                       const AlignmentConfig &config,
                                       const QString &projectDir,
                                       bool extractContours)
{
    AlignmentResult result;
    fillIdentity(result);

    const QString modelPath = resolveModelPath(config.modelFile, projectDir);
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        result.message = QStringLiteral("定位模型不存在，请先创建定位模板");
        return result;
    }

    HTuple modelId;
    bool modelLoaded = false;
    try {
        QString convError;
        HImage gray = toGrayHImage(image, &convError);
        if (!gray.IsInitialized()) {
            result.message = convError;
            return result;
        }

        HImage searchImage = gray;
        if (config.searchRegion.row2 >= 0 && config.searchRegion.col2 >= 0) {
            HRegion searchRoi;
            searchRoi.GenRectangle1(config.searchRegion.row1,
                                    config.searchRegion.col1,
                                    config.searchRegion.row2,
                                    config.searchRegion.col2);
            searchImage = gray.ReduceDomain(searchRoi);
        }

        const QByteArray modelPathLocal = modelPath.toLocal8Bit();
        ReadShapeModel(modelPathLocal.constData(), &modelId);
        modelLoaded = true;

        const double angleStart = qDegreesToRadians(config.angleStartDeg);
        const double angleExtent = qDegreesToRadians(config.angleExtentDeg);

        HTuple row, column, angle, score;
        FindShapeModel(searchImage,
                       modelId,
                       angleStart,
                       angleExtent,
                       config.minScore,
                       1,
                       0.5,
                       "least_squares",
                       0,
                       0.75,
                       &row,
                       &column,
                       &angle,
                       &score);

        if (row.Length() < 1) {
            ClearShapeModel(modelId);
            modelLoaded = false;
            result.message = QStringLiteral("定位失败：未找到形状模型");
            return result;
        }

        result.ok = true;
        result.row = row[0].D();
        result.column = column[0].D();
        result.angleRad = angle[0].D();
        result.score = score[0].D();
        result.message = QStringLiteral("定位成功 score=%1").arg(result.score, 0, 'f', 3);

        HTuple homMat;
        VectorAngleToRigid(0.0, 0.0, 0.0, result.row, result.column, result.angleRad, &homMat);
        for (int i = 0; i < 6 && i < homMat.Length(); ++i) {
            result.homMat2D[i] = homMat[i].D();
        }

        if (extractContours) {
            result.modelContours =
                extractContoursAtPose(modelId, result.row, result.column, result.angleRad);
        }

        ClearShapeModel(modelId);
        modelLoaded = false;
        return result;
    } catch (HException &ex) {
        if (modelLoaded) {
            try {
                ClearShapeModel(modelId);
            } catch (...) {
            }
        }
        result.ok = false;
        result.modelContours.clear();
        result.message = fromHException(ex);
        return result;
    }
}

} // namespace Me
