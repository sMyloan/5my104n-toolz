#include "ScriptRunner.h"
#include "AnsiToHtml.h"
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>
#include <QDebug>

ScriptRunner::ScriptRunner(const QStringList& cmd, const QString& workdir,
                           const QMap<QString, QString>& env, QObject* parent)
: QObject(parent), m_cmd(cmd), m_workdir(workdir), m_env(env) {}

void ScriptRunner::start() {
    m_proc = new QProcess(this);
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
    //отключение буферизации python чтобы вывод шел в реальном времени
    pe.insert("PYTHONUNBUFFERED", "1");
    //форсирование 256-цветного терминала для корректных ansi escape кодов
    pe.insert("TERM", "xterm-256color");
    for (auto it = m_env.begin(); it != m_env.end(); ++it)
        pe.insert(it.key(), it.value());
    m_proc->setProcessEnvironment(pe);
    m_proc->setWorkingDirectory(m_workdir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, &ScriptRunner::onReadyRead);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptRunner::onFinished);
    QStringList args = m_cmd.mid(1);
    if (m_cmd.first().contains("python")) {
        args.prepend("-u");
    }
    m_proc->start(m_cmd.first(), args);
    if (!m_proc->waitForStarted(3000)) {
        emit outputReady("\n[!] Launch error: process did not start\n");
        emit finishedWithCode(-1);
    }
}

void ScriptRunner::onReadyRead() {
    if (m_stopRequested) return;
    QByteArray data = m_proc->readAll();
    if (data.isEmpty()) return;
    QString decoded = QString::fromUtf8(data);
    //удаление escape последовательностей очистки экрана
    static const QRegularExpression clearScreenRe(R"(\x1B\[[0-9;?]*[HJK])");
    decoded.remove(clearScreenRe);
    if (decoded.isEmpty()) return;
    //обработка кареток возврата для прогрессбаров и счетчиков
    if (decoded.contains('\r') && !decoded.endsWith('\n')) {
        flushPending();
        QString html = AnsiToHtml::convert(decoded, m_themeKey);
        if (html.startsWith("__REPLACE_LAST__")) {
            emit replaceLast(html.mid(18));
            return;
        }
    }
    m_pending += decoded;
    //буферизация крупных кусков вывода для снижения нагрузки на ui
    if (m_pending.size() >= 4096) {
        flushPending();
        return;
    }
    if (!m_flushTimer) {
        m_flushTimer = new QTimer(this);
        m_flushTimer->setSingleShot(true);
        connect(m_flushTimer, &QTimer::timeout, this, &ScriptRunner::flushPending);
    }
    if (!m_flushTimer->isActive()) m_flushTimer->start(30);
}

void ScriptRunner::flushPending() {
    if (m_flushTimer && m_flushTimer->isActive()) m_flushTimer->stop();
    if (m_pending.isEmpty()) return;
    QString chunk;
    chunk.swap(m_pending);
    emit outputReady(chunk);
}

void ScriptRunner::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    flushPending();
    if (m_proc) {
        QByteArray remaining = m_proc->readAll();
        if (!remaining.isEmpty()) {
            QString decoded = QString::fromUtf8(remaining);
            emit outputReady(decoded);
        }
    }
    emit finishedWithCode(exitCode);
}

void ScriptRunner::requestStop() {
    m_stopRequested = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
    }
}

void ScriptRunner::sendInput(const QString& text) {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->write(text.toUtf8());
    }
}

bool ScriptRunner::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}
