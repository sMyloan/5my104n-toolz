#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QMap>

class QTimer;

class ScriptRunner : public QObject {
    Q_OBJECT
public:
    explicit ScriptRunner(const QStringList& cmd, const QString& workdir,
                          const QMap<QString, QString>& env = {},
                          QObject* parent = nullptr);
    void start();
    void requestStop();
    void sendInput(const QString& text);
    void setThemeKey(const QString& k) { m_themeKey = k; }
    bool isRunning() const;
signals:
    void outputReady(const QString& text);
    void replaceLast(const QString& html);
    void finishedWithCode(int code);
private slots:
    void onReadyRead();
    void flushPending();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
private:
    QStringList m_cmd;
    QString m_workdir;
    QMap<QString, QString> m_env;
    QProcess* m_proc = nullptr;
    QString m_pending;
    QTimer* m_flushTimer = nullptr;
    QString m_themeKey = "soft_dark";
    bool m_stopRequested = false;
};