#ifndef FINDCIRCLETOOL_H
#define FINDCIRCLETOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 找圆工具（FindCircle）
 *
 * 几何含义（CaliperGeometry）：
 *   - row/column/phi : 圆心与起始角（图像坐标，或相对模板后再仿射）
 *   - length1        : 名义半径（环带中心半径）
 *   - length2        : 环宽（径向半搜索范围）
 *   - angleExtent    : 圆弧展角（弧度，默认 2π 整圆）
 *
 * 算法概要：沿圆周布径向卡尺 → MeasurePos 取最强边缘 → FitCircleContourXld
 * 输出：拟合圆心/半径；测量值 = 半径（px 或标定后单位）
 * 判定：ToleranceService 中作为中间几何「找到即 OK」（半径仅展示）
 */
class FindCircleTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::FindCircle; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // FINDCIRCLETOOL_H
