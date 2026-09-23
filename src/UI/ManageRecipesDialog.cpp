// ManageRecipesDialog — 配方增删改与切换当前配方

#include "UI/ManageRecipesDialog.h"

#include "Core/ProjectService.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ManageRecipesDialog::ManageRecipesDialog(Me::ProjectService *service, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
{
    setWindowTitle(tr("管理配方"));
    setMinimumSize(420, 360);
    setModal(true);

    auto *hint = new QLabel(tr("选择配方并设为当前；测量项列表会跟随当前配方。"), this);
    hint->setWordWrap(true);

    m_list = new QListWidget(this);

    m_activateBtn = new QPushButton(tr("设为当前"), this);
    m_addBtn = new QPushButton(tr("添加"), this);
    m_renameBtn = new QPushButton(tr("重命名"), this);
    m_removeBtn = new QPushButton(tr("删除"), this);
    auto *closeBtn = new QPushButton(tr("关闭"), this);

    auto *btnCol = new QVBoxLayout();
    btnCol->addWidget(m_activateBtn);
    btnCol->addWidget(m_addBtn);
    btnCol->addWidget(m_renameBtn);
    btnCol->addWidget(m_removeBtn);
    btnCol->addStretch(1);
    btnCol->addWidget(closeBtn);

    auto *mid = new QHBoxLayout();
    mid->addWidget(m_list, 1);
    mid->addLayout(btnCol);

    auto *root = new QVBoxLayout(this);
    root->addWidget(hint);
    root->addLayout(mid, 1);

    connect(m_activateBtn, &QPushButton::clicked, this, &ManageRecipesDialog::onActivate);
    connect(m_addBtn, &QPushButton::clicked, this, &ManageRecipesDialog::onAdd);
    connect(m_renameBtn, &QPushButton::clicked, this, &ManageRecipesDialog::onRename);
    connect(m_removeBtn, &QPushButton::clicked, this, &ManageRecipesDialog::onRemove);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onActivate();
    });
    connect(m_service, &Me::ProjectService::structureChanged,
            this, &ManageRecipesDialog::refreshList);

    refreshList();
}

QString ManageRecipesDialog::currentId() const
{
    auto *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ManageRecipesDialog::refreshList()
{
    // 刷新后尽量保持原选中项；标记 [当前] 配方
    const QString keep = currentId().isEmpty()
        ? m_service->project().activeRecipeId
        : currentId();
    const QString activeId = m_service->project().activeRecipeId;

    m_list->clear();
    int selectRow = 0;
    for (const auto &recipe : m_service->project().recipes) {
        QString text = recipe.name;
        if (recipe.id == activeId) {
            text += tr("  [当前]");
        }
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, recipe.id);
        if (!recipe.alignment.modelFile.isEmpty()) {
            item->setToolTip(tr("模型: %1").arg(recipe.alignment.modelFile));
        }
        if (recipe.id == keep) {
            selectRow = m_list->count() - 1;
        }
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(selectRow);
    }

    const bool multi = m_service->project().recipes.size() > 1;
    m_removeBtn->setEnabled(multi);
}

void ManageRecipesDialog::onActivate()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    m_service->setActiveRecipeId(id);
    refreshList();
}

void ManageRecipesDialog::onAdd()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("添加配方"), tr("配方名称:"), QLineEdit::Normal, tr("新配方"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_service->addRecipe(name.trimmed());
}

void ManageRecipesDialog::onRename()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    const auto *recipe = m_service->project().findRecipe(id);
    if (!recipe) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("重命名配方"), tr("配方名称:"), QLineEdit::Normal, recipe->name, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_service->renameRecipe(id, name.trimmed());
}

void ManageRecipesDialog::onRemove()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    // 至少保留一个配方，避免工程无 activeRecipe
    if (m_service->project().recipes.size() <= 1) {
        QMessageBox::information(this, tr("提示"), tr("至少保留一个配方。"));
        return;
    }
    if (QMessageBox::question(this, tr("删除配方"), tr("确定删除所选配方？"))
        != QMessageBox::Yes) {
        return;
    }
    m_service->removeRecipe(id);
}
