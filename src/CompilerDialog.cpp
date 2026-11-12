#include "CompilerDialog.h"
#include "MainWindow.h"
#include "I18N.h"
#include "SmyFormat.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

static const QMap<QString, QString> LANG_RUNTIME = {
    {".py","python3"},{".sh","bash"},{".pl","perl"},{".rb","ruby"},
    {".go","go run"},{".rs","rust-script"},{".c","gcc"},{".cpp","g++"},
    {".js","node"},{".php","php"},
};

static void detectDependencies(const QString& scriptPath, QStringList& outApt, QStringList& outPip) {
    QFile f(scriptPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString content = QString::fromUtf8(f.readAll());
    QSet<QString> apt, pip;
    static const QMap<QString, QString> PY_IMPORTS = {
        {"bleak","bleak"},{"scapy","scapy"},{"requests","requests"},
        {"numpy","numpy"},{"cryptography","cryptography"},{"PySide6","PySide6"},
    };
    static const QMap<QString, QString> SH_CMDS = {
        {"airmon-ng","aircrack-ng"},{"airodump-ng","aircrack-ng"},
        {"aireplay-ng","aircrack-ng"},{"hostapd","hostapd"},{"dnsmasq","dnsmasq"},
        {"iptables","iptables"},{"nmap","nmap"},{"macchanger","macchanger"},
        {"bluetoothctl","bluez"},{"hcitool","bluez"},
    };
    QRegularExpression pyRe(R"(^(?:import|from)\s+(\w+))", QRegularExpression::MultilineOption);
    auto it = pyRe.globalMatch(content);
    while (it.hasNext()) {
        auto m = it.next();
        QString mod = m.captured(1);
        if (PY_IMPORTS.contains(mod)) pip.insert(PY_IMPORTS[mod]);
    }
    for (auto cit = SH_CMDS.constBegin(); cit != SH_CMDS.constEnd(); ++cit) {
        QRegularExpression re(QString(R"(\b%1\b)").arg(QRegularExpression::escape(cit.key())));
        if (re.match(content).hasMatch()) apt.insert(cit.value());
    }
    for (const QString& s : apt) outApt.append(s);
    for (const QString& s : pip) outPip.append(s);
}

CompilerDialog::CompilerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(trn("compiler_title"));
    setMinimumWidth(500);
    auto* layout = new QVBoxLayout(this);

    auto* row1 = new QHBoxLayout();
    m_leScript = new QLineEdit();
    m_leScript->setPlaceholderText(trn("source_script"));
    auto* btnPick = new QPushButton(trn("browse"));
    btnPick->setFixedWidth(40);
    connect(btnPick, &QPushButton::clicked, this, &CompilerDialog::pickScript);
    row1->addWidget(m_leScript);
    row1->addWidget(btnPick);
    layout->addWidget(new QLabel(trn("source_script")));
    layout->addLayout(row1);

    m_leName = new QLineEdit();
    m_leName->setPlaceholderText("MCSP, EVTW, FGR...");
    layout->addWidget(new QLabel(trn("module_name")));
    layout->addWidget(m_leName);
    
    m_leDesc = new QLineEdit();
    m_leDesc->setPlaceholderText(trn("description"));
    layout->addWidget(new QLabel(trn("description")));
    layout->addWidget(m_leDesc);
    
    m_cbCat = new QComboBox();
    m_cbCat->addItems({"wifi","bluetooth","network","stealth","misc"});
    layout->addWidget(new QLabel(trn("category")));
    layout->addWidget(m_cbCat);
    
    m_leApt = new QLineEdit();
    m_leApt->setPlaceholderText("bluez, nmap, hostapd");
    layout->addWidget(new QLabel(trn("apt_deps")));
    layout->addWidget(m_leApt);
    
    m_lePip = new QLineEdit();
    m_lePip->setPlaceholderText("bleak, scapy");
    layout->addWidget(new QLabel(trn("pip_deps")));
    layout->addWidget(m_lePip);
    
    m_cbEncrypt = new QCheckBox(trn("encrypt"));
    layout->addWidget(m_cbEncrypt);
    
    m_lePass = new QLineEdit();
    m_lePass->setPlaceholderText(trn("password"));
    m_lePass->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_lePass);
    
    auto* btnRow = new QHBoxLayout();
    auto* btnOk = new QPushButton(trn("compile"));
    connect(btnOk, &QPushButton::clicked, this, &CompilerDialog::doCompile);
    auto* btnCancel = new QPushButton(trn("cancel"));
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(btnOk);
    btnRow->addWidget(btnCancel);
    layout->addLayout(btnRow);
}

void CompilerDialog::pickScript() {
    //использован перевод для диалога выбора файла
    QString path = QFileDialog::getOpenFileName(this, trn("select_script_dialog"), "", trn("all_files_filter"));
    if (path.isEmpty()) return;
    m_leScript->setText(path);
    if (m_leName->text().isEmpty()) {
        QFileInfo fi(path);
        m_leName->setText(fi.baseName().toUpper().left(4));
    }
    QStringList apt, pip;
    detectDependencies(path, apt, pip);
    if (!apt.isEmpty() && m_leApt->text().isEmpty()) m_leApt->setText(apt.join(", "));
    if (!pip.isEmpty() && m_lePip->text().isEmpty()) m_lePip->setText(pip.join(", "));
}

void CompilerDialog::doCompile() {
    QString script = m_leScript->text().trimmed();
    QString name = m_leName->text().trimmed();
    if (script.isEmpty() || !QFileInfo::exists(script)) {
        QMessageBox::warning(this, trn("error"), trn("specify_script"));
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, trn("error"), trn("specify_name"));
        return;
    }
    //использован перевод для диалога сохранения
    QString outPath = QFileDialog::getSaveFileName(this, trn("save_smy_dialog"), name + ".smy", trn("smy_files_filter"));
    if (outPath.isEmpty()) return;
    
    QStringList apt, pip;
    for (const QString& s : m_leApt->text().split(',')) {
        QString t = s.trimmed();
        if (!t.isEmpty()) apt.append(t);
    }
    for (const QString& s : m_lePip->text().split(',')) {
        QString t = s.trimmed();
        if (!t.isEmpty()) pip.append(t);
    }
    
    QFileInfo fi(script);
    QString ext = fi.suffix().toLower();
    QString runtime = LANG_RUNTIME.value("." + ext, "python3");
    QString author = "5My104n";
    if (auto* mw = qobject_cast<MainWindow*>(parentWidget())) {
        QString userName = mw->userName();
        author = userName.isEmpty() ? "5My104n" : userName;
    }
    
    auto manifest = SmyFormat::createDefaultManifest(
        name,
        m_leDesc->text().trimmed().isEmpty() ? "No description" : m_leDesc->text().trimmed(),
                                                     fi.fileName(), m_cbCat->currentText(), apt, pip, {}, author);
    manifest["runtime"] = runtime;
    
    QString password;
    if (m_cbEncrypt->isChecked()) {
        password = m_lePass->text();
    } else if (auto* mw = qobject_cast<MainWindow*>(parentWidget())) {
        if (mw->defaultPasswordEnabled()) {
            password = mw->defaultPassword();
        }
    }
    
    try {
        SmyFormat::pack(script, manifest, outPath, password);
        QMessageBox::information(this, trn("success"), trn("compiled") + "\n" + outPath);
        accept();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, trn("error"), trn("compile_failed") + "\n" + e.what());
    }
}
