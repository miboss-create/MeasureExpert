#include "Alg/GeomDeriveTool.h"

#include "Alg/ToolCommon.h"

using namespace Me::ToolCommon;

namespace Me {
namespace {

const MeasureResult *priorOf(const ToolContext &ctx, const QString &id)
{
    if (id.isEmpty()) {
        return nullptr;
    }
    auto it = ctx.priors.constFind(id);
    if (it == ctx.priors.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

bool extractPoint(const MeasureResult &r, Point2D *out)
{
    if (!out) {
        return false;
    }
    if (r.hasPoint) {
        *out = r.point;
        return true;
    }
    if (r.circle.valid) {
        out->row = r.circle.row;
        out->col = r.circle.col;
        return true;
    }
    if (r.line.valid) {
        out->row = 0.5 * (r.line.rowBegin + r.line.rowEnd);
        out->col = 0.5 * (r.line.colBegin + r.line.colEnd);
        return true;
    }
    if (r.usedPairIndex >= 0
        && r.usedPairIndex < r.firstEdges.size()
        && r.usedPairIndex < r.secondEdges.size()) {
        const Point2D &a = r.firstEdges.at(r.usedPairIndex);
        const Point2D &b = r.secondEdges.at(r.usedPairIndex);
        out->row = 0.5 * (a.row + b.row);
        out->col = 0.5 * (a.col + b.col);
        return true;
    }
    if (!r.firstEdges.isEmpty()) {
        *out = r.firstEdges.first();
        return true;
    }
    return false;
}

bool extractLine(const MeasureResult &r, LineGeom *out)
{
    if (!out || !r.line.valid) {
        return false;
    }
    *out = r.line;
    return true;
}

double pointLineDistance(const Point2D &p, const LineGeom &line)
{
    // HALCON: nr*row + nc*col - dist = 0
    return qAbs(line.nr * p.row + line.nc * p.col - line.dist);
}

bool intersectLines(const LineGeom &a, const LineGeom &b, Point2D *out)
{
    if (!out) {
        return false;
    }
    // a.nr*r + a.nc*c = a.dist
    // b.nr*r + b.nc*c = b.dist
    const double det = a.nr * b.nc - a.nc * b.nr;
    if (qAbs(det) < 1e-12) {
        return false;
    }
    out->row = (a.dist * b.nc - a.nc * b.dist) / det;
    out->col = (a.nr * b.dist - a.dist * b.nr) / det;
    return qIsFinite(out->row) && qIsFinite(out->col);
}

double lineAngleDeg(const LineGeom &a, const LineGeom &b)
{
    // 方向向量垂直于法向
    const double ax = -a.nc;
    const double ay = a.nr;
    const double bx = -b.nc;
    const double by = b.nr;
    const double na = qSqrt(ax * ax + ay * ay);
    const double nb = qSqrt(bx * bx + by * by);
    if (na < 1e-12 || nb < 1e-12) {
        return 0.0;
    }
    double cosv = (ax * bx + ay * by) / (na * nb);
    cosv = qBound(-1.0, cosv, 1.0);
    double deg = qRadiansToDegrees(qAcos(cosv));
    if (deg > 90.0) {
        deg = 180.0 - deg;
    }
    return deg;
}

} // namespace

MeasureResult PointToLineTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    const MeasureResult *pa = priorOf(ctx, item.refIdA);
    const MeasureResult *pb = priorOf(ctx, item.refIdB);
    if (!pa || !pb) {
        return failResult(item, QStringLiteral("点线距需引用前序点源与找线结果"));
    }

    Point2D pt;
    LineGeom line;
    // refA = 点，refB = 线；若反了则自动交换
    if (extractPoint(*pa, &pt) && extractLine(*pb, &line)) {
        // ok
    } else if (extractPoint(*pb, &pt) && extractLine(*pa, &line)) {
        // swapped
    } else {
        return failResult(item, QStringLiteral("无法从引用项提取点与直线"));
    }

    MeasureResult result = baseResult(item);
    result.hasPoint = true;
    result.point = pt;
    result.line = line;
    result.imageRow = pt.row;
    result.imageCol = pt.col;

    const double distPx = pointLineDistance(pt, line);
    QString unit;
    result.value = applyCalibration(distPx, ctx.calibration, &unit);
    result.unit = unit;
    result.measured = true;
    result.message = QStringLiteral("点线距完成");
    return result;
}

MeasureResult LineIntersectionTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    const MeasureResult *pa = priorOf(ctx, item.refIdA);
    const MeasureResult *pb = priorOf(ctx, item.refIdB);
    if (!pa || !pb) {
        return failResult(item, QStringLiteral("交点需引用两条前序找线"));
    }

    LineGeom la, lb;
    if (!extractLine(*pa, &la) || !extractLine(*pb, &lb)) {
        return failResult(item, QStringLiteral("引用项未提供有效直线"));
    }

    Point2D pt;
    if (!intersectLines(la, lb, &pt)) {
        return failResult(item, QStringLiteral("两线平行或无交点"));
    }

    MeasureResult result = baseResult(item);
    result.line = la;
    result.hasPoint = true;
    result.point = pt;
    result.imageRow = pt.row;
    result.imageCol = pt.col;
    result.value = 0.0;
    result.unit = QStringLiteral("px");
    result.measured = true;
    result.message = QStringLiteral("交点完成");
    return result;
}

MeasureResult AngleTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    const MeasureResult *pa = priorOf(ctx, item.refIdA);
    const MeasureResult *pb = priorOf(ctx, item.refIdB);
    if (!pa || !pb) {
        return failResult(item, QStringLiteral("夹角需引用两条前序找线"));
    }

    LineGeom la, lb;
    if (!extractLine(*pa, &la) || !extractLine(*pb, &lb)) {
        return failResult(item, QStringLiteral("引用项未提供有效直线"));
    }

    MeasureResult result = baseResult(item);
    result.line = la;
    result.value = lineAngleDeg(la, lb);
    result.unit = QStringLiteral("deg");
    result.measured = true;
    result.message = QStringLiteral("夹角完成");
    if (la.valid) {
        result.imageRow = 0.5 * (la.rowBegin + la.rowEnd);
        result.imageCol = 0.5 * (la.colBegin + la.colEnd);
    }
    return result;
}

MeasureResult PointDistanceTool::run(const MeasureItem &item, const ToolContext &ctx) const
{
    const MeasureResult *pa = priorOf(ctx, item.refIdA);
    const MeasureResult *pb = priorOf(ctx, item.refIdB);
    if (!pa || !pb) {
        return failResult(item, QStringLiteral("点距需引用两个前序点源"));
    }

    Point2D a, b;
    if (!extractPoint(*pa, &a) || !extractPoint(*pb, &b)) {
        return failResult(item, QStringLiteral("无法从引用项提取点"));
    }

    MeasureResult result = baseResult(item);
    result.hasPoint = true;
    result.point = a;
    result.imageRow = 0.5 * (a.row + b.row);
    result.imageCol = 0.5 * (a.col + b.col);
    result.firstEdges = {a};
    result.secondEdges = {b};
    result.usedPairIndex = 0;

    const double distPx = qSqrt((a.row - b.row) * (a.row - b.row)
                                + (a.col - b.col) * (a.col - b.col));
    QString unit;
    result.value = applyCalibration(distPx, ctx.calibration, &unit);
    result.unit = unit;
    result.measured = true;
    result.message = QStringLiteral("点距完成");
    return result;
}

} // namespace Me
