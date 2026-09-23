#ifndef FINDLINETOOL_H
#define FINDLINETOOL_H

#include "Alg/ITool.h"

namespace Me {

/**
 * 找线工具（FindLine）
 *
 * 几何含义：矩形卡尺（中心 row/col、朝向 phi、半长 length1、半宽 length2）
 *
 * 算法概要：沿卡尺两个正交方向多点采样边缘 → FitLineContourXld
 * 输出：拟合直线端点/法向；测量值 = 拟合线段长度
 * 判定：ToleranceService 中作为中间几何「找到即 OK」
 */
class FindLineTool : public IMeasureTool
{
public:
    MeasureType type() const override { return MeasureType::FindLine; }
    MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const override;
};

} // namespace Me

#endif // FINDLINETOOL_H
