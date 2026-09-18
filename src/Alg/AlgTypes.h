#ifndef ALGTYPES_H
#define ALGTYPES_H

#include "Core/ProjectTypes.h"

#include <QMetaType>
#include <QString>
#include <QVector>

namespace Me {

struct Point2D {
    double row = 0.0;
    double col = 0.0;
};

struct Contour2D {
    QVector<Point2D> points;
};

struct AlignmentResult {
    bool ok = false;
    QString message;
    double row = 0.0;
    double column = 0.0;
    double angleRad = 0.0;
    double score = 0.0;
    // HomMat2D: row-major 2x3 [a11 a12 a13; a21 a22 a23]
    double homMat2D[6] = {1, 0, 0, 0, 1, 0};
    // 变换到图像坐标的模型轮廓（例程式 XLD 叠加）
    QVector<Contour2D> modelContours;
};

struct MeasureResult {
    QString id;
    QString name;
    MeasureType type = MeasureType::CaliperWidth;
    bool ok = false;
    bool measured = false;
    double value = 0.0;
    double nominal = 0.0;
    double upper = 0.0;
    double lower = 0.0;
    QString unit = QStringLiteral("px");
    int decimals = 3;
    QString message;
    // 图像坐标系卡尺
    double imageRow = 0.0;
    double imageCol = 0.0;
    double imagePhi = 0.0;
    double length1 = 0.0;
    double length2 = 0.0;
    // measure_pairs 边缘点（图像坐标）
    QVector<Point2D> firstEdges;
    QVector<Point2D> secondEdges;
    int usedPairIndex = -1;
};

struct CycleResult {
    bool alignmentOk = false;
    bool totalOk = false;
    QString reason;
    AlignmentResult alignment;
    QVector<MeasureResult> measures;
    int elapsedMs = 0;
};

} // namespace Me

Q_DECLARE_METATYPE(Me::MeasureResult)
Q_DECLARE_METATYPE(QVector<Me::MeasureResult>)
Q_DECLARE_METATYPE(Me::CycleResult)
Q_DECLARE_METATYPE(Me::AlignmentResult)

#endif // ALGTYPES_H
