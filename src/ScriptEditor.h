#pragma once
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QThread>
#include <QProgressDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QTemporaryFile>
#include "I18N.h"
#include "SmyFormat.h"

class LineNumberArea;

class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    enum Language {
        Python, Bash, Ruby, Go, Cpp, CSharp, JavaScript,
        Perl, PHP, Java, Rust, Lua, PowerShell, SQL,
        HTML, CSS, None
    };
    explicit CodeHighlighter(QTextDocument* parent = nullptr);
    void setLanguage(Language lang);
    void setTheme(const QString& key);
    static Language fromExtension(const QString& ext);
    static Language fromRuntime(const QString& runtime);
protected:
    void highlightBlock(const QString& text) override;
private:
    void rebuildRules();
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QVector<Rule> rules;
    Language m_lang = None;
    QString m_themeKey = "soft_dark";
};

class SmyUnpackWorker : public QObject {
    Q_OBJECT
public:
    explicit SmyUnpackWorker(const QString& path, const QString& password, QObject* parent = nullptr);
    void run();
signals:
    void finished(bool success, const QJsonObject& manifest, const QByteArray& scriptBytes, const QString& error);
private:
    QString m_path;
    QString m_password;
};

class SmyPackWorker : public QObject {
    Q_OBJECT
public:
    explicit SmyPackWorker(const QString& scriptPath, const QJsonObject& manifest,
                          const QString& outPath, const QString& password, QObject* parent = nullptr);
    void run();
signals:
    void finished(bool success, const QString& error);
private:
    QString m_scriptPath;
    QJsonObject m_manifest;
    QString m_outPath;
    QString m_password;
};

class ScriptEditor : public QDialog {
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget* parent = nullptr);
    void openFile(const QString& path);
    void setTheme(const QString& key);
private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onClose();
    void onSmyUnpackFinished(bool success, const QJsonObject& manifest, const QByteArray& scriptBytes, const QString& error);
    void onSmyPackFinished(bool success, const QString& error);
private:
    void buildUi();
    void updateTitle();
    void applyTheme();
    void updateGutterWidth();
    void updateCurrentLineHighlight();
    void openSmyFile(const QString& path);
    void saveAsSmyFile(const QString& outPath);
    void detectLanguage(const QString& path, const QJsonObject& manifest = {});

    QTextEdit* m_editor;
    QLabel* m_statusLabel;
    QPushButton* m_saveBtn;
    QPushButton* m_openBtn;
    QPushButton* m_newBtn;
    QString m_currentPath;
    bool m_modified = false;
    QString m_themeKey = "soft_dark";
    CodeHighlighter* m_highlighter;

    QJsonObject m_currentManifest;
    QString m_currentPassword;
    bool m_isSmyFile = false;
    QThread* m_workerThread = nullptr;
    QProgressDialog* m_progress = nullptr;
    LineNumberArea* m_gutter = nullptr;
    QColor m_currentLineColor;
};