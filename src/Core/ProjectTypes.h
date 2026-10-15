#ifndef PROJECTTYPES_H
#define PROJECTTYPES_H

/**
 * @file ProjectTypes.h
 * @brief MeasureExpert 工程数据模型：配方、测量项、标定及 JSON 序列化。
 *
 * 工程文件格式为 .mexp.json，顶层 Project 包含图像源、标定、多配方及当前激活配方 id。
 * 各结构体提供 toJson / fromJson，供 ProjectService 持久化与 UI 编辑。
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Me {

/// 工程文件 format 字段标识
inline constexpr char kProjectFormat[] = "mexp";
/// 当前支持的工程文件版本号
/** 1.1：卡尺 geometry.phi 改为 HALCON rectangle2 角（相对 1.0 的 Qt 角取反） */
inline constexpr char kProjectFileVersion[] = "1.1";
/// 文件对话框过滤器
inline constexpr char kProjectFileFilter[] =
    "MeasureExpert 工程 (*.mexp.json);;所有文件 (*.*)";

/** 图像输入来源类型 */
enum class ImageSourceType {
    Folder, ///< 本地文件夹序列
    Camera  ///< 工业相机（V2+ 预留）
};

/** 测量/几何工具类型，与 Alg 层 IMeasureTool 一一对应 */
enum class MeasureType {
    CaliperWidth,      ///< 卡尺测宽（measure_pairs）
    CaliperDiameter,   ///< 卡尺测径（圆环卡尺）
    FindLine,          ///< 矩形卡尺找线
    FindCircle,        ///< 圆弧卡尺找圆
    PointToLine,       ///< 点到直线距离（派生，引用 refIdA/B）
    LineIntersection,  ///< 两直线交点（派生）
    Angle,             ///< 两直线夹角（派生）
    PointDistance      ///< 两点距离（派生）
};

/** 矩形搜索/模板 ROI，HALCON 图像坐标（row/col） */
struct SearchRegion {
    double row1 = 0.0;
    double col1 = 0.0;
    double row2 = -1.0; ///< <0 表示全图
    double col2 = -1.0;

    QJsonObject toJson() const;
    static SearchRegion fromJson(const QJsonObject &obj);
};

/** 配方级定位（形状匹配）配置 */
struct AlignmentConfig {
    QString modelFile;              ///< 形状模型相对/绝对路径
    SearchRegion searchRegion;      ///< 运行时搜索 ROI
    double minScore = 0.5;          ///< FindShapeModel 最低得分
    double angleStartDeg = 0.0;     ///< 搜索起始角（度），须与训练一致
    double angleExtentDeg = 360.0;  ///< 搜索角度范围（度）

    QJsonObject toJson() const;
    static AlignmentConfig fromJson(const QJsonObject &obj);
};

/**
 * 卡尺/圆弧几何参数（模板坐标系，经定位 HomMat2D 变换到图像坐标）。
 *
 * 找线/测宽：phi 为 HALCON rectangle2 长轴角（rad）；length1=半长，length2=半宽。
 * 找圆/测径：phi 为圆弧起始角；length1=半径；length2=环宽一半；angleExtent=弧角范围。
 */
struct CaliperGeometry {
    double row = 0.0;
    double column = 0.0;
    double phi = 0.0;     // HALCON rectangle2 角（rad）；找圆时为圆弧起始角
    double length1 = 50.0; // 矩形半长 / 圆半径
    double length2 = 20.0; // 矩形半宽 / 圆环半宽(AnnulusRadius)
    double angleExtent = 6.283185307179586; // 找圆：弧角范围（rad），默认整周

    QJsonObject toJson() const;
    static CaliperGeometry fromJson(const QJsonObject &obj);
};

/** 单条测量步骤：几何 + 公差 + 派生引用 */
struct MeasureItem {
    QString id;
    QString name;
    MeasureType type = MeasureType::CaliperWidth;
    CaliperGeometry geometry;
    /// 派生测量引用的前序步骤 id（点线距/交点/夹角/点距）
    QString refIdA;
    QString refIdB;
    double nominal = 0.0;  ///< 标称值
    double upper = 0.0;    ///< 上偏差（相对标称）
    double lower = 0.0;    ///< 下偏差（相对标称，通常为负）
    QString unit = QStringLiteral("px");
    int decimals = 3;
    /// 已在主图确认过卡尺/找圆 ROI。未确认的项不参与试跑，也不叠默认框。
    bool roiDefined = false;

    /// 边缘检测平滑（MeasurePos / MeasurePairs 的 Sigma），越大越抗噪
    double edgeSigma = 1.5;
    /// 边缘幅度阈值（Threshold），越小越灵敏、也越易吸干扰边
    double edgeThreshold = 20.0;
    /// 找线/找圆沿线（或圆周）采样步长（像素）；越小点越密
    double samplePitchPx = 6.0;

    QJsonObject toJson() const;
    static MeasureItem fromJson(const QJsonObject &obj);
    static MeasureItem createDefault(const QString &name);
};

/**
 * 配方：一次检测流程 = 定位 + 有序测量项列表。
 * enabled 为 false 时 RunEngine 跳过该配方。
 */
struct Recipe {
    QString id;
    QString name;
    bool enabled = true;
    AlignmentConfig alignment;
    QVector<MeasureItem> measures;

    QJsonObject toJson() const;
    static Recipe fromJson(const QJsonObject &obj);
    static Recipe createDefault(const QString &name);

    int indexOfMeasure(const QString &measureId) const;
};

/** 工程级图像源配置 */
struct ImageSourceConfig {
    ImageSourceType type = ImageSourceType::Folder;
    QString path; ///< Folder 路径
    /** Camera：后端 ID（Halcon），空则默认 Halcon */
    QString cameraBackend;
    /** Camera：设备 ID（Halcon 为「接口>>Device」） */
    QString cameraDeviceId;
    QString cameraDisplayName;

    QJsonObject toJson() const;
    static ImageSourceConfig fromJson(const QJsonObject &obj);
};

/** 像素标定：将测量值在 px 与 mm 间换算 */
struct CalibrationConfig {
    bool calibrated = false;
    double pixelSizeMm = 1.0; ///< 每像素物理尺寸（mm/px）
    QString unit = QStringLiteral("px"); ///< 显示/判定单位："mm" | "px"

    QJsonObject toJson() const;
    static CalibrationConfig fromJson(const QJsonObject &obj);
};

/** 完整工程内存模型 */
struct Project {
    QString name;
    QString formatVersion = QString::fromUtf8(kProjectFileVersion);
    ImageSourceConfig imageSource;
    CalibrationConfig calibration;
    QVector<Recipe> recipes;
    QString activeRecipeId; ///< 当前 UI/运行选中的配方

    QJsonObject toJson() const;
    static Project fromJson(const QJsonObject &obj, QString *error = nullptr);
    static Project createNew(const QString &name = QString());

    int indexOfRecipe(const QString &recipeId) const;
    Recipe *findRecipe(const QString &recipeId);
    const Recipe *findRecipe(const QString &recipeId) const;
};

QString measureTypeToString(MeasureType type);
MeasureType measureTypeFromString(const QString &text, bool *ok = nullptr);
QString imageSourceTypeToString(ImageSourceType type);
ImageSourceType imageSourceTypeFromString(const QString &text, bool *ok = nullptr);
/** 生成无花括号的 UUID 字符串，用作 id */
QString newId();

} // namespace Me

Q_DECLARE_METATYPE(Me::Recipe)
Q_DECLARE_METATYPE(Me::CalibrationConfig)
Q_DECLARE_METATYPE(Me::MeasureItem)

#endif // PROJECTTYPES_H
