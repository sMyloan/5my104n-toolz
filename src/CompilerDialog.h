#pragma once
#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;

class CompilerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompilerDialog(QWidget* parent = nullptr);
private slots:
    void pickScript();
    void doCompile();
private:
    QLineEdit* m_leScript;
    QLineEdit* m_leName;
    QLineEdit* m_leDesc;
    QComboBox* m_cbCat;
    QLineEdit* m_leApt;
    QLineEdit* m_lePip;
    QCheckBox* m_cbEncrypt;
    QLineEdit* m_lePass;
};