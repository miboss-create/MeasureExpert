#include "UI/ResultOverlay.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace Me {
namespace {

/** AlgTypes 点 → Qt 坐标（x=col, y=row） */
QPointF toQt(const Point2D &p)
{
    return QPointF(p.col, p.row);
}

/** 以 pt 为中心绘制十字；angleRad 为第一臂相对水平的转角（弧度） */
void drawCross(QPainter &p, const QPointF &pt, double arm, double angleRad = 0.0)
{
    const double c = qCos(angleRad);
    const double s = qSin(angleRad);
    const QPointF d1(arm * c, arm * s);
    const QPointF d2(-arm * s, arm * c); // 垂直臂
    p.drawLine(pt - d1, pt + d1);
    p.drawLine(pt - d2, pt + d2);
}

/** 拟合线方向角（图像坐标，x=col / y=row） */
double lineAngleRad(const LineGeom &line)
{
    if (!line.valid) {
        return 0.0;
    }
    const double dx = line.colEnd - line.colBegin;
    const double dy = line.rowEnd - line.rowBegin;
    if (qAbs(dx) < 1e-12 && qAbs(dy) < 1e-12) {
        return 0.0;
    }
    return qAtan2(dy, dx);
}

/** 黑描边 + 彩色填充，提高复杂背景上的可读性 */
void drawTextOutlined(QPainter &p, const QPointF &baselinePos, const QString &text, const QColor &fill)
{
    if (text.isEmpty()) {
        return;
    }
    p.setPen(QPen(Qt::black, 3));
    p.drawText(baselinePos, text);
    p.setPen(fill);
    p.drawText(baselinePos, text);
}

/// 按左上角锚点绘制（避免基线导致顶部裁切）
void drawTextOutlinedTopLeft(QPainter &p, const QPointF &topLeft, const QString &text, const QColor &fill)
{
    if (text.isEmpty()) {
        return;
    }
    const QFontMetrics fm(p.font());
    const QPointF baseline(topLeft.x(), topLeft.y() + fm.ascent());
    drawTextOutlined(p, baseline, text, fill);
}

void drawContours(QPainter &p, const QVector<Contour2D> &contours, const QColor &color, qreal width)
{
    QPen pen(color, width);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    int drawnPts = 0;
    constexpr int kMaxDrawPts = 50000; ///< 防止超大轮廓拖垮绘制性能
    for (const Contour2D &c : contours) {
        if (c.points.size() < 2) {
            continue;
        }
        QPainterPath path;
        path.moveTo(toQt(c.points.first()));
        for (int i = 1; i < c.points.size(); ++i) {
            path.lineTo(toQt(c.points.at(i)));
            if (++drawnPts >= kMaxDrawPts) {
                p.drawPath(path);
                return;
            }
        }
        p.drawPath(path);
    }
}

bool isCaliperType(MeasureType t)
{
    return t == MeasureType::CaliperWidth || t == MeasureType::CaliperDiameter;
}

/** 绘制拟合线段：中点延伸 preferHalfLen，端点小圆标记 */
void drawFittedLine(QPainter &p, const LineGeom &line, double preferHalfLen, const QColor &color)
{
    if (!line.valid
        || !qIsFinite(line.rowBegin) || !qIsFinite(line.colBegin)
        || !qIsFinite(line.rowEnd) || !qIsFinite(line.colEnd)) {
        return;
    }

    QPointF a(line.colBegin, line.rowBegin);
    QPointF b(line.colEnd, line.rowEnd);
    QPointF d = b - a;
    const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
    QPen linePen(color, 1.5);
    linePen.setCosmetic(true);
    p.setPen(linePen);

    if (len < 1e-3) {
        p.setBrush(color);
        p.drawEllipse(a, 1.5, 1.5);
        return;
    }

    const QPointF dir = d / len;
    const QPointF mid = (a + b) * 0.5;
    const double half = qMax(preferHalfLen > 0 ? preferHalfLen : len * 0.5, len * 0.5);
    p.drawLine(mid - dir * half, mid + dir * half);

    p.setBrush(color);
    p.drawEllipse(a, 1.5, 1.5);
    p.drawEllipse(b, 1.5, 1.5);
}

} // namespace

// ---------------------------------------------------------------------------
// 轻量轮廓叠加
// ---------------------------------------------------------------------------

QImage ResultOverlay::renderContours(const QImage &source,
                                     const QVector<Contour2D> &contours,
                                     const QRectF *roi)
{
    if (source.isNull()) {
        return source;
    }
    QImage out = source.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (roi && roi->isValid()) {
        QPen pen(QColor(0, 220, 120), 2);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(QColor(0, 220, 120, 30));
        p.drawRect(*roi);
    }

    drawContours(p, contours, QColor(0, 255, 128), 2.0);
    p.end();
    return out;
}

// ---------------------------------------------------------------------------
// 完整 CycleResult 叠加
// ---------------------------------------------------------------------------

QImage ResultOverlay::render(const QImage &source, const CycleResult &result)
{
    if (source.isNull()) {
        return source;
    }
    QImage out = source.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 11, QFont::Bold));

    // --- 定位结果 ---
    if (result.alignment.ok) {
        drawContours(p, result.alignment.modelContours, QColor(0, 255, 100), 2.0);

        const QPointF origin(result.alignment.column, result.alignment.row);
        p.setPen(QPen(QColor(0, 200, 255), 2));
        p.setBrush(QColor(0, 200, 255, 160));
        p.drawEllipse(origin, 5, 5);
        const double a = result.alignment.angleRad;
        p.drawLine(origin, origin + QPointF(50 * qCos(a), 50 * qSin(a)));
    }

    // --- 各测量项几何 ---
    for (const auto &m : result.measures) {
        const bool hasGeom = m.line.valid || m.circle.valid || m.hasPoint
            || !m.firstEdges.isEmpty() || m.measured
            || (m.imageRow != 0.0 || m.imageCol != 0.0);
        if (!hasGeom) {
            continue;
        }

        const QColor okColor = m.ok ? QColor(50, 220, 80) : QColor(255, 60, 60);
        const QColor measureColor(255, 220, 0);

        // 卡尺/找线：局部坐标系下绘制 length1×length2 矩形框
        if (isCaliperType(m.type) || m.type == MeasureType::FindLine) {
            p.save();
            p.translate(m.imageCol, m.imageRow);
            p.rotate(qRadiansToDegrees(m.imagePhi));
            const double l1 = m.length1 > 0 ? m.length1 : 40.0;
            const double l2 = m.length2 > 0 ? m.length2 : 10.0;
            QPen rectPen(measureColor, 2);
            rectPen.setCosmetic(true);
            p.setPen(rectPen);
            p.setBrush(QColor(255, 220, 0, 35));
            p.drawRect(QRectF(-l1, -l2, l1 * 2.0, l2 * 2.0));
            p.setPen(QPen(QColor(255, 160, 0), 1, Qt::DashLine));
            p.drawLine(QPointF(-l1, 0), QPointF(l1, 0));
            p.restore();
        }

        // 找圆：内外两圈表示环带搜索区域（length1=半径, length2=环宽）
        if (m.type == MeasureType::FindCircle && m.length1 > 0
            && qIsFinite(m.imageRow) && qIsFinite(m.imageCol)
            && qIsFinite(m.length1) && qIsFinite(m.length2)) {
            const double r = qMin(m.length1, 1.0e6);
            const double ann = qBound(1.0, m.length2, r * 0.9);
            QPen arcPen(measureColor, 2);
            arcPen.setCosmetic(true);
            p.setPen(arcPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(m.imageCol, m.imageRow), r + ann, r + ann);
            p.drawEllipse(QPointF(m.imageCol, m.imageRow), qMax(1.0, r - ann), qMax(1.0, r - ann));
        }

        // 先画拟合线，再画边缘点十字（十字相对直线转 45°，避免被线挡住）
        if (m.line.valid) {
            const double half = m.length1 > 0 ? m.length1 : 40.0;
            drawFittedLine(p, m.line, half, okColor);
        }

        const double crossAngle = m.line.valid
            ? (lineAngleRad(m.line) + M_PI_4)
            : M_PI_4;
        QPen edgePen(QColor(255, 80, 80), 1.0);
        edgePen.setCosmetic(true);
        p.setPen(edgePen);
        for (const Point2D &e : m.firstEdges) {
            drawCross(p, toQt(e), 2.5, crossAngle);
        }
        edgePen.setColor(QColor(80, 160, 255));
        p.setPen(edgePen);
        for (const Point2D &e : m.secondEdges) {
            drawCross(p, toQt(e), 2.5, crossAngle);
        }

        if (m.circle.valid && qIsFinite(m.circle.row) && qIsFinite(m.circle.col)
            && qIsFinite(m.circle.radius) && m.circle.radius > 0.0) {
            const double rr = qMin(m.circle.radius, 1.0e6);
            QPen circlePen(okColor, 1.5);
            circlePen.setCosmetic(true);
            p.setPen(circlePen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(m.circle.col, m.circle.row), rr, rr);
            p.setPen(QPen(okColor, 1.0));
            drawCross(p, QPointF(m.circle.col, m.circle.row), 3.0, M_PI_4);
        }

        if (m.hasPoint && qIsFinite(m.point.row) && qIsFinite(m.point.col)) {
            p.setPen(QPen(okColor, 1.0));
            p.setBrush(QColor(okColor.red(), okColor.green(), okColor.blue(), 160));
            p.drawEllipse(toQt(m.point), 2.0, 2.0);
            drawCross(p, toQt(m.point), 3.0, M_PI_4);
        }

        if (m.usedPairIndex >= 0
            && m.usedPairIndex < m.firstEdges.size()
            && m.usedPairIndex < m.secondEdges.size()) {
            const QPointF p1 = toQt(m.firstEdges.at(m.usedPairIndex));
            const QPointF p2 = toQt(m.secondEdges.at(m.usedPairIndex));
            p.setPen(QPen(okColor, 1.5));
            p.drawLine(p1, p2);
            p.setPen(QPen(okColor, 1.0));
            drawCross(p, p1, 3.0, crossAngle);
            drawCross(p, p2, 3.0, crossAngle);
        }
    }

    {
        // 图上只留总判定 OK/NG，详细数值看右侧结果面板
        const QString stamp = result.totalOk ? QStringLiteral("OK") : QStringLiteral("NG");
        const QColor stampColor = result.totalOk ? QColor(40, 200, 80) : QColor(230, 50, 50);
        p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 28, QFont::Bold));
        drawTextOutlinedTopLeft(p, QPointF(12, 12), stamp, stampColor);
    }

    p.end();
    return out;
}

} // namespace Me
