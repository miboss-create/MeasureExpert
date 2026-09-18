#include "LogWidget.h"

#include "LogSink.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(280, 160);
    initWidget();
}

bool LogWidget::initWidget()
{
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setPlaceholderText(tr("运行日志"));

    m_clearBtn = new QPushButton(tr("清空日志"), this);
    m_clearBtn->setFixedHeight(28);

    auto *btnRow = new QWidget(this);
    auto *btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(4, 2, 4, 2);
    btnLayout->setSpacing(4);
    btnLayout->addStretch(1);
    btnLayout->addWidget(m_clearBtn);
    btnRow->setFixedHeight(32);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_browser, 1);
    mainLayout->addWidget(btnRow);

    connect(m_clearBtn, &QPushButton::clicked, this, &LogWidget::onClearClicked);
    connect(LogSink::instance(),
            &LogSink::htmlMessageReady,
            m_browser,
            &QTextBrowser::append,
            Qt::QueuedConnection);

    return true;
}

void LogWidget::appendInfo(const QString &text)
{
    qInfo().noquote() << text;
}

void LogWidget::clearView()
{
    m_browser->clear();
    qInfo().noquote() << tr("已清空日志显示");
}

void LogWidget::onClearClicked()
{
    clearView();
}
