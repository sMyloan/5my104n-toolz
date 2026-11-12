#include "AnsiToHtml.h"
#include <QRegularExpression>
#include <QStringList>

//стандартная палитра ansi цветов для темных тем
QMap<QString, QString> AnsiToHtml::defaultColors() {
    return {
        {"30","#666666"},{"31","#e06c75"},{"32","#98c379"},{"33","#e5c07b"},
        {"34","#61afef"},{"35","#c678dd"},{"36","#56b6c2"},{"37","#d4d4d4"},
        {"90","#666666"},{"91","#e06c75"},{"92","#98c379"},{"93","#e5c07b"},
        {"94","#61afef"},{"95","#c678dd"},{"96","#56b6c2"},{"97","#ffffff"},
    };
}

//для темы hack все цвета заменяются на оттенки зеленого
QMap<QString, QString> AnsiToHtml::hackColors() {
    QMap<QString, QString> c;
    for (const QString& k : {"30","31","32","33","34","35","36","37",
        "91","92","93","94","95","96","97"})
        c[k] = "#00ff00";
        c["90"] = "#004400";
        return c;
}

QString AnsiToHtml::convert(const QString& text, const QString& themeKey) {
    static const QMap<QString, QString> normalColors = defaultColors();
    static const QMap<QString, QString> hackPalette = hackColors();
    const QMap<QString, QString>& colors = (themeKey == "hack") ? hackPalette : normalColors;
    QString t = text;
    //удаление escape последовательностей очистки экрана и курсора
    static const QRegularExpression clearRe(R"(\x1b\[[\?]?[0-9;]*[JKH])");
    t.remove(clearRe);
    //обработка каретки возврата для прогрессбаров
    if (t.contains('\r') && !t.endsWith('\n')) {
        auto parts = t.split('\r');
        t = parts.last();
        if (parts.size() > 1)
            return "__REPLACE_LAST__" + convertLine(t, colors);
    }
    return convertLine(t, colors);
}

QString AnsiToHtml::convertLine(const QString& text, const QMap<QString, QString>& colors) {
    QString result;
    int openSpans = 0;
    //лимит вложенности span чтобы не сломать рендеринг qtextedit
    static constexpr int MAX_SPAN_DEPTH = 10;
    auto closeAll = [&result, &openSpans]() {
        while (openSpans > 0) { result += "</span>"; --openSpans; }
    };
    static const QRegularExpression re(R"(\x1b\[([0-9;]*)m)");
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        QString before = text.mid(last, m.capturedStart(0) - last);
        before = before.toHtmlEscaped();
        result += before;
        QString codesStr = m.captured(1);
        QStringList codes = codesStr.isEmpty() ? QStringList{"0"} : codesStr.split(';');
        for (const QString& c : codes) {
            if (c == "0" || c.isEmpty()) {
                closeAll();
            } else if (c == "1") {
                if (openSpans >= MAX_SPAN_DEPTH) closeAll();
                result += R"(<span style="font-weight:bold">)";
                openSpans++;
            } else if (colors.contains(c)) {
                if (openSpans >= MAX_SPAN_DEPTH) closeAll();
                result += QString(R"(<span style="color:%1">)").arg(colors.value(c));
                openSpans++;
            }
        }
        last = m.capturedEnd(0);
    }
    QString remaining = text.mid(last);
    remaining = remaining.toHtmlEscaped();
    result += remaining;
    closeAll();
    return result;
}
