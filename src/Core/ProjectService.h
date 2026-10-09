#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

/**
 * @file ProjectService.h
 * @brief 工程生命周期与 CRUD 服务层，供 MainWindow 等 UI 调用。
 *
 * 持有当前 Project 内存副本，负责新建/加载/保存 .mexp.json，
 * 并通过 Qt 信号通知 UI 刷新与脏标记变化。
 */

#include "Core/ProjectTypes.h"

#include <QObject>
#include <QString>

namespace Me {

/**
 * 工程服务（单例式由 MainWindow 持有）。
 *
 * 所有修改性操作会 markDirty 并 emit structureChanged；
 * 加载/新建会 emit projectReset。
 */
class ProjectService : public QObject
{
    Q_OBJECT
public:
    explicit ProjectService(QObject *parent = nullptr);

    const Project &project() const { return m_project; }
    Project &project() { return m_project; }

    QString filePath() const { return m_filePath; }
    bool hasFilePath() const { return !m_filePath.isEmpty(); }
    bool isDirty() const { return m_dirty; }

    /** 新建空白工程（含默认配方） */
    void createNew(const QString &name = QString());
    bool loadFromFile(const QString &path, QString *error = nullptr);
    bool saveToFile(const QString &path, QString *error = nullptr);
    /** 保存到已记录的 m_filePath；无路径时失败 */
    bool save(QString *error = nullptr);

    void setDirty(bool dirty);
    void markDirty() { setDirty(true); }

    void setProjectName(const QString &name);
    void setImageSourceFolder(const QString &folderPath);
    void setImageSourceCamera(const QString &backendId,
                              const QString &deviceId,
                              const QString &displayName);
    void setActiveRecipeId(const QString &recipeId, bool markAsDirty = true);

    Recipe *activeRecipe();
    const Recipe *activeRecipe() const;

    /** @return 新配方 id；并设为 activeRecipeId */
    QString addRecipe(const QString &name);
    /** 至少保留一条配方 */
    bool removeRecipe(const QString &recipeId);
    bool renameRecipe(const QString &recipeId, const QString &name);

    QString addMeasure(const QString &recipeId, const QString &name, MeasureType type);
    bool removeMeasure(const QString &recipeId, const QString &measureId);
    bool renameMeasure(const QString &recipeId, const QString &measureId, const QString &name);
    bool updateMeasure(const QString &recipeId, const MeasureItem &item);

    void setCalibration(const CalibrationConfig &config);
    bool updateAlignment(const QString &recipeId, const AlignmentConfig &config);

signals:
    void projectReset();
    void projectLoaded(const QString &path);
    void projectSaved(const QString &path);
    void dirtyChanged(bool dirty);
    void structureChanged();

private:
    /** 原子替换工程并广播 reset/structure/dirty */
    void replaceProject(Project project, const QString &filePath, bool dirty);

    Project m_project;
    QString m_filePath;
    bool m_dirty = false;
};

} // namespace Me

#endif // PROJECTSERVICE_H
