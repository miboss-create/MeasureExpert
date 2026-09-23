#ifndef ITOOL_H
#define ITOOL_H

#include "Alg/AlgTypes.h"
#include "Core/ProjectTypes.h"

#include <QHash>
#include <QImage>
#include <QString>

namespace Me {

/** 单次工具执行上下文（定位结果 + 前序测量几何）。 */
struct ToolContext {
    QImage image;
    AlignmentResult alignment;
    CalibrationConfig calibration;
    QString projectDir;
    /// true：ROI 存相对模板，运行时需定位成功再仿射；false：ROI 即图像坐标，无需定位
    bool followPose = false;
    /// 已按配方顺序跑完的前序测量结果（key = MeasureItem.id）
    QHash<QString, MeasureResult> priors;
};

/**
 * 测量工具统一接口。
 * 卡尺/找线/找圆/派生各自实现 IMeasureTool；定位走 IAlignmentTool。
 * 调度入口：ToolRegistry → MeasureService / RunEngine。
 */
class IMeasureTool
{
public:
    virtual ~IMeasureTool() = default;
    virtual MeasureType type() const = 0;
    virtual MeasureResult run(const MeasureItem &item, const ToolContext &ctx) const = 0;
};

/** 定位工具（配方流水线可选第一步，不进入 measures 列表） */
class IAlignmentTool
{
public:
    virtual ~IAlignmentTool() = default;
    virtual AlignmentResult run(const ToolContext &ctx, const AlignmentConfig &config) const = 0;
};

} // namespace Me

#endif // ITOOL_H
