#include "ThemeManager.h"
#include <QApplication>

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

QString ThemeManager::buildTheme(const QString& bg, const QString& card, const QString& text,
                                 const QString& textDim, const QString& accent,
                                 const QString& border, const QString& logBg,
                                 const QString& radius, const QString& btnRadius,
                                 const QString& fontWeight, const QString& cardBorderWidth) {
    return QString(R"(
QMainWindow, QWidget {
    background-color: %1;
    color: %3;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-weight: %9;
}
QLabel { color: %3; }
QLabel#title {
    color: %5;
    font-size: 20px;
    font-weight: bold;
    letter-spacing: 2px;
}
QPushButton {
    background-color: %2;
    color: %3;
    border: %10 solid %6;
    border-radius: %8;
    padding: 6px 14px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: %5;
    color: %1;
}
QPushButton:pressed { background-color: %6; }
QPushButton:disabled {
    background-color: %1;
    color: %4;
    border-color: %4;
}
QPushButton#danger {
    border-color: #ff003c;
    color: #ff003c;
}
QPushButton#danger:hover {
    background-color: #ff003c;
    color: %1;
}
QLineEdit, QComboBox {
    background-color: %2;
    color: %3;
    border: 1px solid %6;
    border-radius: %8;
    padding: 5px;
}
QComboBox QAbstractItemView {
    background-color: %2;
    color: %3;
    selection-background-color: %5;
    selection-color: %1;
}
QTextEdit, QScrollArea {
    background-color: %7;
    color: %3;
    border: 1px solid %6;
    border-radius: %7;
}
QFrame#card {
    background-color: %2;
    border: %10 solid %6;
    border-radius: %7;
    padding: 10px;
}
QFrame#card_ok, QFrame#card[state="ok"] { border-color: %5; }
QFrame#card_warn, QFrame#card[state="warn"] { border-color: #fcee0a; }
QFrame#card_err, QFrame#card[state="err"] { border-color: #ff003c; }
QFrame#card:hover { border-color: %5; }
QGroupBox {
    border: 1px solid %6;
    border-radius: %7;
    margin-top: 10px;
    padding-top: 14px;
    font-weight: bold;
    color: %5;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
    color: %5;
}
QScrollBar:vertical {
    background: %7;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %5;
    border-radius: 5px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover { background: %6; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal {
    background: %7;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %5;
    border-radius: 5px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover { background: %6; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
QStatusBar {
    background-color: %2;
    color: %4;
    border-top: 1px solid %6;
}
QStatusBar QLabel { color: %4; padding: 2px 8px; }
    )")
    .arg(bg, card, text, textDim, accent, border, logBg, btnRadius, fontWeight, cardBorderWidth);
                                 }
                                 
                                 ThemeManager::ThemeManager() {
                                     m_themes["soft_dark"] = {"Soft Dark", buildTheme("#1e1e1e","#2a2a2a","#d4d4d4","#666666","#4a4a4a","#3a3a3a","#151515")};
                                     m_themes["midnight"] = {"Midnight", buildTheme("#0a0a0a","#141414","#b0b0b0","#444444","#2a2a2a","#1f1f1f","#050505")};
                                     m_themes["graphite"] = {"Graphite", buildTheme("#2b2b2b","#363636","#e8e8e8","#777777","#555555","#444444","#202020")};
                                     //в светлой теме основной текст задан как #1a1a1a для читаемости
                                     m_themes["light"] = {"Light", buildTheme("#f5f5f5","#ffffff","#1a1a1a","#999999","#d0d0d0","#cccccc","#eaeaea")};
                                     m_themes["hack"] = {"HACK", buildTheme("#000000","#0a0a0a","#00ff00","#004400","#00cc00","#00ff00","#000000","0px","0px","bold","2px")};
                                     m_themes["arasaka"] = {"ARASAKA CORP", buildTheme("#0d0d0d","#1a1a1a","#e0e0e0","#555555","#cc0000","#333333","#080808","0px","0px","normal","1px")};
                                 }
                                 
                                 QStringList ThemeManager::keys() const { return m_themes.keys(); }
                                 QString ThemeManager::label(const QString& key) const { return m_themes.value(key).first; }
                                 QString ThemeManager::qss(const QString& key) const { return m_themes.value(key).second; }
                                 
                                 void ThemeManager::apply(const QString& key) {
                                     if (!m_themes.contains(key)) return;
                                     qApp->setStyleSheet(m_themes[key].second);
                                 }
