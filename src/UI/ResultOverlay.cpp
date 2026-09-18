#include "UI/ResultOverlay.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace Me {
namespace {

QPointF toQt(const Point2D &p)
{
    return QPointF(p.col, p.row);
}

void drawCross(QPainter &p, const QPointF &pt, double arm)
{
    p.drawLine(QPointF(pt.x() - arm, pt.y()), QPointF(pt.x() + arm, pt.y()));
    p.drawLine(QPointF(pt.x(), pt.y() - arm), QPointF(pt.x(), pt.y() + arm));
}

void drawTextOutlined(QPainter &p, const QPointF &pos, const QString &text, const QColor &fill)
{
    p.setPen(QPen(Qt::black, 3));
    p.drawText(pos, text);
    p.setPen(fill);
    p.drawText(pos, text);
}

void drawContours(QPainter &p, const QVector<Contour2D> &contours, const QColor &color, qreal width)
{
    QPen pen(color, width);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    int drawnPts = 0;
    constexpr int kMaxDrawPts = 50000;
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

} // namespace

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

QImage ResultOverlay::render(const QImage &source, const CycleResult &result)
{
    if (source.isNull()) {
        return source;
    }
    QImage out = source.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 11, QFont::Bold));

    // 1) 形状模型轮廓（绿色 XLD）
    if (result.alignmentOk) {
        drawContours(p, result.alignment.modelContours, QColor(0, 255, 100), 2.0);

        // 匹配原点 + 方向
        const QPointF origin(result.alignment.column, result.alignment.row);
        p.setPen(QPen(QColor(0, 200, 255), 2));
        p.setBrush(QColor(0, 200, 255, 160));
        p.drawEllipse(origin, 5, 5);
        const double a = result.alignment.angleRad;
        p.drawLine(origin,
                   origin + QPointF(50 * qCos(a), 50 * qSin(a)));
        drawTextOutlined(p,
                         origin + QPointF(8, -8),
                         QStringLiteral("score=%1").arg(result.alignment.score, 0, 'f', 3),
                         QColor(0, 220, 255));
    }

    // 2) 卡尺与边缘
    for (const auto &m : result.measures) {
        if (m.imageRow == 0.0 && m.imageCol == 0.0 && !m.measured
            && m.firstEdges.isEmpty()) {
            continue;
        }

        const QColor okColor = m.ok ? QColor(50, 220, 80) : QColor(255, 60, 60);
        const QColor measureColor(255, 220, 0);

        // 卡尺矩形（例程 gen_measure_rectangle2 区域）
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
        // 测量轴
        p.setPen(QPen(QColor(255, 160, 0), 1, Qt::DashLine));
        p.drawLine(QPointF(-l1, 0), QPointF(l1, 0));
        p.restore();

        // 全部边缘点（小十字）
        p.setPen(QPen(QColor(255, 80, 80), 1));
        for (const Point2D &e : m.firstEdges) {
            drawCross(p, toQt(e), 4);
        }
        p.setPen(QPen(QColor(80, 160, 255), 1));
        for (const Point2D &e : m.secondEdges) {
            drawCross(p, toQt(e), 4);
        }

        // 选用的边缘对 + 连线 + 数值
        if (m.usedPairIndex >= 0
            && m.usedPairIndex < m.firstEdges.size()
            && m.usedPairIndex < m.secondEdges.size()) {
            const QPointF p1 = toQt(m.firstEdges.at(m.usedPairIndex));
            const QPointF p2 = toQt(m.secondEdges.at(m.usedPairIndex));
            p.setPen(QPen(okColor, 2));
            p.drawLine(p1, p2);
            drawCross(p, p1, 7);
            drawCross(p, p2, 7);

            const QPointF mid = (p1 + p2) * 0.5;
            const QString label = m.measured
                ? QStringLiteral("%1=%2%3")
                      .arg(m.name)
                      .arg(m.value, 0, 'f', m.decimals)
                      .arg(m.unit)
                : QStringLiteral("%1=--").arg(m.name);
            drawTextOutlined(p, mid + QPointF(6, -6), label, okColor);
        } else {
            drawTextOutlined(p,
                             QPointF(m.imageCol + 6, m.imageRow - 6),
                             QStringLiteral("%1: %2").arg(m.name, m.message),
                             okColor);
        }
    }

    // 总判定角标
    {
        const QString stamp = result.totalOk ? QStringLiteral("OK") : QStringLiteral("NG");
        const QColor c = result.totalOk ? QColor(40, 200, 80) : QColor(230, 50, 50);
        p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 28, QFont::Bold));
        drawTextOutlined(p, QPointF(16, 40), stamp, c);
        p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
        drawTextOutlined(p, QPointF(16, 58), result.reason, QColor(230, 230, 230));
    }

    p.end();
    return out;
}

} // namespace Me
