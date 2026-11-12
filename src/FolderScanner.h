#pragma once
#include <QThread>
#include <QStringList>

class FolderScanner : public QThread {
    Q_OBJECT
public:
    explicit FolderScanner(const QString& folder, QObject* parent = nullptr);
protected:
    void run() override;
signals:
    void finishedScan(const QStringList& paths);
    void errorOccurred(const QString& msg);
private:
    QString m_folder;
};