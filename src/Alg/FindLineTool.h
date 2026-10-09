#ifndef FINDLINETOOL_H
#define FINDLINETOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 找线工具（FindLine）
 *
 * 几何：定向矩形卡尺（中心、phi、length1=搜索半长、length2=沿线半宽）
 * 流程：定位 HomMat 把卡尺放到当前图同位置 → 沿线 MeasurePos → FitLineContourXld
 * 输出：拟合直线；测量值 = 拟合线段长度
 * 判定：ToleranceService 中「找到即 OK」
 */
class FindLineTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::FindLine; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // FINDLINETOOL_H
