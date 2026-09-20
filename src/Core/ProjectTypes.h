#ifndef PROJECTTYPES_H
#define PROJECTTYPES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Me {

inline constexpr char kProjectFormat[] = "mexp";
inline constexpr char kProjectFileVersion[] = "1.0";
inline constexpr char kProjectFileFilter[] =
    "MeasureExpert 工程 (*.mexp.json);;所有文件 (*.*)";

enum class ImageSourceType {
    Folder,
    Camera
};

enum class MeasureType {
    CaliperWidth,
    CaliperDiameter,
    FindLine,
    FindCircle,
    PointToLine,
    LineIntersection,
    Angle,
    PointDistance
};

struct SearchRegion {
    double row1 = 0.0;
    double col1 = 0.0;
    double row2 = -1.0; // <0 表示全图
    double col2 = -1.0;

    QJsonObject toJson() const;
    static SearchRegion fromJson(const QJsonObject &obj);
};

struct AlignmentConfig {
    QString modelFile;
    SearchRegion searchRegion;
    double minScore = 0.5;
    double angleStartDeg = 0.0;
    double angleExtentDeg = 360.0;

    QJsonObject toJson() const;
    static AlignmentConfig fromJson(const QJsonObject &obj);
};

struct CaliperGeometry {
    double row = 0.0;
    double column = 0.0;
    double phi = 0.0;     // rad，相对模板；找圆时为圆弧起始角
    double length1 = 50.0; // 矩形半长 / 圆半径
    double length2 = 20.0; // 矩形半宽 / 圆环半宽(AnnulusRadius)
    double angleExtent = 6.283185307179586; // 找圆：弧角范围（rad），默认整周

    QJsonObject toJson() const;
    static CaliperGeometry fromJson(const QJsonObject &obj);
};

struct MeasureItem {
    QString id;
    QString name;
    MeasureType type = MeasureType::CaliperWidth;
    CaliperGeometry geometry;
    /// 派生测量引用的前序步骤 id（点线距/交点/夹角/点距）
    QString refIdA;
    QString refIdB;
    double nominal = 0.0;
    double upper = 0.0;
    double lower = 0.0;
    QString unit = QStringLiteral("px");
    int decimals = 3;

    QJsonObject toJson() const;
    static MeasureItem fromJson(const QJsonObject &obj);
    static MeasureItem createDefault(const QString &name);
};

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

struct ImageSourceConfig {
    ImageSourceType type = ImageSourceType::Folder;
    QString path; // Folder 路径；Camera 参数预留

    QJsonObject toJson() const;
    static ImageSourceConfig fromJson(const QJsonObject &obj);
};

struct CalibrationConfig {
    bool calibrated = false;
    double pixelSizeMm = 1.0;
    QString unit = QStringLiteral("px"); // "mm" | "px"

    QJsonObject toJson() const;
    static CalibrationConfig fromJson(const QJsonObject &obj);
};

struct Project {
    QString name;
    QString formatVersion = QString::fromUtf8(kProjectFileVersion);
    ImageSourceConfig imageSource;
    CalibrationConfig calibration;
    QVector<Recipe> recipes;
    QString activeRecipeId;

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
QString newId();

} // namespace Me

Q_DECLARE_METATYPE(Me::Recipe)
Q_DECLARE_METATYPE(Me::CalibrationConfig)
Q_DECLARE_METATYPE(Me::MeasureItem)

#endif // PROJECTTYPES_H
