#ifndef MANAGERECIPESDIALOG_H
#define MANAGERECIPESDIALOG_H

#include <QDialog>

class QListWidget;
class QPushButton;

namespace Me {
class ProjectService;
}

/**
 * 配方管理对话框：切换当前配方、增删改名称。
 *
 * 当前配方决定左侧测量项列表内容；至少保留一个配方。
 * 监听 ProjectService::structureChanged 自动刷新列表。
 */
class ManageRecipesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ManageRecipesDialog(Me::ProjectService *service, QWidget *parent = nullptr);

private slots:
    void refreshList();
    void onActivate();
    void onAdd();
    void onRename();
    void onRemove();

private:
    /** 列表当前选中项对应的配方 id（UserRole） */
    QString currentId() const;

    Me::ProjectService *m_service = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_activateBtn = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
};

#endif // MANAGERECIPESDIALOG_H
