#ifndef MANAGERECIPESDIALOG_H
#define MANAGERECIPESDIALOG_H

#include <QDialog>

class QListWidget;
class QPushButton;

namespace Me {
class ProjectService;
}

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
    QString currentId() const;

    Me::ProjectService *m_service = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_activateBtn = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
};

#endif // MANAGERECIPESDIALOG_H
