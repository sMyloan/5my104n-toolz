#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void setUserName(const QString& name);
    void setDefaultPassword(const QString& password);
    void setDefaultPasswordEnabled(bool enabled);
    void setTheme(const QString& key);
    void setLanguage(const QString& lang);
    void setTimestamps(bool enabled);
    QString userName() const;
    QString defaultPassword() const;
    bool defaultPasswordEnabled() const;
signals:
    void userNameChanged(const QString& name);
    void defaultPasswordChanged(const QString& password);
    void defaultPasswordToggled(bool enabled);
    void themeChanged(const QString& key);
    void languageChanged(const QString& lang);
    void timestampsToggled(bool enabled);
    void exportLogRequested();
    void showStatsRequested();
    void closeRequested();
private:
    void buildUi();
    void applyTheme(const QString& key);
    QLineEdit* m_userNameEdit = nullptr;
    QCheckBox* m_passwordCheck = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QComboBox* m_langCombo = nullptr;
    QCheckBox* m_timestampsCheck = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_statsBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_separatorLine = nullptr;
    QString m_themeKey = "soft_dark";
};