/**
 * @file ProjectService.cpp
 * @brief ProjectService 实现：JSON 文件 IO 与配方/测量项编辑。
 */

#include "Core/ProjectService.h"

#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QSaveFile>

#include <utility>

namespace Me {

ProjectService::ProjectService(QObject *parent)
    : QObject(parent)
{
    m_project = Project::createNew();
}

void ProjectService::replaceProject(Project project, const QString &filePath, bool dirty)
{
    m_project = std::move(project);
    m_filePath = filePath;
    const bool dirtyChangedFlag = (m_dirty != dirty);
    m_dirty = dirty;
    emit projectReset();
    emit structureChanged();
    if (dirtyChangedFlag) {
        emit dirtyChanged(m_dirty);
    }
}

// --- 工程文件 IO ---

void ProjectService::createNew(const QString &name)
{
    replaceProject(Project::createNew(name), {}, true);
}

bool ProjectService::loadFromFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("无法打开文件: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        }
        return false;
    }

    QString modelError;
    Project project = Project::fromJson(doc.object(), &modelError);
    if (!modelError.isEmpty()) {
        if (error) {
            *error = modelError;
        }
        return false;
    }

    replaceProject(std::move(project), path, false);
    emit projectLoaded(path);
    return true;
}

bool ProjectService::saveToFile(const QString &path, QString *error)
{
    // QSaveFile 保证原子写入（commit 前写临时文件）
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("无法写入文件: %1").arg(file.errorString());
        }
        return false;
    }

    const QJsonDocument doc(m_project.toJson());
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = QStringLiteral("写入失败: %1").arg(file.errorString());
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("保存失败: %1").arg(file.errorString());
        }
        return false;
    }

    m_filePath = path;
    setDirty(false);
    emit projectSaved(path);
    return true;
}

bool ProjectService::save(QString *error)
{
    if (m_filePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("未指定工程路径");
        }
        return false;
    }
    return saveToFile(m_filePath, error);
}

// --- 脏标记与基础属性 ---

void ProjectService::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

void ProjectService::setProjectName(const QString &name)
{
    if (m_project.name == name) {
        return;
    }
    m_project.name = name;
    markDirty();
    emit structureChanged();
}

void ProjectService::setImageSourceFolder(const QString &folderPath)
{
    m_project.imageSource.type = ImageSourceType::Folder;
    if (m_project.imageSource.path == folderPath) {
        return;
    }
    m_project.imageSource.path = folderPath;
    markDirty();
}

void ProjectService::setActiveRecipeId(const QString &recipeId, bool markAsDirty)
{
    if (m_project.activeRecipeId == recipeId) {
        return;
    }
    if (m_project.indexOfRecipe(recipeId) < 0) {
        return;
    }
    m_project.activeRecipeId = recipeId;
    if (markAsDirty) {
        this->markDirty();
    }
    emit structureChanged();
}

Recipe *ProjectService::activeRecipe()
{
    return m_project.findRecipe(m_project.activeRecipeId);
}

const Recipe *ProjectService::activeRecipe() const
{
    return m_project.findRecipe(m_project.activeRecipeId);
}

// --- 配方 CRUD ---

QString ProjectService::addRecipe(const QString &name)
{
    Recipe recipe = Recipe::createDefault(name);
    m_project.recipes.push_back(recipe);
    m_project.activeRecipeId = recipe.id;
    markDirty();
    emit structureChanged();
    return recipe.id;
}

bool ProjectService::removeRecipe(const QString &recipeId)
{
    const int idx = m_project.indexOfRecipe(recipeId);
    if (idx < 0) {
        return false;
    }
    if (m_project.recipes.size() <= 1) {
        return false;
    }
    m_project.recipes.removeAt(idx);
    if (m_project.activeRecipeId == recipeId) {
        m_project.activeRecipeId = m_project.recipes.first().id;
    }
    markDirty();
    emit structureChanged();
    return true;
}

bool ProjectService::renameRecipe(const QString &recipeId, const QString &name)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe || recipe->name == name) {
        return false;
    }
    recipe->name = name;
    markDirty();
    emit structureChanged();
    return true;
}

// --- 测量项 CRUD ---

QString ProjectService::addMeasure(const QString &recipeId, const QString &name, MeasureType type)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe) {
        return {};
    }
    MeasureItem item = MeasureItem::createDefault(name);
    item.type = type;
    recipe->measures.push_back(item);
    markDirty();
    emit structureChanged();
    return item.id;
}

bool ProjectService::removeMeasure(const QString &recipeId, const QString &measureId)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe) {
        return false;
    }
    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        return false;
    }
    recipe->measures.removeAt(idx);
    markDirty();
    emit structureChanged();
    return true;
}

bool ProjectService::renameMeasure(const QString &recipeId, const QString &measureId, const QString &name)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe) {
        return false;
    }
    const int idx = recipe->indexOfMeasure(measureId);
    if (idx < 0) {
        return false;
    }
    if (recipe->measures[idx].name == name) {
        return false;
    }
    recipe->measures[idx].name = name;
    markDirty();
    emit structureChanged();
    return true;
}

bool ProjectService::updateMeasure(const QString &recipeId, const MeasureItem &item)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe) {
        return false;
    }
    const int idx = recipe->indexOfMeasure(item.id);
    if (idx < 0) {
        return false;
    }
    recipe->measures[idx] = item;
    markDirty();
    emit structureChanged();
    return true;
}

// --- 标定与定位配置 ---

void ProjectService::setCalibration(const CalibrationConfig &config)
{
    m_project.calibration = config;
    markDirty();
    emit structureChanged();
}

bool ProjectService::updateAlignment(const QString &recipeId, const AlignmentConfig &config)
{
    Recipe *recipe = m_project.findRecipe(recipeId);
    if (!recipe) {
        return false;
    }
    recipe->alignment = config;
    markDirty();
    emit structureChanged();
    return true;
}

} // namespace Me
