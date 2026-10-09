/**
 * @file ProjectTypes.cpp
 * @brief ProjectTypes 的 JSON 序列化、枚举字符串转换及工厂方法实现。
 */

#include "Core/ProjectTypes.h"

#include <QUuid>

namespace Me {
namespace {

// --- JSON 字段读取辅助（缺省或类型不符时返回 fallback）---

double jsonDouble(const QJsonObject &obj, const char *key, double fallback)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : fallback;
}

int jsonInt(const QJsonObject &obj, const char *key, int fallback)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    return v.isDouble() ? v.toInt() : fallback;
}

QString jsonString(const QJsonObject &obj, const char *key, const QString &fallback = {})
{
    const QJsonValue v = obj.value(QLatin1String(key));
    return v.isString() ? v.toString() : fallback;
}

bool jsonBool(const QJsonObject &obj, const char *key, bool fallback)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    return v.isBool() ? v.toBool() : fallback;
}

} // namespace

// --- 通用工具 ---

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// --- MeasureType / ImageSourceType 字符串互转（持久化用）---

QString measureTypeToString(MeasureType type)
{
    switch (type) {
    case MeasureType::CaliperWidth:
        return QStringLiteral("CaliperWidth");
    case MeasureType::CaliperDiameter:
        return QStringLiteral("CaliperDiameter");
    case MeasureType::FindLine:
        return QStringLiteral("FindLine");
    case MeasureType::FindCircle:
        return QStringLiteral("FindCircle");
    case MeasureType::PointToLine:
        return QStringLiteral("PointToLine");
    case MeasureType::LineIntersection:
        return QStringLiteral("LineIntersection");
    case MeasureType::Angle:
        return QStringLiteral("Angle");
    case MeasureType::PointDistance:
        return QStringLiteral("PointDistance");
    }
    return QStringLiteral("CaliperWidth");
}

MeasureType measureTypeFromString(const QString &text, bool *ok)
{
    if (ok) {
        *ok = true;
    }
    if (text == QLatin1String("CaliperWidth")) {
        return MeasureType::CaliperWidth;
    }
    if (text == QLatin1String("CaliperDiameter")) {
        return MeasureType::CaliperDiameter;
    }
    if (text == QLatin1String("FindLine")) {
        return MeasureType::FindLine;
    }
    if (text == QLatin1String("FindCircle")) {
        return MeasureType::FindCircle;
    }
    if (text == QLatin1String("PointToLine")) {
        return MeasureType::PointToLine;
    }
    if (text == QLatin1String("LineIntersection")) {
        return MeasureType::LineIntersection;
    }
    if (text == QLatin1String("Angle")) {
        return MeasureType::Angle;
    }
    if (text == QLatin1String("PointDistance")) {
        return MeasureType::PointDistance;
    }
    if (ok) {
        *ok = false;
    }
    return MeasureType::CaliperWidth;
}

QString imageSourceTypeToString(ImageSourceType type)
{
    switch (type) {
    case ImageSourceType::Folder:
        return QStringLiteral("Folder");
    case ImageSourceType::Camera:
        return QStringLiteral("Camera");
    }
    return QStringLiteral("Folder");
}

ImageSourceType imageSourceTypeFromString(const QString &text, bool *ok)
{
    if (ok) {
        *ok = true;
    }
    if (text == QLatin1String("Folder")) {
        return ImageSourceType::Folder;
    }
    if (text == QLatin1String("Camera")) {
        return ImageSourceType::Camera;
    }
    if (ok) {
        *ok = false;
    }
    return ImageSourceType::Folder;
}

// --- SearchRegion ---

QJsonObject SearchRegion::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("row1"), row1);
    obj.insert(QStringLiteral("col1"), col1);
    obj.insert(QStringLiteral("row2"), row2);
    obj.insert(QStringLiteral("col2"), col2);
    return obj;
}

SearchRegion SearchRegion::fromJson(const QJsonObject &obj)
{
    SearchRegion r;
    r.row1 = jsonDouble(obj, "row1", 0.0);
    r.col1 = jsonDouble(obj, "col1", 0.0);
    r.row2 = jsonDouble(obj, "row2", -1.0);
    r.col2 = jsonDouble(obj, "col2", -1.0);
    return r;
}

// --- AlignmentConfig ---

QJsonObject AlignmentConfig::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("modelFile"), modelFile);
    obj.insert(QStringLiteral("searchRegion"), searchRegion.toJson());
    obj.insert(QStringLiteral("minScore"), minScore);
    obj.insert(QStringLiteral("angleStartDeg"), angleStartDeg);
    obj.insert(QStringLiteral("angleExtentDeg"), angleExtentDeg);
    return obj;
}

AlignmentConfig AlignmentConfig::fromJson(const QJsonObject &obj)
{
    AlignmentConfig a;
    a.modelFile = jsonString(obj, "modelFile");
    a.searchRegion = SearchRegion::fromJson(obj.value(QStringLiteral("searchRegion")).toObject());
    a.minScore = jsonDouble(obj, "minScore", 0.5);
    a.angleStartDeg = jsonDouble(obj, "angleStartDeg", 0.0);
    a.angleExtentDeg = jsonDouble(obj, "angleExtentDeg", 360.0);
    return a;
}

// --- CaliperGeometry ---

QJsonObject CaliperGeometry::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("row"), row);
    obj.insert(QStringLiteral("column"), column);
    obj.insert(QStringLiteral("phi"), phi);
    obj.insert(QStringLiteral("length1"), length1);
    obj.insert(QStringLiteral("length2"), length2);
    obj.insert(QStringLiteral("angleExtent"), angleExtent);
    return obj;
}

CaliperGeometry CaliperGeometry::fromJson(const QJsonObject &obj)
{
    CaliperGeometry g;
    g.row = jsonDouble(obj, "row", 0.0);
    g.column = jsonDouble(obj, "column", 0.0);
    g.phi = jsonDouble(obj, "phi", 0.0);
    g.length1 = jsonDouble(obj, "length1", 50.0);
    g.length2 = jsonDouble(obj, "length2", 20.0);
    g.angleExtent = jsonDouble(obj, "angleExtent", 6.283185307179586);
    return g;
}

// --- MeasureItem ---

MeasureItem MeasureItem::createDefault(const QString &name)
{
    MeasureItem item;
    item.id = newId();
    item.name = name;
    item.type = MeasureType::CaliperWidth;
    item.unit = QStringLiteral("px");
    item.decimals = 3;
    return item;
}

QJsonObject MeasureItem::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("type"), measureTypeToString(type));
    obj.insert(QStringLiteral("geometry"), geometry.toJson());
    obj.insert(QStringLiteral("refIdA"), refIdA);
    obj.insert(QStringLiteral("refIdB"), refIdB);
    obj.insert(QStringLiteral("nominal"), nominal);
    obj.insert(QStringLiteral("upper"), upper);
    obj.insert(QStringLiteral("lower"), lower);
    obj.insert(QStringLiteral("unit"), unit);
    obj.insert(QStringLiteral("decimals"), decimals);
    return obj;
}

MeasureItem MeasureItem::fromJson(const QJsonObject &obj)
{
    MeasureItem item;
    item.id = jsonString(obj, "id", newId());
    item.name = jsonString(obj, "name", QStringLiteral("测量项"));
    item.type = measureTypeFromString(jsonString(obj, "type", QStringLiteral("CaliperWidth")));
    item.geometry = CaliperGeometry::fromJson(obj.value(QStringLiteral("geometry")).toObject());
    item.refIdA = jsonString(obj, "refIdA");
    item.refIdB = jsonString(obj, "refIdB");
    item.nominal = jsonDouble(obj, "nominal", 0.0);
    item.upper = jsonDouble(obj, "upper", 0.0);
    item.lower = jsonDouble(obj, "lower", 0.0);
    item.unit = jsonString(obj, "unit", QStringLiteral("px"));
    item.decimals = jsonInt(obj, "decimals", 3);
    return item;
}

// --- Recipe ---

Recipe Recipe::createDefault(const QString &name)
{
    Recipe recipe;
    recipe.id = newId();
    recipe.name = name;
    recipe.enabled = true;
    return recipe;
}

int Recipe::indexOfMeasure(const QString &measureId) const
{
    for (int i = 0; i < measures.size(); ++i) {
        if (measures.at(i).id == measureId) {
            return i;
        }
    }
    return -1;
}

QJsonObject Recipe::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("enabled"), enabled);
    obj.insert(QStringLiteral("alignment"), alignment.toJson());
    QJsonArray arr;
    for (const MeasureItem &m : measures) {
        arr.append(m.toJson());
    }
    obj.insert(QStringLiteral("measures"), arr);
    return obj;
}

Recipe Recipe::fromJson(const QJsonObject &obj)
{
    Recipe recipe;
    recipe.id = jsonString(obj, "id", newId());
    recipe.name = jsonString(obj, "name", QStringLiteral("配方"));
    recipe.enabled = jsonBool(obj, "enabled", true);
    recipe.alignment = AlignmentConfig::fromJson(obj.value(QStringLiteral("alignment")).toObject());
    const QJsonArray arr = obj.value(QStringLiteral("measures")).toArray();
    for (const QJsonValue &v : arr) {
        if (v.isObject()) {
            recipe.measures.push_back(MeasureItem::fromJson(v.toObject()));
        }
    }
    return recipe;
}

// --- ImageSourceConfig / CalibrationConfig ---

QJsonObject ImageSourceConfig::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), imageSourceTypeToString(type));
    obj.insert(QStringLiteral("path"), path);
    if (!cameraBackend.isEmpty()) {
        obj.insert(QStringLiteral("cameraBackend"), cameraBackend);
    }
    if (!cameraDeviceId.isEmpty()) {
        obj.insert(QStringLiteral("cameraDeviceId"), cameraDeviceId);
    }
    if (!cameraDisplayName.isEmpty()) {
        obj.insert(QStringLiteral("cameraDisplayName"), cameraDisplayName);
    }
    return obj;
}

ImageSourceConfig ImageSourceConfig::fromJson(const QJsonObject &obj)
{
    ImageSourceConfig cfg;
    cfg.type = imageSourceTypeFromString(jsonString(obj, "type", QStringLiteral("Folder")));
    cfg.path = jsonString(obj, "path");
    cfg.cameraBackend = jsonString(obj, "cameraBackend");
    cfg.cameraDeviceId = jsonString(obj, "cameraDeviceId");
    cfg.cameraDisplayName = jsonString(obj, "cameraDisplayName");
    return cfg;
}

QJsonObject CalibrationConfig::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("calibrated"), calibrated);
    obj.insert(QStringLiteral("pixelSizeMm"), pixelSizeMm);
    obj.insert(QStringLiteral("unit"), unit);
    return obj;
}

CalibrationConfig CalibrationConfig::fromJson(const QJsonObject &obj)
{
    CalibrationConfig cfg;
    cfg.calibrated = jsonBool(obj, "calibrated", false);
    cfg.pixelSizeMm = jsonDouble(obj, "pixelSizeMm", 1.0);
    cfg.unit = jsonString(obj, "unit", QStringLiteral("px"));
    return cfg;
}

// --- Project ---

Project Project::createNew(const QString &name)
{
    Project project;
    project.name = name.isEmpty() ? QStringLiteral("未命名工程") : name;
    project.formatVersion = QString::fromUtf8(kProjectFileVersion);
    Recipe recipe = Recipe::createDefault(QStringLiteral("默认配方"));
    project.recipes.push_back(recipe);
    project.activeRecipeId = recipe.id;
    return project;
}

int Project::indexOfRecipe(const QString &recipeId) const
{
    for (int i = 0; i < recipes.size(); ++i) {
        if (recipes.at(i).id == recipeId) {
            return i;
        }
    }
    return -1;
}

Recipe *Project::findRecipe(const QString &recipeId)
{
    const int idx = indexOfRecipe(recipeId);
    return idx >= 0 ? &recipes[idx] : nullptr;
}

const Recipe *Project::findRecipe(const QString &recipeId) const
{
    const int idx = indexOfRecipe(recipeId);
    return idx >= 0 ? &recipes.at(idx) : nullptr;
}

QJsonObject Project::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("format"), QString::fromUtf8(kProjectFormat));
    obj.insert(QStringLiteral("version"), formatVersion);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("imageSource"), imageSource.toJson());
    obj.insert(QStringLiteral("calibration"), calibration.toJson());
    obj.insert(QStringLiteral("activeRecipeId"), activeRecipeId);
    QJsonArray arr;
    for (const Recipe &r : recipes) {
        arr.append(r.toJson());
    }
    obj.insert(QStringLiteral("recipes"), arr);
    return obj;
}

Project Project::fromJson(const QJsonObject &obj, QString *error)
{
    const QString format = jsonString(obj, "format");
    if (!format.isEmpty() && format != QLatin1String(kProjectFormat)) {
        if (error) {
            *error = QStringLiteral("不是有效的 MeasureExpert 工程文件（format=%1）").arg(format);
        }
        return {};
    }

    Project project;
    project.name = jsonString(obj, "name", QStringLiteral("未命名工程"));
    project.formatVersion = jsonString(obj, "version", QString::fromUtf8(kProjectFileVersion));
    project.imageSource = ImageSourceConfig::fromJson(obj.value(QStringLiteral("imageSource")).toObject());
    project.calibration = CalibrationConfig::fromJson(obj.value(QStringLiteral("calibration")).toObject());
    project.activeRecipeId = jsonString(obj, "activeRecipeId");

    const QJsonArray arr = obj.value(QStringLiteral("recipes")).toArray();
    for (const QJsonValue &v : arr) {
        if (v.isObject()) {
            project.recipes.push_back(Recipe::fromJson(v.toObject()));
        }
    }

    // 兼容空配方或 activeRecipeId 失效的旧文件
    if (project.recipes.isEmpty()) {
        Recipe recipe = Recipe::createDefault(QStringLiteral("默认配方"));
        project.recipes.push_back(recipe);
        project.activeRecipeId = recipe.id;
    } else if (project.indexOfRecipe(project.activeRecipeId) < 0) {
        project.activeRecipeId = project.recipes.first().id;
    }

    return project;
}

} // namespace Me
