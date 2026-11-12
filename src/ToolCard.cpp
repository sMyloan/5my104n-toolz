#include "ToolCard.h"
#include "MainWindow.h"
#include "SmyFormat.h"
#include "I18N.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonArray>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <QStyle>

namespace {

class DependencyCheckTask : public QRunnable {
public:
    DependencyCheckTask(const QJsonObject& manifest, ToolCard* card)
        : m_manifest(manifest), m_card(card) {}

    void run() override {
        auto missing = SmyFormat::checkDependencies(m_manifest);
        int total = 0;
        for (auto it = missing.constBegin(); it != missing.constEnd(); ++it)
            total += it.value().size();
        if (!m_card) return;
        QMetaObject::invokeMethod(m_card.data(), [card = m_card, total]() {
            if (card) card->setDependencyStatus(total);
        }, Qt::QueuedConnection);
    }

private:
    QJsonObject m_manifest;
    QPointer<ToolCard> m_card;
};

bool hasDeclaredDependencies(const QJsonObject& manifest) {
    const QJsonObject deps = manifest.value("dependencies").toObject();
    return !deps.value("apt").toArray().isEmpty()
        || !deps.value("pip").toArray().isEmpty()
        || !deps.value("binary").toArray().isEmpty();
}

}

ToolCard::ToolCard(const QString& smyPath, const QJsonObject& manifest, MainWindow* parent)
    : QFrame(parent), m_smyPath(smyPath), m_manifest(manifest), m_parentWin(parent)
{
    setObjectName("card");
    QString name = manifest.value("name").toString("???");
    QString desc = manifest.value("description").toString();
    QString author = manifest.value("author").toString("5My104n");
    QString cat = manifest.value("category").toString("misc");
    m_metaPrefix = QString("<i>by %1</i> | %2 | ").arg(author.toHtmlEscaped(), cat.toHtmlEscaped());

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    auto* title = new QLabel(QString("<b>[%1]</b> -- %2").arg(name.toHtmlEscaped(), desc.toHtmlEscaped()));
    title->setStyleSheet("font-size: 15px;");
    layout->addWidget(title);
    m_metaLabel = new QLabel(m_metaPrefix + trn("checking_deps"));
    m_metaLabel->setStyleSheet("font-size: 12px; opacity: 0.7;");
    layout->addWidget(m_metaLabel);
    auto* btnRow = new QHBoxLayout();
    auto* btnRun = new QPushButton(trn("run"));
    connect(btnRun, &QPushButton::clicked, this, &ToolCard::onRun);
    auto* btnCheck = new QPushButton(trn("check_deps"));
    connect(btnCheck, &QPushButton::clicked, this, &ToolCard::onCheck);
    auto* btnExtract = new QPushButton(trn("extract"));
    connect(btnExtract, &QPushButton::clicked, this, &ToolCard::onExtract);
    auto* btnTerminal = new QPushButton(trn("terminal"));
    connect(btnTerminal, &QPushButton::clicked, this, &ToolCard::onTerminal);
    for (auto* b : {btnRun, btnCheck, btnExtract, btnTerminal}) btnRow->addWidget(b);
    layout->addLayout(btnRow);

    if (hasDeclaredDependencies(manifest))
        QThreadPool::globalInstance()->start(new DependencyCheckTask(manifest, this));
    else
        setDependencyStatus(0);
}

void ToolCard::setDependencyStatus(int totalMissing) {
    const QString status = (totalMissing == 0)
        ? trn("deps_ok")
        : trn("deps_missing").arg(totalMissing);
    if (m_metaLabel) m_metaLabel->setText(m_metaPrefix + status);

    const QString state = (totalMissing == 0) ? "ok" : "warn";
    if (property("state").toString() != state) {
        setProperty("state", state);
        style()->unpolish(this);
        style()->polish(this);
    }
}

void ToolCard::onRun() { m_parentWin->runTool(m_smyPath, m_manifest); }
void ToolCard::onCheck() { m_parentWin->checkTool(m_manifest); }
void ToolCard::onExtract() { m_parentWin->extractTool(m_smyPath, m_manifest); }
void ToolCard::onTerminal() { m_parentWin->runInTerminal(m_smyPath, m_manifest); }
