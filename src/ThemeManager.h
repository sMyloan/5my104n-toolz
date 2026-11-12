#pragma once
#include <QString>
#include <QMap>
#include <QPair>

class ThemeManager {
public:
    static ThemeManager& instance();
    QStringList keys() const;
    QString label(const QString& key) const;
    QString qss(const QString& key) const;
    void apply(const QString& key);
private:
    ThemeManager();
    static QString buildTheme(const QString& bg, const QString& card, const QString& text,
                              const QString& textDim, const QString& accent,
                              const QString& border, const QString& logBg,
                              const QString& radius = "4px",
                              const QString& btnRadius = "4px",
                              const QString& fontWeight = "bold",
                              const QString& cardBorderWidth = "1px");
    QMap<QString, QPair<QString, QString>> m_themes;
};