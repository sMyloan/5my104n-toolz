#include "ScriptEditor.h"
#include <QKeySequence>
#include <QTextCursor>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QScrollBar>
#include <QKeyEvent>
#include <QHash>

//подсветка синтаксиса для разных языков
CodeHighlighter::CodeHighlighter(QTextDocument* parent)
: QSyntaxHighlighter(parent) { rebuildRules(); }

void CodeHighlighter::setLanguage(Language lang) {
    m_lang = lang; rebuildRules(); rehighlight();
}

void CodeHighlighter::setTheme(const QString& key) {
    m_themeKey = key; rebuildRules(); rehighlight();
}

//определение языка по расширению файла
CodeHighlighter::Language CodeHighlighter::fromExtension(const QString& ext) {
    if (ext == ".py") return Python;
    if (ext == ".sh" || ext == ".bash") return Bash;
    if (ext == ".rb") return Ruby;
    if (ext == ".go") return Go;
    if (ext == ".c" || ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".cc") return Cpp;
    if (ext == ".cs") return CSharp;
    if (ext == ".js" || ext == ".ts") return JavaScript;
    if (ext == ".pl" || ext == ".pm") return Perl;
    if (ext == ".php") return PHP;
    if (ext == ".java") return Java;
    if (ext == ".rs") return Rust;
    if (ext == ".lua") return Lua;
    if (ext == ".ps1") return PowerShell;
    if (ext == ".sql") return SQL;
    if (ext == ".html" || ext == ".htm") return HTML;
    if (ext == ".css") return CSS;
    return None;
}

//определение языка по runtime из манифеста
CodeHighlighter::Language CodeHighlighter::fromRuntime(const QString& runtime) {
    QString r = runtime.toLower();
    if (r.contains("python")) return Python;
    if (r.contains("bash") || r.contains("sh")) return Bash;
    if (r.contains("ruby")) return Ruby;
    if (r.contains("go")) return Go;
    if (r.contains("g++") || r.contains("gcc") || r.contains("c++")) return Cpp;
    if (r.contains("csc") || r.contains("dotnet")) return CSharp;
    if (r.contains("node")) return JavaScript;
    if (r.contains("perl")) return Perl;
    if (r.contains("php")) return PHP;
    if (r.contains("java")) return Java;
    if (r.contains("rust") || r.contains("cargo")) return Rust;
    if (r.contains("lua")) return Lua;
    if (r.contains("powershell")) return PowerShell;
    return None;
}

//построение правил подсветки для текущего языка и темы
void CodeHighlighter::rebuildRules() {
    rules.clear();
    QColor kwColor, biColor, strColor, comColor, numColor, decColor, varColor;
    
    //цвета зависят от выбранной темы
    if (m_themeKey == "light") {
        kwColor = QColor("#0000ff"); biColor = QColor("#0064c8");
        strColor = QColor("#a31515"); comColor = QColor("#008000");
        numColor = QColor("#098658"); decColor = QColor("#795e26");
        varColor = QColor("#001080");
    } else if (m_themeKey == "hack") {
        kwColor = QColor("#00ff00"); biColor = QColor("#00cc00");
        strColor = QColor("#90ee90"); comColor = QColor("#006400");
        numColor = QColor("#32cd32"); decColor = QColor("#00ff80");
        varColor = QColor("#adff2f");
    } else if (m_themeKey == "arasaka") {
        kwColor = QColor("#ff4444"); biColor = QColor("#cc0000");
        strColor = QColor("#ff8888"); comColor = QColor("#880000");
        numColor = QColor("#ff6666"); decColor = QColor("#cc0000");
        varColor = QColor("#ffaaaa");
    } else {
        kwColor = QColor("#ff79c6"); biColor = QColor("#8be9fd");
        strColor = QColor("#f1fa8c"); comColor = QColor("#6272a4");
        numColor = QColor("#bd93f9"); decColor = QColor("#50fa7b");
        varColor = QColor("#f8f8f2");
    }
    
    QTextCharFormat kwFmt, biFmt, strFmt, comFmt, numFmt, decFmt, varFmt;
    kwFmt.setForeground(kwColor); kwFmt.setFontWeight(QFont::Bold);
    biFmt.setForeground(biColor);
    strFmt.setForeground(strColor);
    comFmt.setForeground(comColor); comFmt.setFontItalic(true);
    numFmt.setForeground(numColor);
    decFmt.setForeground(decColor);
    varFmt.setForeground(varColor);
    
    //правила для python
    if (m_lang == Python) {
        for (const QString& kw : {"def","class","if","else","elif","for","while",
            "return","import","from","as","try","except","finally","with",
            "lambda","yield","pass","break","continue","and","or","not",
            "in","is","True","False","None","async","await","raise","del",
            "global","nonlocal","assert"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"print","len","range","int","str","list","dict",
                "set","tuple","type","isinstance","super","property","open",
                "input","enumerate","zip","map","filter","sorted","reversed",
                "any","all","min","max","sum","abs","round"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"\"\"[\\s\\S]*?\"\"\""), strFmt});
                rules.append({QRegularExpression("'''[\\s\\S]*?'''"), strFmt});
                rules.append({QRegularExpression("f\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("f'[^']*'"), strFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
                rules.append({QRegularExpression("@\\w+"), decFmt});
    } else if (m_lang == Bash) {
        for (const QString& kw : {"if","then","else","elif","fi","for","while",
            "do","done","case","esac","function","return","exit","break",
            "continue","in","select","until","shift","export","local",
            "readonly","declare","typeset","trap","eval","exec","source"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"echo","printf","read","cd","pwd","ls","cp","mv",
                "rm","mkdir","chmod","chown","grep","sed","awk","find","cat",
                "head","tail","wc","sort","uniq","cut","tr","tee","xargs",
                "sudo","apt","pip","python3","bash","sh","curl","wget"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression("\\$\\{[^}]+\\}"), varFmt});
                rules.append({QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("\\b\\d+\\b"), numFmt});
    } else if (m_lang == Ruby) {
        for (const QString& kw : {"def","end","class","module","if","else","elsif",
            "unless","while","until","for","do","begin","rescue","ensure",
            "raise","return","yield","break","next","redo","retry","in",
            "and","or","not","nil","true","false","self","super","require",
            "include","extend","attr_accessor","attr_reader","attr_writer"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"puts","print","p","gets","chomp","to_s","to_i",
                "to_f","length","size","each","map","select","reduce","inject"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression(":[A-Za-z_][A-Za-z0-9_]*"), decFmt});
                rules.append({QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == Go) {
        for (const QString& kw : {"func","package","import","return","if","else",
            "for","range","switch","case","default","select","go","defer",
            "chan","map","struct","interface","type","var","const","break",
            "continue","fallthrough","goto"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"fmt","Println","Printf","Print","Errorf",
                "make","new","len","cap","append","copy","close","delete",
                "complex","real","imag","panic","recover"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("`[^`]*`"), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == Cpp) {
        for (const QString& kw : {"int","char","float","double","void","bool",
            "class","struct","enum","union","namespace","template","typename",
            "public","private","protected","virtual","override","const",
            "static","extern","inline","friend","typedef","using","auto",
            "if","else","for","while","do","switch","case","default","break",
            "continue","return","goto","try","catch","throw","new","delete",
            "sizeof","alignof","decltype","constexpr","noexcept","static_cast",
            "dynamic_cast","const_cast","reinterpret_cast"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"cout","cin","endl","printf","scanf","malloc",
                "free","calloc","realloc","memcpy","memset","strlen","strcpy",
                "strcmp","atoi","atof","exit","abort"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("#\\s*\\w+"), decFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*[fFlLuU]*\\b"), numFmt});
    } else if (m_lang == CSharp) {
        for (const QString& kw : {"class","struct","interface","enum","namespace",
            "using","public","private","protected","internal","static",
            "virtual","override","abstract","sealed","readonly","const",
            "if","else","for","foreach","while","do","switch","case","default",
            "break","continue","return","goto","try","catch","finally","throw",
            "new","delete","typeof","sizeof","checked","unchecked","async",
            "await","yield","var","dynamic","void","int","string","bool",
            "double","float","decimal","char","byte","short","long","object"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"Console","WriteLine","ReadLine","Write",
                "Convert","ToString","Parse","TryParse","Math","String",
                "Array","List","Dictionary","IEnumerable","IEnumerator"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*[fFdDmM]?\\b"), numFmt});
    } else if (m_lang == JavaScript) {
        for (const QString& kw : {"function","var","let","const","if","else",
            "for","while","do","switch","case","default","break","continue",
            "return","throw","try","catch","finally","new","delete","typeof",
            "instanceof","void","class","extends","super","import","export",
            "async","await","yield","this","null","undefined","true","false"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"console","log","error","warn","document",
                "window","alert","prompt","confirm","setTimeout","setInterval",
                "clearTimeout","clearInterval","fetch","Promise","JSON",
                "Math","Date","Array","Object","String","Number","Boolean"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("`[^`]*`"), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == Perl) {
        for (const QString& kw : {"sub","if","else","elsif","unless","while",
            "until","for","foreach","do","last","next","redo","return",
            "die","warn","eval","require","use","package","my","local",
            "our","state","BEGIN","END","CHECK","INIT"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"print","printf","say","chomp","chop",
                "length","substr","index","rindex","split","join","sort",
                "reverse","map","grep","keys","values","exists","delete"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("%[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == PHP) {
        for (const QString& kw : {"function","class","interface","trait","namespace",
            "use","public","private","protected","static","abstract","final",
            "if","else","elseif","for","foreach","while","do","switch","case",
            "default","break","continue","return","throw","try","catch",
            "finally","new","clone","instanceof","echo","print","require",
            "include","require_once","include_once","var","const","global"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"array","count","strlen","strpos","substr",
                "str_replace","explode","implode","json_encode","json_decode",
                "file_get_contents","file_put_contents","print_r","var_dump"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == Java) {
        for (const QString& kw : {"class","interface","enum","package","import",
            "public","private","protected","static","final","abstract",
            "synchronized","volatile","transient","native","strictfp",
            "if","else","for","while","do","switch","case","default",
            "break","continue","return","throw","throws","try","catch",
            "finally","new","instanceof","void","int","long","short","byte",
            "float","double","boolean","char","String","this","super",
            "extends","implements","true","false","null"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"System","out","err","in","println","print",
                "Math","abs","sqrt","pow","random","Integer","parseInt",
                "Double","parseDouble","String","valueOf","length"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("@\\w+"), decFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*[fFdDlL]?\\b"), numFmt});
    } else if (m_lang == Rust) {
        for (const QString& kw : {"fn","let","mut","const","static","struct",
            "enum","impl","trait","type","use","mod","pub","crate","self",
            "super","if","else","for","while","loop","match","break",
            "continue","return","yield","async","await","move","ref",
            "unsafe","extern","where","as","in","true","false"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"println","print","eprintln","format",
                "vec","String","Some","None","Ok","Err","Box","Rc","Arc",
                "HashMap","Vec","Option","Result"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("//[^\n]*"), comFmt});
                rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
                rules.append({QRegularExpression("#\\[\\w+\\]"), decFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*[fF]?\\b"), numFmt});
    } else if (m_lang == Lua) {
        for (const QString& kw : {"function","end","if","then","else","elseif",
            "for","while","do","repeat","until","return","break","local",
            "and","or","not","in","nil","true","false","goto"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"print","type","tostring","tonumber","pairs",
                "ipairs","next","select","unpack","require","error","pcall",
                "xpcall","assert","getmetatable","setmetatable","rawget","rawset"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("--[^\n]*"), comFmt});
                rules.append({QRegularExpression("--\\[\\[[\\s\\S]*?\\]\\]"), comFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == PowerShell) {
        for (const QString& kw : {"function","param","begin","process","end",
            "if","else","elseif","for","foreach","while","do","until",
            "switch","break","continue","return","exit","throw","try",
            "catch","finally","trap","filter","in","class","enum","using",
            "var","dynamicparam","true","false","null"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            for (const QString& b : {"Write-Host","Write-Output","Write-Error",
                "Write-Warning","Write-Verbose","Get-ChildItem","Get-Content",
                "Set-Content","Add-Content","Remove-Item","New-Item",
                "Select-Object","Where-Object","ForEach-Object","Sort-Object"})
                rules.append({QRegularExpression("\\b" + b + "\\b"), biFmt});
                rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
                rules.append({QRegularExpression("'[^']*'"), strFmt});
                rules.append({QRegularExpression("#[^\n]*"), comFmt});
                rules.append({QRegularExpression("<#[\\s\\S]*?#>"), comFmt});
                rules.append({QRegularExpression("\\$[A-Za-z_][A-Za-z0-9_]*"), varFmt});
                rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == SQL) {
        for (const QString& kw : {"SELECT","FROM","WHERE","INSERT","INTO","VALUES",
            "UPDATE","SET","DELETE","CREATE","TABLE","ALTER","DROP",
            "INDEX","VIEW","TRIGGER","PROCEDURE","FUNCTION","DATABASE",
            "JOIN","INNER","LEFT","RIGHT","OUTER","FULL","CROSS",
            "ON","AND","OR","NOT","IN","EXISTS","BETWEEN","LIKE",
            "IS","NULL","AS","DISTINCT","ORDER","BY","GROUP","HAVING",
            "LIMIT","OFFSET","UNION","ALL","CASE","WHEN","THEN","ELSE","END",
            "PRIMARY","KEY","FOREIGN","REFERENCES","CONSTRAINT","DEFAULT",
            "CHECK","UNIQUE","NOT","NULL","AUTO_INCREMENT","IF"})
            rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});
            rules.append({QRegularExpression("'[^']*'"), strFmt});
            rules.append({QRegularExpression("--[^\n]*"), comFmt});
            rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
            rules.append({QRegularExpression("\\b\\d+\\.?\\d*\\b"), numFmt});
    } else if (m_lang == HTML) {
        rules.append({QRegularExpression("<[^>]+>"), kwFmt});
        rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
        rules.append({QRegularExpression("'[^']*'"), strFmt});
        rules.append({QRegularExpression("<!--[^>]*-->"), comFmt});
    } else if (m_lang == CSS) {
        rules.append({QRegularExpression("[.#]?[A-Za-z_][A-Za-z0-9_-]*\\s*\\{"), kwFmt});
        rules.append({QRegularExpression("[A-Za-z-]+\\s*:"), biFmt});
        rules.append({QRegularExpression("\"[^\"]*\""), strFmt});
        rules.append({QRegularExpression("'[^']*'"), strFmt});
        rules.append({QRegularExpression("/\\*[\\s\\S]*?\\*/"), comFmt});
        rules.append({QRegularExpression("\\b\\d+\\.?\\d*(px|em|rem|%|vh|vw|pt|cm|mm|in)?\\b"), numFmt});
    }
}

void CodeHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

//воркер для распаковки smy в отдельном потоке
SmyUnpackWorker::SmyUnpackWorker(const QString& path, const QString& password, QObject* parent)
: QObject(parent), m_path(path), m_password(password) {}

void SmyUnpackWorker::run() {
    try {
        auto [manifest, scriptBytes] = SmyFormat::unpack(m_path, m_password);
        emit finished(true, manifest, scriptBytes, QString());
    } catch (const std::exception& e) {
        emit finished(false, QJsonObject(), QByteArray(), QString::fromUtf8(e.what()));
    }
}

//воркер для упаковки smy в отдельном потоке
SmyPackWorker::SmyPackWorker(const QString& scriptPath, const QJsonObject& manifest,
                             const QString& outPath, const QString& password, QObject* parent)
: QObject(parent), m_scriptPath(scriptPath), m_manifest(manifest),
m_outPath(outPath), m_password(password) {}

void SmyPackWorker::run() {
    try {
        SmyFormat::pack(m_scriptPath, m_manifest, m_outPath, m_password);
        emit finished(true, QString());
    } catch (const std::exception& e) {
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

//область с номерами строк слева от редактора
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(QTextEdit* editor) : QWidget(editor), m_editor(editor) {
        setFont(editor->font());
        setFixedWidth(48);
    }
    
    void setGutterColors(const QColor& fg, const QColor& bg) {
        m_fg = fg; m_bg = bg; update();
    }
    
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), m_bg);
        p.setPen(m_fg);
        QTextDocument* doc = m_editor->document();
        if (!doc) return;
        const int scroll = m_editor->verticalScrollBar()->value();
        const int lineHeight = m_editor->fontMetrics().height();
        const int frame = m_editor->frameWidth();
        int number = 1;
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++number) {
            const QRectF r = doc->documentLayout()->blockBoundingRect(block);
            const int top = qRound(r.top()) - scroll + frame;
            if (top > height()) break;
            if (top + lineHeight < 0) continue;
            p.drawText(QRect(0, top, width() - 6, qMax(lineHeight, qRound(r.height()))),
                       Qt::AlignRight | Qt::AlignVCenter, QString::number(number));
        }
    }
    
private:
    QTextEdit* m_editor;
    QColor m_fg = QColor(120, 120, 120);
    QColor m_bg = QColor(20, 20, 20);
};

//реализация редактора с автозакрытием скобок
class CodeEditorImpl : public QTextEdit {
public:
    explicit CodeEditorImpl(QWidget* parent = nullptr) : QTextEdit(parent) {}
    
protected:
    void keyPressEvent(QKeyEvent* e) override {
        const QString typed = e->text();
        if (typed.size() == 1) {
            const QChar open = typed.at(0);
            static const QHash<QChar, QChar> pairs = {
                {'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}
            };
            auto pit = pairs.constFind(open);
            if (pit != pairs.constEnd()) {
                const QChar close = pit.value();
                QTextCursor cur = textCursor();
                if (cur.hasSelection()) {
                    const QString selected = cur.selectedText();
                    cur.insertText(QString(open) + selected + close);
                    setTextCursor(cur);
                    e->accept();
                    return;
                }
                if (open == close && !cur.atBlockEnd()
                    && cur.block().text().at(cur.positionInBlock()) == close) {
                    cur.movePosition(QTextCursor::Right);
                setTextCursor(cur);
                e->accept();
                return;
                    }
                    cur.insertText(QString(open) + close);
                    cur.movePosition(QTextCursor::Left);
                    setTextCursor(cur);
                    e->accept();
                    return;
            }
            static const QString closers = ")]}\"'";
            if (closers.contains(open)) {
                QTextCursor cur = textCursor();
                if (!cur.hasSelection() && !cur.atBlockEnd()
                    && cur.block().text().at(cur.positionInBlock()) == open) {
                    cur.movePosition(QTextCursor::Right);
                setTextCursor(cur);
                e->accept();
                return;
                    }
            }
        }
        QTextEdit::keyPressEvent(e);
    }
};

ScriptEditor::ScriptEditor(QWidget* parent) : QDialog(parent) {
    setWindowTitle(I18N::instance().tr("script_editor"));
    setMinimumSize(900, 650);
    resize(1000, 700);
    setWindowFlags(Qt::Window);
    buildUi();
}

void ScriptEditor::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    auto* menuBar = new QMenuBar(this);
    auto* fileMenu = menuBar->addMenu(I18N::instance().tr("file"));
    auto* actNew = fileMenu->addAction(I18N::instance().tr("new_file"));
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &ScriptEditor::onNew);
    auto* actOpen = fileMenu->addAction(I18N::instance().tr("open_file"));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, &ScriptEditor::onOpen);
    auto* actSave = fileMenu->addAction(I18N::instance().tr("save_file"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &ScriptEditor::onSave);
    auto* actSaveAs = fileMenu->addAction(I18N::instance().tr("save_as"));
    actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(actSaveAs, &QAction::triggered, this, &ScriptEditor::onSaveAs);
    fileMenu->addSeparator();
    auto* actClose = fileMenu->addAction(I18N::instance().tr("close_editor"));
    actClose->setShortcut(QKeySequence::Close);
    connect(actClose, &QAction::triggered, this, &ScriptEditor::onClose);
    layout->addWidget(menuBar);
    
    m_editor = new CodeEditorImpl();
    m_editor->setFont(QFont("Consolas", 11));
    m_editor->setLineWrapMode(QTextEdit::NoWrap);
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(' '));
    m_highlighter = new CodeHighlighter(m_editor->document());
    
    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, [this](bool mod) { m_modified = mod; updateTitle(); });
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        updateGutterWidth();
        if (m_gutter) m_gutter->update();
    });
        connect(m_editor, &QTextEdit::cursorPositionChanged,
                this, &ScriptEditor::updateCurrentLineHighlight);
        
        m_gutter = new LineNumberArea(m_editor);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                m_gutter, qOverload<>(&QWidget::update));
        
        auto* editorRow = new QHBoxLayout();
        editorRow->setSpacing(0);
        editorRow->setContentsMargins(0, 0, 0, 0);
        editorRow->addWidget(m_gutter);
        editorRow->addWidget(m_editor, 1);
        layout->addLayout(editorRow, 1);
        
        m_statusLabel = new QLabel(I18N::instance().tr("ready"));
        m_statusLabel->setStyleSheet("QLabel{padding:5px;font-family:Consolas;font-size:11px;}");
        layout->addWidget(m_statusLabel);
        
        auto* btnRow = new QHBoxLayout();
        m_newBtn = new QPushButton(I18N::instance().tr("new_file"));
        m_openBtn = new QPushButton(I18N::instance().tr("open_file"));
        m_saveBtn = new QPushButton(I18N::instance().tr("save_file"));
        m_saveBtn->setEnabled(false);
        connect(m_newBtn, &QPushButton::clicked, this, &ScriptEditor::onNew);
        connect(m_openBtn, &QPushButton::clicked, this, &ScriptEditor::onOpen);
        connect(m_saveBtn, &QPushButton::clicked, this, &ScriptEditor::onSave);
        btnRow->addWidget(m_newBtn);
        btnRow->addWidget(m_openBtn);
        btnRow->addWidget(m_saveBtn);
        btnRow->addStretch();
        layout->addLayout(btnRow);
        
        applyTheme();
}

void ScriptEditor::applyTheme() {
    QString color, bg, border;
    if (m_themeKey == "hack")      { color = "#00ff00"; bg = "#000a00"; border = "#00ff00"; }
    else if (m_themeKey == "arasaka") { color = "#cc0000"; bg = "#1a0000"; border = "#cc0000"; }
    else if (m_themeKey == "light")   { color = "#0064c8"; bg = "#e8e8e8"; border = "#0064c8"; }
    else                              { color = "#00ff80"; bg = "#001a00"; border = "#00ff80"; }
    
    QString btnStyle = QString(
        "QPushButton{background-color:%2;color:%1;border:2px solid %1;padding:8px;"
        "font-weight:bold;font-family:Consolas;}"
        "QPushButton:hover{background-color:%1;color:%2;}").arg(color, bg);
        
        m_newBtn->setStyleSheet(btnStyle);
        m_openBtn->setStyleSheet(btnStyle);
        m_saveBtn->setStyleSheet(btnStyle);
        m_statusLabel->setStyleSheet(
            QString("QLabel{color:%1;padding:5px;font-family:Consolas;font-size:11px;}").arg(color));
        
        //в светлой теме редактор белый с черным текстом
        if (m_themeKey == "light") {
            m_editor->setStyleSheet("QTextEdit{background-color:#ffffff;color:#000000;}");
        } else {
            m_editor->setStyleSheet(
                QString("QTextEdit{background-color:%1;color:#f8f8f2;}").arg(bg));
        }
        
        QColor currentLine(color);
        currentLine.setAlpha(28);
        m_currentLineColor = currentLine;
        
        if (m_gutter) {
            QColor gutterFg(color);
            gutterFg.setAlpha(150);
            const QColor gutterBg = (m_themeKey == "light") ? QColor("#eaeaea") : QColor(bg).darker(115);
            m_gutter->setGutterColors(gutterFg, gutterBg);
        }
        
        updateCurrentLineHighlight();
        updateGutterWidth();
        if (m_highlighter) m_highlighter->setTheme(m_themeKey);
}

void ScriptEditor::updateGutterWidth() {
    if (!m_gutter || !m_editor) return;
    const int digits = QString::number(qMax(1, m_editor->document()->blockCount())).size();
    const int width = m_editor->fontMetrics().horizontalAdvance('9') * qMax(2, digits) + 12;
    m_gutter->setFixedWidth(width);
}

void ScriptEditor::updateCurrentLineHighlight() {
    if (!m_editor || !m_currentLineColor.isValid()) return;
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(m_currentLineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = m_editor->textCursor();
    selection.cursor.clearSelection();
    m_editor->setExtraSelections({selection});
}

void ScriptEditor::setTheme(const QString& key) {
    m_themeKey = key;
    applyTheme();
}

void ScriptEditor::updateTitle() {
    QString name = m_currentPath.isEmpty() ? I18N::instance().tr("untitled") : QFileInfo(m_currentPath).fileName();
    QString mod = m_modified ? " *" : "";
    QString smyTag = m_isSmyFile ? " [SMY]" : "";
    setWindowTitle(QString("%1 - %2%3%4").arg(I18N::instance().tr("script_editor"), name, smyTag, mod));
}

//определение языка подсветки по файлу или манифесту
void ScriptEditor::detectLanguage(const QString& path, const QJsonObject& manifest) {
    QString ext = QFileInfo(path).suffix().toLower();
    CodeHighlighter::Language lang = CodeHighlighter::fromExtension(ext);
    if (m_isSmyFile && !manifest.isEmpty()) {
        QString runtime = manifest.value("runtime").toString();
        QString entry = manifest.value("entry").toString();
        if (!runtime.isEmpty()) lang = CodeHighlighter::fromRuntime(runtime);
        else if (!entry.isEmpty()) lang = CodeHighlighter::fromExtension(QFileInfo(entry).suffix().toLower());
    }
    m_highlighter->setLanguage(lang);
}

void ScriptEditor::openFile(const QString& path) {
    if (path.endsWith(".smy")) { openSmyFile(path); return; }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        //переведенная ошибка открытия файла
        QMessageBox::critical(this, I18N::instance().tr("error"),
                              I18N::instance().tr("cannot_open") + "\n" + path);
        return;
    }
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    f.close();
    m_currentPath = path;
    m_isSmyFile = false;
    m_currentManifest = QJsonObject();
    m_currentPassword.clear();
    m_modified = false;
    m_editor->document()->setModified(false);
    m_saveBtn->setEnabled(true);
    detectLanguage(path);
    updateTitle();
    m_statusLabel->setText(I18N::instance().tr("opened") + " " + path);
}

void ScriptEditor::openSmyFile(const QString& path) {
    m_progress = new QProgressDialog(I18N::instance().tr("unpacking_smy"), QString(), 0, 0, this);
    m_progress->setWindowModality(Qt::WindowModal);
    m_progress->show();
    QString password;
    bool encrypted = false;
    try {
        auto [manifest, _] = SmyFormat::unpack(path, QString());
        encrypted = false;
    } catch (...) {
        encrypted = true;
        bool ok = false;
        //переведенный диалог ввода пароля
        password = QInputDialog::getText(this, I18N::instance().tr("password_required"),
                                         I18N::instance().tr("enter_password"), QLineEdit::Password, "", &ok);
        if (!ok) { m_progress->close(); return; }
    }
    m_workerThread = new QThread(this);
    auto* worker = new SmyUnpackWorker(path, password);
    worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, worker, &SmyUnpackWorker::run);
    connect(worker, &SmyUnpackWorker::finished, this, &ScriptEditor::onSmyUnpackFinished);
    connect(worker, &SmyUnpackWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &SmyUnpackWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    m_workerThread->start();
    m_currentPassword = password;
}

void ScriptEditor::onSmyUnpackFinished(bool success, const QJsonObject& manifest, const QByteArray& scriptBytes, const QString& error) {
    if (m_progress) m_progress->close();
    if (!success) {
        //переведенная ошибка распаковки
        QMessageBox::critical(this, I18N::instance().tr("error"),
                              I18N::instance().tr("unpack_failed") + "\n" + error);
        return;
    }
    m_editor->setPlainText(QString::fromUtf8(scriptBytes));
    m_currentManifest = manifest;
    m_isSmyFile = true;
    m_modified = false;
    m_editor->document()->setModified(false);
    m_saveBtn->setEnabled(true);
    detectLanguage(m_currentPath, manifest);
    updateTitle();
    m_statusLabel->setText(I18N::instance().tr("opened") + " " + m_currentPath + " [SMY]");
}

void ScriptEditor::onNew() {
    if (m_modified) {
        auto r = QMessageBox::question(this, I18N::instance().tr("save_file"),
                                       I18N::instance().tr("unsaved_changes"),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (r == QMessageBox::Save) onSave();
        else if (r == QMessageBox::Cancel) return;
    }
    m_editor->clear();
    m_currentPath.clear();
    m_isSmyFile = false;
    m_currentManifest = QJsonObject();
    m_currentPassword.clear();
    m_modified = false;
    m_editor->document()->setModified(false);
    m_saveBtn->setEnabled(false);
    m_highlighter->setLanguage(CodeHighlighter::None);
    updateTitle();
    m_statusLabel->setText(I18N::instance().tr("new_created"));
}

void ScriptEditor::onOpen() {
    if (m_modified) {
        auto r = QMessageBox::question(this, I18N::instance().tr("save_file"),
                                       I18N::instance().tr("unsaved_changes"),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (r == QMessageBox::Save) onSave();
        else if (r == QMessageBox::Cancel) return;
    }
    //фильтры файлов для диалога открытия
    QString filter = "All Supported (*.py *.sh *.bash *.rb *.go *.c *.cpp *.h *.hpp *.cs *.js *.ts *.pl *.pm *.php *.java *.rs *.lua *.ps1 *.sql *.html *.css *.smy);;SMY Files (*.smy);;Python Files (*.py);;Bash Scripts (*.sh *.bash);;Ruby Files (*.rb);;Go Files (*.go);;C/C++ Files (*.c *.cpp *.h *.hpp);;C# Files (*.cs);;JavaScript/TypeScript (*.js *.ts);;Perl Files (*.pl *.pm);;PHP Files (*.php);;Java Files (*.java);;Rust Files (*.rs);;Lua Files (*.lua);;PowerShell Files (*.ps1);;SQL Files (*.sql);;HTML Files (*.html);;CSS Files (*.css);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, I18N::instance().tr("open_file"),
                                                QString(), filter);
    if (path.isEmpty()) return;
    openFile(path);
}

void ScriptEditor::onSave() {
    if (m_isSmyFile) { saveAsSmyFile(m_currentPath); return; }
    if (m_currentPath.isEmpty()) { onSaveAs(); return; }
    QFile f(m_currentPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        //переведенная ошибка сохранения
        QMessageBox::critical(this, I18N::instance().tr("error"),
                              I18N::instance().tr("cannot_save") + "\n" + m_currentPath);
        return;
    }
    f.write(m_editor->toPlainText().toUtf8());
    f.close();
    m_modified = false;
    m_editor->document()->setModified(false);
    updateTitle();
    m_statusLabel->setText(I18N::instance().tr("saved_file") + " " + m_currentPath);
}

void ScriptEditor::saveAsSmyFile(const QString& outPath) {
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        //переведенная ошибка создания временного файла
        QMessageBox::critical(this, I18N::instance().tr("error"),
                              I18N::instance().tr("cannot_create_temp"));
        return;
    }
    tmpFile.write(m_editor->toPlainText().toUtf8());
    tmpFile.close();
    QString tmpPath = tmpFile.fileName();
    m_progress = new QProgressDialog(I18N::instance().tr("packing_smy"), QString(), 0, 0, this);
    m_progress->setWindowModality(Qt::WindowModal);
    m_progress->show();
    m_workerThread = new QThread(this);
    auto* worker = new SmyPackWorker(tmpPath, m_currentManifest, outPath, m_currentPassword);
    worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, worker, &SmyPackWorker::run);
    connect(worker, &SmyPackWorker::finished, this, &ScriptEditor::onSmyPackFinished);
    connect(worker, &SmyPackWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &SmyPackWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    m_workerThread->start();
}

void ScriptEditor::onSmyPackFinished(bool success, const QString& error) {
    if (m_progress) m_progress->close();
    if (!success) {
        //переведенная ошибка упаковки
        QMessageBox::critical(this, I18N::instance().tr("error"),
                              I18N::instance().tr("pack_failed") + "\n" + error);
        return;
    }
    m_modified = false;
    m_editor->document()->setModified(false);
    updateTitle();
    m_statusLabel->setText(I18N::instance().tr("saved_file") + " " + m_currentPath + " [SMY]");
    QMessageBox::information(this, I18N::instance().tr("success"),
                             I18N::instance().tr("smy_saved") + "\n" + m_currentPath);
}

void ScriptEditor::onSaveAs() {
    QString filter = "All Supported (*.py *.sh *.bash *.rb *.go *.c *.cpp *.h *.hpp *.cs *.js *.ts *.pl *.pm *.php *.java *.rs *.lua *.ps1 *.sql *.html *.css *.smy);;SMY Files (*.smy);;Python Files (*.py);;Bash Scripts (*.sh *.bash);;Ruby Files (*.rb);;Go Files (*.go);;C/C++ Files (*.c *.cpp *.h *.hpp);;C# Files (*.cs);;JavaScript/TypeScript (*.js *.ts);;Perl Files (*.pl *.pm);;PHP Files (*.php);;Java Files (*.java);;Rust Files (*.rs);;Lua Files (*.lua);;PowerShell Files (*.ps1);;SQL Files (*.sql);;HTML Files (*.html);;CSS Files (*.css);;All Files (*)";
    if (m_isSmyFile) filter = "SMY Files (*.smy)";
    QString path = QFileDialog::getSaveFileName(this, I18N::instance().tr("save_as"),
                                                QString(), filter);
    if (path.isEmpty()) return;
    if (path.endsWith(".smy")) {
        m_currentPath = path;
        saveAsSmyFile(path);
    } else {
        m_currentPath = path;
        m_isSmyFile = false;
        m_currentManifest = QJsonObject();
        m_currentPassword.clear();
        onSave();
    }
}

void ScriptEditor::onClose() {
    if (m_modified) {
        auto r = QMessageBox::question(this, I18N::instance().tr("save_file"),
                                       I18N::instance().tr("unsaved_changes"),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (r == QMessageBox::Save) onSave();
        else if (r == QMessageBox::Cancel) return;
    }
    close();
}
