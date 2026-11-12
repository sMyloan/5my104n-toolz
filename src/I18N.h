#pragma once
#include <QString>
#include <QMap>
#include <QHash>

class I18N {
public:
    static I18N& instance();
    void setLanguage(const QString& lang);
    QString tr(const QString& key) const;
    QString language() const { return m_lang; }

private:
    I18N();
    void buildEn();
    void buildRu();
    void buildUk();
    QString m_lang = "en";
    QHash<QString, QHash<QString, QString>> m_dict;
};

inline QString trn(const QString& key) { return I18N::instance().tr(key); }