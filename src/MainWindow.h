#pragma once
#include "ScriptEditor.h"
#include "SettingsPanel.h"
#include <QMainWindow>
#include <QCloseEvent>
#include <QJsonObject>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPointer>

class FolderScanner;
class ScriptRunner;
class RadarWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void runTool(const QString& smyPath, const QJsonObject& manifest);
    void checkTool(const QJsonObject& manifest);
    void extractTool(const QString& smyPath, const QJsonObject& manifest);
    void runInTerminal(const QString& smyPath, const QJsonObject& manifest);
    
    QString userName() const { return m_userName; }
    QString defaultPassword() const { return m_defaultPassword; }
    bool defaultPasswordEnabled() const { return m_defaultPasswordEnabled; }

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void pickFolder();
    void openCompiler();
    void openRadar();
    void showStatistics();
    void exportLog();
    void onScanFinished(const QStringList& paths);
    void onScanError(const QString& msg);
    void onRunnerOutput(const QString& text);
    void onRunnerReplaceLast(const QString& html);
    void onRunnerFinished(int code);
    void sendStdin();
    void stopRunner();
    void toggleSettings();
    void onSettingsClosed();

private:
    void buildUi();
    void applyTheme(const QString& key);
    void applyLanguage(const QString& lang);
    void loadToolsAsync(const QString& folder);
    void log(const QString& text);
    void setStatus(const QString& key);
    void forceCleanup();
    void trackUsage(const QString& toolName);
    QJsonObject loadConfig();
    void saveConfig(const QJsonObject& cfg);
    void openEditor();

    QString m_toolsDir;
    QJsonObject m_config;
    FolderScanner* m_scanner = nullptr;
    QPointer<ScriptRunner> m_runner;
    QString m_currentWorkdir;
    RadarWindow* m_radarWindow = nullptr;
    QLabel* m_lblTitle = nullptr;
    QPushButton* m_btnFolder = nullptr;
    QPushButton* m_btnCompiler = nullptr;
    QPushButton* m_btnRadar = nullptr;
    QPushButton* m_btnEditor = nullptr;
    QPushButton* m_btnSettings = nullptr;
    QGroupBox* m_logGroup = nullptr;
    QTextEdit* m_logEdit = nullptr;
    QLineEdit* m_leStdin = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;
    ScriptEditor* m_scriptEditor = nullptr;
    SettingsPanel* m_settingsPanel = nullptr;
    QWidget* m_overlay = nullptr;
    QLabel* m_statusLabel = nullptr;
    QString m_statusKey = "ready";
    bool m_showTimestamps = true;
    
    QString m_userName;
    QString m_defaultPassword;
    bool m_defaultPasswordEnabled = false;
};