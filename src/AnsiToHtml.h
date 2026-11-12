#pragma once
#include <QString>
#include <QMap>

class AnsiToHtml {
public:
    static QString convert(const QString& text, const QString& themeKey = "soft_dark");
private:
    static QString convertLine(const QString& text, const QMap<QString, QString>& colors);
    static QMap<QString, QString> defaultColors();
    static QMap<QString, QString> hackColors();
};