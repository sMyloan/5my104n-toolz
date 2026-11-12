#pragma once
#include <QFrame>
#include <QJsonObject>

class MainWindow;
class QLabel;

class ToolCard : public QFrame {
    Q_OBJECT
public:
    ToolCard(const QString& smyPath, const QJsonObject& manifest, MainWindow* parent);
public slots:
    void setDependencyStatus(int totalMissing);
private slots:
    void onRun();
    void onCheck();
    void onExtract();
    void onTerminal();
private:
    QString m_smyPath;
    QJsonObject m_manifest;
    MainWindow* m_parentWin;
    QLabel* m_metaLabel = nullptr;
    QString m_metaPrefix;
};
