#ifndef ALGTYPES_H
#define ALGTYPES_H

/**
 * @file AlgTypes.h
 * @brief 算法层运行时类型：几何 primitive、定位/测量结果、单次周期汇总。
 *
 * 与 ProjectTypes（持久化配置）分离：RunEngine 将 Recipe 转为 CycleResult 供 UI 与 CSV 使用。
 * 坐标系均为 HALCON 图像坐标（row 向下，col 向右）。
 */

#include "Core/ProjectTypes.h"

#include <QMetaType>
#include <QString>
#include <QVector>

namespace Me {

/** 二维点（图像 row/col） */
struct Point2D {
    double row = 0.0;
    double col = 0.0;
};

/** 折线轮廓，用于模型 XLD 叠加显示 */
struct Contour2D {
    QVector<Point2D> points;
};

/** 形状匹配定位结果 */
struct AlignmentResult {
    bool ok = false;
    QString message;
    double row = 0.0;       ///< 匹配中心 row
    double column = 0.0;    ///< 匹配中心 col
    double angleRad = 0.0;  ///< 相对模板旋转（rad）
    double score = 0.0;
    // HomMat2D: row-major 2x3 [a11 a12 a13; a21 a22 a23]
    double homMat2D[6] = {1, 0, 0, 0, 1, 0};
    // 变换到图像坐标的模型轮廓（例程式 XLD 叠加）
    QVector<Contour2D> modelContours;
};

/** 直线几何（HALCON line 参数 + 端点） */
struct LineGeom {
    bool valid = false;
    double rowBegin = 0.0;
    double colBegin = 0.0;
    double rowEnd = 0.0;
    double colEnd = 0.0;
    double nr = 0.0;   ///< 单位法向 row 分量
    double nc = 0.0;   ///< 单位法向 col 分量
    double dist = 0.0; ///< 原点到直线有符号距离（法向形式）
};

/** 圆几何 */
struct CircleGeom {
    bool valid = false;
    double row = 0.0;
    double col = 0.0;
    double radius = 0.0;
};

/**
 * 单条测量步骤的运行结果（含公差判定与可视化数据）。
 *
 * measured=false 表示算法未产出数值（如找线失败）；
 * ok 由 ToleranceService 根据 nominal/upper/lower 判定。
 */
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
    // 图像坐标系卡尺 / 圆弧中心（ResultOverlay 绘制用）
    double imageRow = 0.0;
    double imageCol = 0.0;
    double imagePhi = 0.0;
    double length1 = 0.0;
    double length2 = 0.0;
    double angleExtent = 0.0;
    // measure_pairs / measure_pos 边缘点（图像坐标）
    QVector<Point2D> firstEdges;
    QVector<Point2D> secondEdges;
    int usedPairIndex = -1;
    // 统一几何结果（找线/找圆/派生）
    LineGeom line;
    CircleGeom circle;
    Point2D point; ///< 交点或派生点
    bool hasPoint = false;
};

/** 单次「定位 + 全配方测量」周期结果 */
struct CycleResult {
    bool alignmentOk = false;
    bool totalOk = false;   ///< 定位成功且全部测量项 ok
    QString reason;         ///< 失败摘要（定位/某测量项）
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
