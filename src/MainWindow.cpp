#include "MainWindow.h"
#include "ToolCard.h"
#include "CompilerDialog.h"
#include "FolderScanner.h"
#include "ScriptRunner.h"
#include "RadarWidget.h"
#include "SmyFormat.h"
#include "ThemeManager.h"
#include "I18N.h"
#include "SettingsPanel.h"
#include "AnsiToHtml.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextCursor>
#include <QFont>
#include <QTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QCloseEvent>
#include <QInputDialog>
#include <QStatusBar>
#include <QTime>
#include <unistd.h>
#include <QDebug>

static QString configDir() { return QDir::homePath() + "/.config/5my104n_toolz"; }
static QString configFile() { return configDir() + "/settings.json"; }

QJsonObject MainWindow::loadConfig() {
    QFile f(configFile());
    if (f.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) return doc.object();
    }
    QJsonObject def;
    def["theme"] = "soft_dark";
    def["language"] = "en";
    return def;
}

void MainWindow::saveConfig(const QJsonObject& cfg) {
    QDir().mkpath(configDir());
    QFile f(configFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    //версия берется из i18n чтобы не было рассинхрона
    setWindowTitle(trn("version"));
    setMinimumSize(1100, 750);
    m_config = loadConfig();
    m_showTimestamps = m_config.value("timestamps").toBool(true);
    I18N::instance().setLanguage(m_config.value("language").toString("en"));
    buildUi();
    applyTheme(m_config.value("theme").toString("soft_dark"));
    applyLanguage(m_config.value("language").toString("en"));
    log("[*] 5MY104N_TOOLZ started.");
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setSpacing(8);
    
    auto* titleRow = new QHBoxLayout();
    m_lblTitle = new QLabel(I18N::instance().tr("title"));
    m_lblTitle->setObjectName("title");
    titleRow->addWidget(m_lblTitle);
    titleRow->addStretch();
    m_btnSettings = new QPushButton("⚙");
    m_btnSettings->setFixedSize(40, 40);
    m_btnSettings->setToolTip(I18N::instance().tr("settings"));
    connect(m_btnSettings, &QPushButton::clicked, this, &MainWindow::toggleSettings);
    titleRow->addWidget(m_btnSettings);
    root->addLayout(titleRow);
    
    auto* top = new QHBoxLayout();
    m_btnFolder = new QPushButton(I18N::instance().tr("select_folder"));
    connect(m_btnFolder, &QPushButton::clicked, this, &MainWindow::pickFolder);
    m_btnCompiler = new QPushButton(I18N::instance().tr("compiler"));
    connect(m_btnCompiler, &QPushButton::clicked, this, &MainWindow::openCompiler);
    m_btnRadar = new QPushButton(I18N::instance().tr("radar"));
    connect(m_btnRadar, &QPushButton::clicked, this, &MainWindow::openRadar);
    m_btnEditor = new QPushButton(I18N::instance().tr("editor"));
    connect(m_btnEditor, &QPushButton::clicked, this, &MainWindow::openEditor);
    for (auto* b : {m_btnFolder, m_btnCompiler, m_btnRadar, m_btnEditor})
        top->addWidget(b);
        root->addLayout(top);
        
        auto* splitter = new QSplitter(Qt::Vertical);
        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        auto* cardsContainer = new QWidget();
        m_cardsLayout = new QVBoxLayout(cardsContainer);
        m_cardsLayout->setAlignment(Qt::AlignTop);
        m_cardsLayout->setSpacing(10);
        scroll->setWidget(cardsContainer);
        splitter->addWidget(scroll);
        
        m_logGroup = new QGroupBox(I18N::instance().tr("log"));
        auto* logLayout = new QVBoxLayout(m_logGroup);
        m_logEdit = new QTextEdit();
        m_logEdit->setReadOnly(true);
        m_logEdit->setFont(QFont("Consolas", 10));
        m_logEdit->setLineWrapMode(QTextEdit::NoWrap);
        m_logEdit->document()->setMaximumBlockCount(5000);
        logLayout->addWidget(m_logEdit);
        
        auto* stdinRow = new QHBoxLayout();
        m_leStdin = new QLineEdit();
        m_leStdin->setPlaceholderText(I18N::instance().tr("stdin_placeholder"));
        connect(m_leStdin, &QLineEdit::returnPressed, this, &MainWindow::sendStdin);
        auto* btnSend = new QPushButton(I18N::instance().tr("send"));
        connect(btnSend, &QPushButton::clicked, this, &MainWindow::sendStdin);
        auto* btnStop = new QPushButton(I18N::instance().tr("stop"));
        btnStop->setObjectName("danger");
        connect(btnStop, &QPushButton::clicked, this, &MainWindow::stopRunner);
        stdinRow->addWidget(m_leStdin);
        stdinRow->addWidget(btnSend);
        stdinRow->addWidget(btnStop);
        logLayout->addLayout(stdinRow);
        splitter->addWidget(m_logGroup);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 1);
        root->addWidget(splitter);
        
        m_statusLabel = new QLabel(this);
        m_statusLabel->setObjectName("status");
        statusBar()->addWidget(m_statusLabel, 1);
        statusBar()->setSizeGripEnabled(true);
        setStatus("ready");
        
        m_overlay = new QWidget(central);
        m_overlay->setStyleSheet("background-color:rgba(0,0,0,0.6);");
        m_overlay->hide();
        m_overlay->installEventFilter(this);
        
        m_settingsPanel = new SettingsPanel(central);
        m_settingsPanel->hide();
        
        m_userName = m_config.value("user_name").toString("");
        m_defaultPassword = m_config.value("default_password").toString("");
        m_defaultPasswordEnabled = m_config.value("default_password_enabled").toBool(false);
        
        m_settingsPanel->setUserName(m_userName);
        m_settingsPanel->setDefaultPassword(m_defaultPassword);
        m_settingsPanel->setDefaultPasswordEnabled(m_defaultPasswordEnabled);
        m_settingsPanel->setTheme(m_config.value("theme").toString("soft_dark"));
        m_settingsPanel->setLanguage(m_config.value("language").toString("en"));
        m_settingsPanel->setTimestamps(m_showTimestamps);
        
        connect(m_settingsPanel, &SettingsPanel::userNameChanged, this, [this](const QString& name) {
            m_userName = name; m_config["user_name"] = name; saveConfig(m_config);
        });
        connect(m_settingsPanel, &SettingsPanel::defaultPasswordChanged, this, [this](const QString& pwd) {
            m_defaultPassword = pwd; m_config["default_password"] = pwd; saveConfig(m_config);
        });
        connect(m_settingsPanel, &SettingsPanel::defaultPasswordToggled, this, [this](bool enabled) {
            m_defaultPasswordEnabled = enabled; m_config["default_password_enabled"] = enabled; saveConfig(m_config);
        });
        connect(m_settingsPanel, &SettingsPanel::themeChanged, this, [this](const QString& key) {
            m_config["theme"] = key; saveConfig(m_config); applyTheme(key);
        });
        connect(m_settingsPanel, &SettingsPanel::languageChanged, this, [this](const QString& lang) {
            m_config["language"] = lang; saveConfig(m_config); applyLanguage(lang);
        });
        connect(m_settingsPanel, &SettingsPanel::timestampsToggled, this, [this](bool enabled) {
            m_showTimestamps = enabled; m_config["timestamps"] = enabled; saveConfig(m_config);
        });
        connect(m_settingsPanel, &SettingsPanel::exportLogRequested, this, &MainWindow::exportLog);
        connect(m_settingsPanel, &SettingsPanel::showStatsRequested, this, [this]() {
            onSettingsClosed(); showStatistics();
        });
        connect(m_settingsPanel, &SettingsPanel::closeRequested, this, &MainWindow::onSettingsClosed);
}

void MainWindow::applyTheme(const QString& key) {
    //убран кастомный стиль кнопок теперь цвета берутся из глобального qss
    ThemeManager::instance().apply(key);
    if (m_radarWindow) m_radarWindow->setTheme(key);
    if (m_scriptEditor) m_scriptEditor->setTheme(key);
    if (m_settingsPanel) m_settingsPanel->setTheme(key);
}

void MainWindow::applyLanguage(const QString& lang) {
    I18N::instance().setLanguage(lang);
    setWindowTitle(trn("version"));
    m_lblTitle->setText(I18N::instance().tr("title"));
    m_btnFolder->setText(I18N::instance().tr("select_folder"));
    m_btnCompiler->setText(I18N::instance().tr("compiler"));
    m_btnRadar->setText(I18N::instance().tr("radar"));
    m_btnEditor->setText(I18N::instance().tr("editor"));
    m_btnSettings->setToolTip(I18N::instance().tr("settings"));
    m_logGroup->setTitle(I18N::instance().tr("log"));
    m_leStdin->setPlaceholderText(I18N::instance().tr("stdin_placeholder"));
    setStatus(m_statusKey);
    if (!m_toolsDir.isEmpty()) loadToolsAsync(m_toolsDir);
    if (m_settingsPanel) m_settingsPanel->setLanguage(lang);
}

void MainWindow::log(const QString& text) {
    QString textToLog = text;
    if (m_showTimestamps && !textToLog.isEmpty()) {
        const QString ts = "[" + QTime::currentTime().toString("HH:mm:ss") + "] ";
        QStringList lines = textToLog.split('\n');
        for (QString& line : lines)
            if (!line.isEmpty()) line.prepend(ts);
            textToLog = lines.join('\n');
    }
    QString html = AnsiToHtml::convert(textToLog, m_config.value("theme").toString("soft_dark"));
    html.replace("\n", "<br>");
    if (html.startsWith("__REPLACE_LAST__")) {
        html = html.mid(18);
        QTextCursor c = m_logEdit->textCursor();
        c.movePosition(QTextCursor::End);
        c.movePosition(QTextCursor::Up, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        c.insertHtml(html + "<br>");
    } else {
        m_logEdit->append(html);
    }
    m_logEdit->moveCursor(QTextCursor::End);
}

void MainWindow::setStatus(const QString& key) {
    m_statusKey = key;
    if (m_statusLabel) m_statusLabel->setText(I18N::instance().tr(key));
}

void MainWindow::pickFolder() {
    //переведенный диалог выбора папки
    QString d = QFileDialog::getExistingDirectory(this, trn("folder_dialog_title"));
    if (d.isEmpty()) return;
    m_toolsDir = d;
    loadToolsAsync(d);
}

void MainWindow::loadToolsAsync(const QString& folder) {
    while (QLayoutItem* item = m_cardsLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    auto* lbl = new QLabel(I18N::instance().tr("scanning"));
    lbl->setStyleSheet("color:#fcee0a; font-size:14px;");
    m_cardsLayout->addWidget(lbl);
    if (m_scanner) {
        disconnect(m_scanner, nullptr, this, nullptr);
        if (m_scanner->isRunning()) {
            m_scanner->requestInterruption();
            m_scanner->wait(3000);
        }
        m_scanner->deleteLater();
    }
    m_scanner = new FolderScanner(folder, this);
    connect(m_scanner, &FolderScanner::finishedScan, this, &MainWindow::onScanFinished);
    connect(m_scanner, &FolderScanner::errorOccurred, this, &MainWindow::onScanError);
    setStatus("status_scanning");
    m_scanner->start();
}

void MainWindow::onScanFinished(const QStringList& paths) {
    while (QLayoutItem* item = m_cardsLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    log(QString("[*] Scan: %1 | .smy found: %2").arg(m_toolsDir).arg(paths.size()));
    if (paths.isEmpty()) {
        m_cardsLayout->addWidget(new QLabel(I18N::instance().tr("no_files")));
        return;
    }
    for (const QString& path : paths) {
        try {
            const auto up = SmyFormat::unpack(path);
            auto* card = new ToolCard(path, up.first, this);
            if (card) m_cardsLayout->addWidget(card);
            log(I18N::instance().tr("loaded") + " " + up.first.value("name").toString("?") + " (" + QFileInfo(path).fileName() + ")");
        } catch (const std::exception& e) {
            const QString why = QString::fromUtf8(e.what());
            log("[!] " + QFileInfo(path).fileName() + ": " + why);
            if (why.contains("Wrong password") || why.contains("corrupted") || why.contains("padding")) {
                QJsonObject fakeManifest;
                fakeManifest["name"] = QFileInfo(path).baseName().toUpper();
                fakeManifest["description"] = "[ENCRYPTED]";
                fakeManifest["author"] = "???";
                fakeManifest["category"] = "locked";
                fakeManifest["encrypted"] = true;
                fakeManifest["entry"] = "script";
                fakeManifest["runtime"] = "python3";
                m_cardsLayout->addWidget(new ToolCard(path, fakeManifest, this));
            } else {
                auto* lbl = new QLabel("[!] " + QFileInfo(path).fileName() + ": " + why);
                lbl->setStyleSheet("color:#fcee0a;");
                m_cardsLayout->addWidget(lbl);
            }
        }
    }
    QApplication::processEvents();
    setStatus("ready");
}

void MainWindow::onScanError(const QString& msg) {
    log("[!] " + msg);
    setStatus("ready");
}

void MainWindow::openCompiler() {
    CompilerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && !m_toolsDir.isEmpty())
        loadToolsAsync(m_toolsDir);
}

void MainWindow::openRadar() {
    if (!m_radarWindow) {
        m_radarWindow = new RadarWindow(this);
        m_radarWindow->setTheme(m_config.value("theme").toString("soft_dark"));
    }
    m_radarWindow->show();
    m_radarWindow->raise();
    m_radarWindow->activateWindow();
}

void MainWindow::runTool(const QString& smyPath, const QJsonObject& manifest) {
    if (m_runner && m_runner->isRunning()) {
        QMessageBox::warning(this, I18N::instance().tr("busy"), I18N::instance().tr("another_running"));
        return;
    }
    QTemporaryDir tmp;
    tmp.setAutoRemove(false);
    QString workdir = tmp.path();
    QString scriptPath, password;
    m_currentWorkdir.clear();
    bool encrypted = manifest.value("encrypted").toBool(false);
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            scriptPath = SmyFormat::extract(smyPath, workdir, password);
            break;
        } catch (const std::exception& e) {
            QString msg = QString::fromUtf8(e.what());
            if ((msg.contains("Wrong password") || msg.contains("corrupted")) && encrypted) {
                bool ok = false;
                //переведенный диалог ввода пароля
                password = QInputDialog::getText(this, trn("password_dialog_title"),
                                                 trn("password_dialog_msg").arg(manifest.value("name").toString("?")),
                                                 QLineEdit::Password, "", &ok);
                if (!ok) { QDir(workdir).removeRecursively(); return; }
                continue;
            }
            QMessageBox::critical(this, I18N::instance().tr("error"), I18N::instance().tr("extract_failed") + "\n" + msg);
            QDir(workdir).removeRecursively();
            return;
        }
    }
    if (scriptPath.isEmpty()) { QDir(workdir).removeRecursively(); return; }
    QString runtime = manifest.value("runtime").toString("python3");
    QStringList cmd = runtime.split(' ', Qt::SkipEmptyParts);
    cmd << scriptPath;
    QJsonArray args = manifest.value("args").toArray();
    for (const auto& a : args) cmd << a.toString();
    m_currentWorkdir = workdir;
    log("\n" + I18N::instance().tr("running") + " " + cmd.join(' '));
    m_runner = new ScriptRunner(cmd, workdir, {}, this);
    m_runner->setThemeKey(m_config.value("theme").toString("soft_dark"));
    connect(m_runner, &ScriptRunner::outputReady, this, &MainWindow::onRunnerOutput);
    connect(m_runner, &ScriptRunner::replaceLast, this, &MainWindow::onRunnerReplaceLast);
    connect(m_runner, &ScriptRunner::finishedWithCode, this, &MainWindow::onRunnerFinished);
    m_runner->start();
    setStatus("status_running");
    trackUsage(manifest.value("name").toString("?"));
}

void MainWindow::trackUsage(const QString& toolName) {
    QString path = configDir() + "/usage_stats.json";
    QJsonObject stats;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) stats = QJsonDocument::fromJson(f.readAll()).object();
    stats["total_runs"] = stats.value("total_runs").toInt() + 1;
    QJsonObject tools = stats.value("tools_used").toObject();
    tools[toolName] = tools.value(toolName).toInt() + 1;
    stats["tools_used"] = tools;
    QDir().mkpath(configDir());
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        out.write(QJsonDocument(stats).toJson(QJsonDocument::Indented));
}

void MainWindow::onRunnerOutput(const QString& text) { log(text); }

void MainWindow::onRunnerReplaceLast(const QString& html) {
    QTextCursor c = m_logEdit->textCursor();
    c.movePosition(QTextCursor::End);
    c.movePosition(QTextCursor::Up, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.insertHtml(html + "<br>");
    m_logEdit->moveCursor(QTextCursor::End);
}

void MainWindow::onRunnerFinished(int code) {
    log("\n" + trn("finished") + " " + QString::number(code));
    if (!m_currentWorkdir.isEmpty()) { QDir(m_currentWorkdir).removeRecursively(); m_currentWorkdir.clear(); }
    if (m_runner) { m_runner->deleteLater(); m_runner = nullptr; }
    setStatus("ready");
}

void MainWindow::stopRunner() {
    if (m_runner && m_runner->isRunning()) { m_runner->requestStop(); log(I18N::instance().tr("stop_request")); }
}

void MainWindow::forceCleanup() {
    if (m_runner && m_runner->property("stopRequested").toBool() == false) m_runner->requestStop();
    if (!m_currentWorkdir.isEmpty()) { QDir(m_currentWorkdir).removeRecursively(); m_currentWorkdir.clear(); }
    if (m_runner) { m_runner->deleteLater(); m_runner = nullptr; }
    log(trn("process_terminated"));
    setStatus("ready");
}

void MainWindow::sendStdin() {
    QString text = m_leStdin->text() + "\n";
    m_leStdin->clear();
    if (m_runner && m_runner->isRunning()) { m_runner->sendInput(text); log("[>] " + text.trimmed()); }
    else { log(I18N::instance().tr("no_process")); }
}

void MainWindow::checkTool(const QJsonObject& manifest) {
    auto missing = SmyFormat::checkDependencies(manifest);
    int total = 0;
    for (const auto& v : missing) total += v.size();
    QStringList msg;
    if (missing.contains("apt") && !missing["apt"].isEmpty()) msg << "APT:\n- " + missing["apt"].join("\n- ");
    if (missing.contains("pip") && !missing["pip"].isEmpty()) msg << "PIP:\n- " + missing["pip"].join("\n- ");
    if (missing.contains("binary") && !missing["binary"].isEmpty()) msg << "Binaries:\n- " + missing["binary"].join("\n- ");
    if (total == 0) { QMessageBox::information(this, I18N::instance().tr("check"), I18N::instance().tr("all_deps_ok")); }
    else {
        QMessageBox box(this);
        box.setWindowTitle(I18N::instance().tr("check"));
        box.setText(I18N::instance().tr("missing_deps").arg(total));
        box.setDetailedText(msg.join("\n"));
        auto* btnInstall = box.addButton(I18N::instance().tr("install_missing"), QMessageBox::AcceptRole);
        box.addButton(I18N::instance().tr("cancel"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == btnInstall) {
            log(I18N::instance().tr("installing"));
            bool ok = SmyFormat::installDependencies(missing);
            if (ok) QMessageBox::information(this, I18N::instance().tr("success"), I18N::instance().tr("install_complete"));
            else QMessageBox::warning(this, I18N::instance().tr("error"), I18N::instance().tr("install_warning"));
        }
    }
}

void MainWindow::extractTool(const QString& smyPath, const QJsonObject& manifest) {
    QString entry = manifest.value("entry").toString("script");
    //переведенные диалоги сохранения
    QString outPath = QFileDialog::getSaveFileName(this, trn("save_source_script"), entry, trn("all_files_filter"));
    if (outPath.isEmpty()) return;
    QString password;
    bool encrypted = manifest.value("encrypted").toBool(false);
    QByteArray body;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            auto [_, body2] = SmyFormat::unpack(smyPath, password);
            body = body2;
            break;
        } catch (const std::exception& e) {
            QString msg = QString::fromUtf8(e.what());
            if ((msg.contains("Wrong password") || msg.contains("corrupted")) && encrypted) {
                bool ok = false;
                password = QInputDialog::getText(this, trn("password_dialog_title"),
                                                 trn("enter_password"), QLineEdit::Password, "", &ok);
                if (!ok) return;
                continue;
            }
            QMessageBox::critical(this, I18N::instance().tr("error"), msg);
            return;
        }
    }
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { 
        //переведенная ошибка записи
        QMessageBox::critical(this, I18N::instance().tr("error"), trn("cannot_write_file")); 
        return; 
    }
    f.write(body);
    f.close();
    log(I18N::instance().tr("extracted") + " " + outPath);
    QMessageBox::information(this, I18N::instance().tr("success"), I18N::instance().tr("saved") + "\n" + outPath);
}

void MainWindow::runInTerminal(const QString& smyPath, const QJsonObject& manifest) {
    QTemporaryDir tmp;
    tmp.setAutoRemove(false);
    QString workdir = tmp.path();
    QString scriptPath, password;
    bool encrypted = manifest.value("encrypted").toBool(false);
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            scriptPath = SmyFormat::extract(smyPath, workdir, password);
            break;
        } catch (const std::exception& e) {
            QString msg = QString::fromUtf8(e.what());
            if ((msg.contains("Wrong password") || msg.contains("corrupted")) && encrypted) {
                bool ok = false;
                password = QInputDialog::getText(this, trn("password_dialog_title"),
                                                 trn("enter_password"), QLineEdit::Password, "", &ok);
                if (!ok) return;
                continue;
            }
            QMessageBox::critical(this, I18N::instance().tr("error"), msg);
            return;
        }
    }
    QString runtime = manifest.value("runtime").toString("python3");
    QStringList cmd = runtime.split(' ', Qt::SkipEmptyParts) << scriptPath;
    struct Term { QStringList prefix; };
    QList<Term> terms = { {{"konsole", "--hold", "-e"}}, {{"xterm", "-hold", "-e"}}, {{"gnome-terminal", "--"}} };
    for (const auto& t : terms) {
        QProcess check;
        check.start("which", QStringList{t.prefix.first()});
        check.waitForFinished(1000);
        if (check.exitCode() == 0) {
            QStringList full = t.prefix + cmd;
            log(I18N::instance().tr("launching_terminal") + " " + full.join(' '));
            QProcess::startDetached(full.first(), full.mid(1));
            return;
        }
    }
    QMessageBox::warning(this, I18N::instance().tr("no_terminal"), I18N::instance().tr("no_terminal_msg") + "\n" + workdir);
}

void MainWindow::showStatistics() {
    QString path = configDir() + "/usage_stats.json";
    QJsonObject stats;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) stats = QJsonDocument::fromJson(f.readAll()).object();
    int totalRuns = stats.value("total_runs").toInt();
    QJsonObject tools = stats.value("tools_used").toObject();
    QString detailsText = tools.isEmpty() ? I18N::instance().tr("no_tools_used") : "";
    for (auto it = tools.begin(); it != tools.end(); ++it) detailsText += QString("  • %1: %2\n").arg(it.key()).arg(it.value().toInt());
    QString mainText = QString("<b>%1</b><br><br>%2: <b>%3</b>").arg(I18N::instance().tr("stats_title")).arg(I18N::instance().tr("total_runs")).arg(totalRuns);
    QMessageBox box(this);
    box.setWindowTitle(I18N::instance().tr("statistics"));
    box.setText(mainText);
    box.setInformativeText(I18N::instance().tr("show_details_hint"));
    auto* btnDetails = box.addButton(I18N::instance().tr("show_details"), QMessageBox::ActionRole);
    auto* btnClear = box.addButton(I18N::instance().tr("clear_statistics"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.exec();
    if (box.clickedButton() == btnDetails) {
        QMessageBox detailsBox(this);
        detailsBox.setWindowTitle(I18N::instance().tr("details"));
        detailsBox.setText(QString("<b>%1</b><br><br>%2").arg(I18N::instance().tr("tools_used")).arg(detailsText));
        detailsBox.setTextFormat(Qt::RichText);
        detailsBox.exec();
    } else if (box.clickedButton() == btnClear) {
        QFile::remove(path);
        log("[*] " + I18N::instance().tr("stats_cleared"));
    }
}

void MainWindow::openEditor() {
    if (!m_scriptEditor) {
        m_scriptEditor = new ScriptEditor(this);
        m_scriptEditor->setTheme(m_config.value("theme").toString("soft_dark"));
        connect(m_scriptEditor, &QDialog::finished, this, [this]() { m_scriptEditor = nullptr; });
    }
    m_scriptEditor->show();
    m_scriptEditor->raise();
    m_scriptEditor->activateWindow();
}

void MainWindow::exportLog() {
    //переведенный диалог экспорта
    QString path = QFileDialog::getSaveFileName(this, trn("export_log"), "arasaka_log.txt", trn("text_files_filter"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_logEdit->toPlainText().toUtf8());
        log(trn("log_exported") + " " + path);
    } else {
        //переведенная ошибка экспорта
        QMessageBox::critical(this, I18N::instance().tr("error"), trn("export_failed"));
    }
}

void MainWindow::toggleSettings() {
    if (m_settingsPanel && m_settingsPanel->isVisible()) {
        onSettingsClosed();
        return;
    }
    if (m_overlay) {
        m_overlay->setGeometry(0, 0, centralWidget()->width(), centralWidget()->height());
        m_overlay->show();
        m_overlay->raise();
    }
    if (m_settingsPanel) {
        m_settingsPanel->setGeometry(0, 0, 340, centralWidget()->height());
        m_settingsPanel->show();
        m_settingsPanel->raise();
    }
}

void MainWindow::onSettingsClosed() {
    if (m_settingsPanel) m_settingsPanel->hide();
    if (m_overlay) m_overlay->hide();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_overlay && event->type() == QEvent::MouseButtonPress) {
        onSettingsClosed();
        return true;
    }
    if (obj == centralWidget() && event->type() == QEvent::Resize) {
        if (m_overlay) m_overlay->setGeometry(0, 0, centralWidget()->width(), centralWidget()->height());
        if (m_settingsPanel && m_settingsPanel->isVisible()) m_settingsPanel->setGeometry(0, 0, 340, centralWidget()->height());
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_runner && m_runner->isRunning()) m_runner->requestStop();
    if (m_scanner && m_scanner->isRunning()) {
        m_scanner->requestInterruption();
        m_scanner->wait(2000);
    }
    if (m_radarWindow) m_radarWindow->close();
    if (!m_currentWorkdir.isEmpty()) QDir(m_currentWorkdir).removeRecursively();
    event->accept();
}
