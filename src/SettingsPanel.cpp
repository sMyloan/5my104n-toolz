#include "SettingsPanel.h"
#include "I18N.h"
#include "ThemeManager.h"

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(340);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
}

void SettingsPanel::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);
    
    auto* headerRow = new QHBoxLayout();
    m_titleLabel = new QLabel(trn("settings_title"));
    m_titleLabel->setStyleSheet("QLabel{font-size:20px;font-weight:bold;font-family:Consolas;color:#00ff80;}");
    headerRow->addWidget(m_titleLabel);
    headerRow->addStretch();
    
    m_closeBtn = new QPushButton(trn("close_btn"));
    m_closeBtn->setFixedSize(36, 36);
    m_closeBtn->setStyleSheet("QPushButton{background-color:#1a0000;color:#ff4444;border:2px solid #ff4444;font-size:18px;font-weight:bold;font-family:Consolas;}QPushButton:hover{background-color:#ff4444;color:#000a00;}");
    connect(m_closeBtn, &QPushButton::clicked, this, &SettingsPanel::closeRequested);
    headerRow->addWidget(m_closeBtn);
    layout->addLayout(headerRow);
    
    m_separatorLine = new QLabel();
    m_separatorLine->setFixedHeight(2);
    m_separatorLine->setStyleSheet("QLabel{background-color:#00ff80;}");
    layout->addWidget(m_separatorLine);
    
    auto* userGroup = new QGroupBox(trn("user_name"));
    auto* userLayout = new QVBoxLayout(userGroup);
    m_userNameEdit = new QLineEdit();
    m_userNameEdit->setPlaceholderText(trn("user_name_placeholder"));
    connect(m_userNameEdit, &QLineEdit::textChanged, this, &SettingsPanel::userNameChanged);
    userLayout->addWidget(m_userNameEdit);
    layout->addWidget(userGroup);
    
    auto* passGroup = new QGroupBox(trn("default_smy_password"));
    auto* passLayout = new QVBoxLayout(passGroup);
    m_passwordCheck = new QCheckBox(trn("enable_password"));
    connect(m_passwordCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_passwordEdit->setEnabled(checked);
        if (!checked) m_passwordEdit->clear();
        emit defaultPasswordToggled(checked);
    });
    passLayout->addWidget(m_passwordCheck);
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(trn("password_placeholder"));
    m_passwordEdit->setEnabled(false);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &SettingsPanel::defaultPasswordChanged);
    passLayout->addWidget(m_passwordEdit);
    layout->addWidget(passGroup);
    
    auto* themeGroup = new QGroupBox(trn("theme_group"));
    auto* themeLayout = new QVBoxLayout(themeGroup);
    m_themeCombo = new QComboBox();
    for (const QString& key : ThemeManager::instance().keys())
        m_themeCombo->addItem(ThemeManager::instance().label(key), key);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto keys = ThemeManager::instance().keys();
        if (idx >= 0 && idx < keys.size())
            emit themeChanged(keys[idx]);
    });
    themeLayout->addWidget(m_themeCombo);
    layout->addWidget(themeGroup);
    
    auto* langGroup = new QGroupBox(trn("language_group"));
    auto* langLayout = new QVBoxLayout(langGroup);
    m_langCombo = new QComboBox();
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Русский", "ru");
    m_langCombo->addItem("Українська", "uk");
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        QStringList langs{"en", "ru", "uk"};
        if (idx >= 0 && idx < langs.size())
            emit languageChanged(langs[idx]);
    });
    langLayout->addWidget(m_langCombo);
    layout->addWidget(langGroup);
    
    auto* logGroup = new QGroupBox(trn("log"));
    auto* logLayout = new QVBoxLayout(logGroup);
    m_timestampsCheck = new QCheckBox(trn("show_timestamps"));
    m_timestampsCheck->setChecked(true);
    connect(m_timestampsCheck, &QCheckBox::toggled, this, &SettingsPanel::timestampsToggled);
    logLayout->addWidget(m_timestampsCheck);
    layout->addWidget(logGroup);
    
    layout->addStretch();
    
    m_exportBtn = new QPushButton(trn("export_log"));
    connect(m_exportBtn, &QPushButton::clicked, this, &SettingsPanel::exportLogRequested);
    layout->addWidget(m_exportBtn);
    
    m_statsBtn = new QPushButton(trn("statistics"));
    connect(m_statsBtn, &QPushButton::clicked, this, &SettingsPanel::showStatsRequested);
    layout->addWidget(m_statsBtn);
    
    applyTheme("soft_dark");
}

void SettingsPanel::applyTheme(const QString& key) {
    m_themeKey = key;
    QString color, bg, textColor, placeholderColor;
    
    if (key == "hack") {
        color = "#00ff00"; bg = "#000a00"; textColor = "#00ff00"; placeholderColor = "#006400";
    } else if (key == "arasaka") {
        color = "#cc0000"; bg = "#1a0000"; textColor = "#e0e0e0"; placeholderColor = "#660000";
    } else if (key == "light") {
        //в светлой теме текст должен быть темным иначе он не читается
        color = "#0064c8"; bg = "#e8e8e8"; textColor = "#1a1a1a"; placeholderColor = "#888888";
    } else {
        color = "#00ff80"; bg = "#001a00"; textColor = "#d4d4d4"; placeholderColor = "#006432";
    }
    
    setStyleSheet(QString("QWidget { background-color: %1; color: %2; }").arg(bg, textColor));
    
    if (m_titleLabel) m_titleLabel->setStyleSheet(QString("QLabel{font-size:20px;font-weight:bold;font-family:Consolas;color:%1;}").arg(color));
    if (m_separatorLine) m_separatorLine->setStyleSheet(QString("QLabel{background-color:%1;}").arg(color));
    
    QString groupStyle = QString(
        "QGroupBox { color: %1; border: 1px solid %1; border-radius: 4px; "
        "margin-top: 10px; padding-top: 15px; font-family: Consolas; font-weight: bold; "
        "background-color: %2; } "
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; "
        "padding: 0 5px; color: %1; font-weight: bold; }"
    ).arg(color, bg);
    
    QString editStyle = QString(
        "QLineEdit { padding: 8px; font-family: Consolas; border: 1px solid %1; "
        "background-color: %2; color: %3; } "
        "QLineEdit:disabled { background-color: #333333; color: #666666; border-color: #555555; }"
    ).arg(color, bg, textColor);
    
    QString comboStyle = QString(
        "QComboBox { padding: 8px; font-family: Consolas; border: 1px solid %1; "
        "background-color: %2; color: %3; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: %2; color: %3; "
        "selection-background-color: %1; selection-color: %2; }"
    ).arg(color, bg, textColor);
    
    QString btnStyle = QString(
        "QPushButton { background-color: %2; color: %3; border: 2px solid %1; "
        "padding: 10px; font-weight: bold; font-family: Consolas; } "
        "QPushButton:hover { background-color: %1; color: %2; }"
    ).arg(color, bg, textColor);
    
    QString checkStyle = QString(
        "QCheckBox { font-family: Consolas; color: %1; spacing: 5px; } "
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %2; "
        "background-color: %3; } "
        "QCheckBox::indicator:checked { background-color: %2; }"
    ).arg(textColor, color, bg);
    
    for (auto* g : findChildren<QGroupBox*>()) g->setStyleSheet(groupStyle);
    
    const QColor placeholder(placeholderColor);
    for (auto* e : findChildren<QLineEdit*>()) {
        e->setStyleSheet(editStyle);
        QPalette pal = e->palette();
        pal.setColor(QPalette::PlaceholderText, placeholder);
        pal.setColor(QPalette::Text, QColor(textColor));
        e->setPalette(pal);
    }
    
    for (auto* c : findChildren<QComboBox*>()) {
        c->setStyleSheet(comboStyle);
        QPalette pal = c->palette();
        pal.setColor(QPalette::Text, QColor(textColor));
        c->setPalette(pal);
    }
    
    for (auto* cb : findChildren<QCheckBox*>()) cb->setStyleSheet(checkStyle);
    
    if (m_exportBtn) m_exportBtn->setStyleSheet(btnStyle);
    if (m_statsBtn) m_statsBtn->setStyleSheet(btnStyle);
    if (m_closeBtn) m_closeBtn->setStyleSheet(
        "QPushButton{background-color:#1a0000;color:#ff4444;border:2px solid #ff4444;"
        "font-size:18px;font-weight:bold;font-family:Consolas;}"
        "QPushButton:hover{background-color:#ff4444;color:#000a00;}");
}

void SettingsPanel::setUserName(const QString& name) { if (m_userNameEdit) m_userNameEdit->setText(name); }
void SettingsPanel::setDefaultPassword(const QString& password) { if (m_passwordEdit) m_passwordEdit->setText(password); }
void SettingsPanel::setDefaultPasswordEnabled(bool enabled) {
    if (m_passwordCheck) m_passwordCheck->setChecked(enabled);
    if (m_passwordEdit) m_passwordEdit->setEnabled(enabled);
}
void SettingsPanel::setTheme(const QString& key) {
    int idx = ThemeManager::instance().keys().indexOf(key);
    if (idx >= 0 && m_themeCombo) m_themeCombo->setCurrentIndex(idx);
    applyTheme(key);
}
void SettingsPanel::setLanguage(const QString& lang) {
    QStringList langs{"en", "ru", "uk"};
    int idx = langs.indexOf(lang);
    if (idx >= 0 && m_langCombo) m_langCombo->setCurrentIndex(idx);
    
    //группы находятся по objectname поэтому старый текст не мешает переводу
    for (auto* g : findChildren<QGroupBox*>()) {
        QString title = g->title();
        if (title == "User Name" || title == "Имя пользователя" || title == "Ім'я користувача") g->setTitle(trn("user_name"));
        else if (title == "Default .smy Password" || title == "Стандартный пароль .smy") g->setTitle(trn("default_smy_password"));
        else if (title == "Theme" || title == "Тема") g->setTitle(trn("theme_group"));
        else if (title == "Language" || title == "Язык" || title == "Мова") g->setTitle(trn("language_group"));
        else if (title == "LOG" || title == "ЛОГ") g->setTitle(trn("log"));
    }
    if (m_titleLabel) m_titleLabel->setText(trn("settings_title"));
    if (m_closeBtn) m_closeBtn->setText(trn("close_btn"));
    if (m_exportBtn) m_exportBtn->setText(trn("export_log"));
    if (m_statsBtn) m_statsBtn->setText(trn("statistics"));
    if (m_passwordCheck) m_passwordCheck->setText(trn("enable_password"));
    if (m_timestampsCheck) m_timestampsCheck->setText(trn("show_timestamps"));
    if (m_userNameEdit) m_userNameEdit->setPlaceholderText(trn("user_name_placeholder"));
    if (m_passwordEdit) m_passwordEdit->setPlaceholderText(trn("password_placeholder"));
}
void SettingsPanel::setTimestamps(bool enabled) { if (m_timestampsCheck) m_timestampsCheck->setChecked(enabled); }
QString SettingsPanel::userName() const { return m_userNameEdit ? m_userNameEdit->text() : QString(); }
QString SettingsPanel::defaultPassword() const { return m_passwordEdit ? m_passwordEdit->text() : QString(); }
bool SettingsPanel::defaultPasswordEnabled() const { return m_passwordCheck ? m_passwordCheck->isChecked() : false; }
