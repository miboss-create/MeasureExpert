#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include "Core/ProjectTypes.h"

#include <QObject>
#include <QString>

namespace Me {

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

    void createNew(const QString &name = QString());
    bool loadFromFile(const QString &path, QString *error = nullptr);
    bool saveToFile(const QString &path, QString *error = nullptr);
    bool save(QString *error = nullptr);

    void setDirty(bool dirty);
    void markDirty() { setDirty(true); }

    void setProjectName(const QString &name);
    void setImageSourceFolder(const QString &folderPath);
    void setActiveRecipeId(const QString &recipeId, bool markAsDirty = true);

    Recipe *activeRecipe();
    const Recipe *activeRecipe() const;

    QString addRecipe(const QString &name);
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
    void replaceProject(Project project, const QString &filePath, bool dirty);

    Project m_project;
    QString m_filePath;
    bool m_dirty = false;
};

} // namespace Me

#endif // PROJECTSERVICE_H
