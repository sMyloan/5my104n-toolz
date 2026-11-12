#pragma once
#include <QString>
#include <QJsonObject>
#include <QPair>
#include <QMap>
#include <QStringList>
#include <QByteArray>

class SmyFormat {
public:
    static QPair<QJsonObject, QByteArray> unpack(const QString& path,
                                                 const QString& password = QString());
    static void pack(const QString& scriptPath, const QJsonObject& manifest,
                     const QString& outPath, const QString& password = QString());
    static QString extract(const QString& smyPath, const QString& workdir,
                           const QString& password = QString());
    static QJsonObject createDefaultManifest(const QString& name, const QString& description,
                                             const QString& entry, const QString& category,
                                             const QStringList& apt = {},
                                             const QStringList& pip = {},
                                             const QStringList& binary = {},
                                             const QString& author = "5My104n");
    static QMap<QString, QStringList> checkDependencies(const QJsonObject& manifest);
    static bool installDependencies(const QMap<QString, QStringList>& missing);
};