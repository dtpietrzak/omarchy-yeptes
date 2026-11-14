#include "highlighter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
QSet<QString> words(const QJsonValue &value) {
    const QStringList list = value.toString().split(' ', Qt::SkipEmptyParts);
    return QSet<QString>(list.begin(), list.end());
}
}

Highlighter::Highlighter(QTextDocument *document, const QString &syntaxDirectory)
    : QSyntaxHighlighter(document), m_directory(syntaxDirectory) {
    rebuildFormats();
}

Highlighter::~Highlighter() {
    qDeleteAll(m_languages);
}

void Highlighter::setFencesEnabled(bool enabled) {
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    rehighlight();
}

void Highlighter::setAccent(const QColor &accent) {
    if (!accent.isValid() || accent == m_accent)
        return;
    m_accent = accent;
    rebuildFormats();
    rehighlight();
}

void Highlighter::setDocumentLanguage(const QString &name) {
    if (name == m_documentLanguage)
        return;
    m_documentLanguage = name;
    rehighlight();
}

// Keywords and headings take the Omarchy accent; the rest is a fixed palette
// picked to read on the dark window.
void Highlighter::rebuildFormats() {
    auto set = [this](Token token, const QColor &color) {
        m_formats[token] = QTextCharFormat();
        m_formats[token].setForeground(color);
    };
    set(Keyword, m_accent);
    set(Type, QColor("#4fd6be"));
    set(Literal, QColor("#ff9e64"));
    set(Number, QColor("#ff9e64"));
    set(String, QColor("#9ece6a"));
    set(Comment, QColor("#6b7089"));
    set(Function, QColor("#7dcfff"));
    set(Property, QColor("#bb9af7"));
    set(Tag, QColor("#f7768e"));
    set(Fence, QColor("#7a7f99"));
    set(Heading, m_accent);
    m_formats[Comment].setFontItalic(true);
    m_formats[Heading].setFontWeight(QFont::Bold);
    m_formats[Strong] = QTextCharFormat();
    m_formats[Strong].setFontWeight(QFont::Bold);
    m_formats[Emphasis] = QTextCharFormat();
    m_formats[Emphasis].setFontItalic(true);
}

QStringList Highlighter::loadedLanguages() const {
    QStringList names;
    for (auto it = m_languages.begin(); it != m_languages.end(); ++it)
        if (it.value())
            names.append(it.key());
    names.sort();
    return names;
}

QStringList Highlighter::availableLanguages(const QString &syntaxDirectory) {
    QStringList names;
    for (const QString &file : QDir(syntaxDirectory).entryList({"*.json"}, QDir::Files))
        names.append(QFileInfo(file).completeBaseName());
    names.sort();
    return names;
}

QString Highlighter::displayName(const QString &name) {
    static const QHash<QString, QString> names{
        {"bash", "Bash"}, {"c", "C"}, {"cpp", "C++"}, {"csharp", "C#"}, {"css", "CSS"},
        {"go", "Go"}, {"html", "HTML"}, {"java", "Java"}, {"javascript", "JavaScript"},
        {"json", "JSON"}, {"kotlin", "Kotlin"}, {"lua", "Lua"}, {"markdown", "Markdown"},
        {"php", "PHP"}, {"python", "Python"}, {"ruby", "Ruby"}, {"rust", "Rust"},
        {"sql", "SQL"}, {"swift", "Swift"}, {"toml", "TOML"}, {"typescript", "TypeScript"},
        {"xml", "XML"}, {"yaml", "YAML"},
    };
    const auto found = names.constFind(name);
    if (found != names.constEnd())
        return found.value();
    return name.isEmpty() ? name : name.left(1).toUpper() + name.mid(1);
}

QString Highlighter::canonicalName(const QString &fenceInfo) {
    static const QHash<QString, QString> aliases{
        {"js", "javascript"}, {"jsx", "javascript"}, {"mjs", "javascript"}, {"cjs", "javascript"},
        {"node", "javascript"}, {"ts", "typescript"}, {"tsx", "typescript"},
        {"py", "python"}, {"python3", "python"},
        {"h", "c"}, {"c++", "cpp"}, {"cc", "cpp"}, {"cxx", "cpp"}, {"hpp", "cpp"}, {"hxx", "cpp"},
        {"cs", "csharp"}, {"c#", "csharp"},
        {"md", "markdown"}, {"htm", "html"}, {"xhtml", "html"}, {"svg", "xml"},
        {"jsonc", "json"}, {"json5", "json"},
        {"sh", "bash"}, {"shell", "bash"}, {"zsh", "bash"}, {"fish", "bash"},
        {"yml", "yaml"}, {"rs", "rust"}, {"golang", "go"}, {"rb", "ruby"}, {"kt", "kotlin"},
        {"scss", "css"}, {"less", "css"}, {"ini", "toml"},
        {"postgres", "sql"}, {"postgresql", "sql"}, {"mysql", "sql"}, {"sqlite", "sql"},
    };
    const QString name = fenceInfo.trimmed().toLower();
    return aliases.value(name, name);
}

QString Highlighter::fenceLanguage(const QString &line) {
    static const QRegularExpression open(QStringLiteral("^\\s{0,3}`{3,}\\s*([A-Za-z0-9_+#.-]*)\\s*$"));
    const auto match = open.match(line);
    if (!match.hasMatch())
        return QString();
    // An empty (not null) string is a fence without a language.
    const QString name = canonicalName(match.captured(1));
    return name.isEmpty() ? QStringLiteral("") : name;
}

bool Highlighter::isFenceEnd(const QString &line) {
    static const QRegularExpression close(QStringLiteral("^\\s{0,3}`{3,}\\s*$"));
    return close.match(line).hasMatch();
}

int Highlighter::checkboxStart(const QString &line) {
    static const QRegularExpression box(QStringLiteral("^\\s*(?:[-*+]|\\d+[.)])\\s+(\\[[ xX]\\])(?=\\s|$)"));
    const auto match = box.match(line);
    return match.hasMatch() ? int(match.capturedStart(1)) : -1;
}

// Checklist boxes are clickable in every file, so they are marked in every
// file: the box in the accent, and a ticked line dimmed as done.
void Highlighter::highlightCheckbox(const QString &text) {
    const int box = checkboxStart(text);
    if (box < 0)
        return;
    setFormat(box, 3, m_formats[Keyword]);
    if (text[box + 1] != ' ')
        setFormat(box + 3, text.size() - box - 3, m_formats[Fence]);
}

const SyntaxLanguage *Highlighter::language(const QString &name) {
    if (name.isEmpty())
        return nullptr;
    const auto found = m_languages.constFind(name);
    if (found != m_languages.constEnd())
        return found.value();

    // A name without rules is remembered as null, so it is only looked up once.
    SyntaxLanguage *language = nullptr;
    QFile file(m_directory + "/" + name + ".json");
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject rules = QJsonDocument::fromJson(file.readAll()).object();
        if (!rules.isEmpty()) {
            language = new SyntaxLanguage;
            const QString kind = rules.value("kind").toString();
            language->kind = kind == "markup" ? SyntaxLanguage::Markup
                           : kind == "markdown" ? SyntaxLanguage::Markdown : SyntaxLanguage::Code;
            language->caseInsensitive = rules.value("caseInsensitive").toBool();
            language->keyStrings = rules.value("keyStrings").toBool();
            language->colonProperties = rules.value("colonProperties").toBool();
            language->preprocessor = rules.value("preprocessor").toBool();
            language->capitalTypes = rules.value("capitalTypes").toBool();
            language->identifierExtras = rules.value("identifierExtras").toString();
            language->sigils = rules.value("sigils").toString();
            language->keywords = words(rules.value("keywords"));
            language->types = words(rules.value("types"));
            language->literals = words(rules.value("literals"));
            for (const auto &comment : rules.value("lineComments").toArray())
                language->lineComments.append(comment.toString());
            for (const auto &comment : rules.value("blockComments").toArray())
                language->blockComments.append({comment.toArray().at(0).toString(), comment.toArray().at(1).toString()});
            for (const auto &value : rules.value("strings").toArray()) {
                const QJsonObject object = value.toObject();
                SyntaxString string;
                string.open = object.value("open").toString();
                string.close = object.value("close").toString(string.open);
                const QString escape = object.value("escape").toString();
                if (!escape.isEmpty())
                    string.escape = escape.at(0);
                string.multiline = object.value("multiline").toBool();
                if (!string.open.isEmpty() && !string.close.isEmpty())
                    language->strings.append(string);
            }
        }
    }
    m_languages.insert(name, language);
    return language;
}

// A block's state is 0 outside a fence. Inside one, the high bits hold the
// fence's language (index into m_fences, plus one) and the low byte holds
// where the tokenizer stopped, such as inside a block comment.
void Highlighter::highlightBlock(const QString &text) {
    const int previous = qMax(0, previousBlockState());
    // The Code view: every line is in the one language, and the state is
    // just where the tokenizer stopped.
    if (!m_documentLanguage.isEmpty()) {
        const SyntaxLanguage *rules = language(m_documentLanguage);
        setCurrentBlockState(rules ? highlightIn(*rules, text, previous & 0xff) : 0);
        return;
    }
    if (!m_enabled) {
        highlightCheckbox(text);
        setCurrentBlockState(0);
        return;
    }

    if (previous == 0) {
        const QString name = fenceLanguage(text);
        if (name.isNull()) {
            highlightCheckbox(text);
            setCurrentBlockState(0);
            return;
        }
        setFormat(0, text.size(), m_formats[Fence]);
        int index = m_fences.indexOf(name);
        if (index < 0) {
            m_fences.append(name);
            index = m_fences.size() - 1;
        }
        // The language's name lights up once there are rules for it.
        if (language(name)) {
            const int at = text.lastIndexOf('`') + 1;
            setFormat(at, text.size() - at, m_formats[Keyword]);
        }
        setCurrentBlockState((index + 1) << 8);
        return;
    }

    if (isFenceEnd(text)) {
        setFormat(0, text.size(), m_formats[Fence]);
        setCurrentBlockState(0);
        return;
    }

    const SyntaxLanguage *rules = language(m_fences.value((previous >> 8) - 1));
    const int inner = rules ? highlightIn(*rules, text, previous & 0xff) : 0;
    setCurrentBlockState((previous & ~0xff) | inner);
}

int Highlighter::highlightIn(const SyntaxLanguage &language, const QString &text, int state) {
    if (language.kind == SyntaxLanguage::Markup)
        return highlightMarkup(text, state);
    if (language.kind == SyntaxLanguage::Markdown) {
        highlightMarkdown(text);
        return 0;
    }
    return highlightCode(language, text, state);
}

// States: 0 is plain code, 1+k is inside block comment k, 100+j is inside
// multi-line string j.
int Highlighter::highlightCode(const SyntaxLanguage &l, const QString &t, int state) {
    const int n = t.size();
    int i = 0;
    auto startsWith = [&](int at, const QString &s) { return QStringView(t).mid(at).startsWith(s); };
    // The index just past a string's closing delimiter, or -1 if it isn't on this line.
    auto stringEnd = [&](const SyntaxString &s, int from) {
        for (int p = from; p < n; ++p) {
            if (!s.escape.isNull() && t[p] == s.escape) {
                ++p;
                continue;
            }
            if (startsWith(p, s.close))
                return int(p + s.close.size());
        }
        return -1;
    };
    auto isWordStart = [](QChar c) { return c.isLetter() || c == '_'; };
    auto isWordPart = [&](QChar c) { return c.isLetterOrNumber() || c == '_' || l.identifierExtras.contains(c); };
    auto nextVisible = [&](int from) {
        while (from < n && t[from].isSpace())
            ++from;
        return from;
    };

    if (state >= 100 && state - 100 < l.strings.size()) {
        const int end = stringEnd(l.strings[state - 100], 0);
        if (end < 0) {
            setFormat(0, n, m_formats[String]);
            return state;
        }
        setFormat(0, end, m_formats[String]);
        i = end;
    } else if (state >= 1 && state - 1 < l.blockComments.size()) {
        const QString &close = l.blockComments[state - 1].second;
        const int end = t.indexOf(close);
        if (end < 0) {
            setFormat(0, n, m_formats[Comment]);
            return state;
        }
        setFormat(0, end + close.size(), m_formats[Comment]);
        i = end + close.size();
    }

    bool first = true;
    while (i < n) {
        const QChar c = t[i];
        if (c.isSpace()) {
            ++i;
            continue;
        }
        const bool lineStart = first;
        first = false;

        if (l.preprocessor && lineStart && c == '#') {
            setFormat(i, n - i, m_formats[Property]);
            return 0;
        }

        // Block comments first, so Lua's --[[ wins over its -- line comment.
        bool matched = false;
        for (int k = 0; k < l.blockComments.size() && !matched; ++k) {
            const auto &[open, close] = l.blockComments[k];
            if (!startsWith(i, open))
                continue;
            const int end = t.indexOf(close, i + open.size());
            if (end < 0) {
                setFormat(i, n - i, m_formats[Comment]);
                return 1 + k;
            }
            setFormat(i, end + close.size() - i, m_formats[Comment]);
            i = end + close.size();
            matched = true;
        }
        if (matched)
            continue;

        for (const QString &comment : l.lineComments) {
            if (startsWith(i, comment)) {
                setFormat(i, n - i, m_formats[Comment]);
                return 0;
            }
        }

        for (int j = 0; j < l.strings.size() && !matched; ++j) {
            const SyntaxString &s = l.strings[j];
            if (!startsWith(i, s.open))
                continue;
            const int end = stringEnd(s, i + s.open.size());
            if (end < 0) {
                // An unclosed one-line "string" is more likely an apostrophe.
                if (!s.multiline)
                    continue;
                setFormat(i, n - i, m_formats[String]);
                return 100 + j;
            }
            const int after = nextVisible(end);
            const bool key = l.keyStrings && after < n && t[after] == ':';
            setFormat(i, end - i, m_formats[key ? Property : String]);
            i = end;
            matched = true;
        }
        if (matched)
            continue;

        if (c.isDigit() || (c == '.' && i + 1 < n && t[i + 1].isDigit())) {
            int end = i + 1;
            while (end < n && (t[end].isLetterOrNumber() || t[end] == '.' || t[end] == '_'))
                ++end;
            setFormat(i, end - i, m_formats[Number]);
            i = end;
            continue;
        }

        if ((l.sigils.contains(c) || c == '@') && i + 1 < n && (isWordStart(t[i + 1]) || t[i + 1] == '{')) {
            int end = i + 2;
            while (end < n && (isWordPart(t[end]) || t[end] == '}'))
                ++end;
            setFormat(i, end - i, m_formats[Property]);
            i = end;
            continue;
        }

        if (isWordStart(c)) {
            int end = i + 1;
            while (end < n && isWordPart(t[end]))
                ++end;
            const QString word = l.caseInsensitive ? t.mid(i, end - i).toLower() : t.mid(i, end - i);
            const int after = nextVisible(end);
            const QChar next = after < n ? t[after] : QChar();
            if (l.keywords.contains(word))
                setFormat(i, end - i, m_formats[Keyword]);
            else if (l.types.contains(word))
                setFormat(i, end - i, m_formats[Type]);
            else if (l.literals.contains(word))
                setFormat(i, end - i, m_formats[Literal]);
            else if (next == '(')
                setFormat(i, end - i, m_formats[Function]);
            else if (l.colonProperties && next == ':' && !(after + 1 < n && t[after + 1] == ':'))
                setFormat(i, end - i, m_formats[Property]);
            else if (l.capitalTypes && c.isUpper())
                setFormat(i, end - i, m_formats[Type]);
            i = end;
            continue;
        }

        ++i;
    }
    return 0;
}

// States: 0 is text, 1 is inside a comment, 2 is inside a tag, 3 and 4 are
// inside a double- or single-quoted attribute value.
int Highlighter::highlightMarkup(const QString &t, int state) {
    const int n = t.size();
    int i = 0;
    auto isNamePart = [](QChar c) {
        return c.isLetterOrNumber() || c == '-' || c == '_' || c == ':' || c == '.' || c == '@';
    };
    while (i < n) {
        const QChar c = t[i];
        if (state == 1) {
            const int end = t.indexOf(QLatin1String("-->"), i);
            if (end < 0) {
                setFormat(i, n - i, m_formats[Comment]);
                return 1;
            }
            setFormat(i, end + 3 - i, m_formats[Comment]);
            i = end + 3;
            state = 0;
        } else if (state == 3 || state == 4) {
            const int end = t.indexOf(state == 3 ? '"' : '\'', i);
            if (end < 0) {
                setFormat(i, n - i, m_formats[String]);
                return state;
            }
            setFormat(i, end + 1 - i, m_formats[String]);
            i = end + 1;
            state = 2;
        } else if (state == 2) {
            if (c == '>' || c == '/' || c == '?') {
                setFormat(i, 1, m_formats[Tag]);
                if (c == '>')
                    state = 0;
                ++i;
            } else if (c == '"' || c == '\'') {
                setFormat(i, 1, m_formats[String]);
                state = c == '"' ? 3 : 4;
                ++i;
            } else if (isNamePart(c)) {
                int end = i + 1;
                while (end < n && isNamePart(t[end]))
                    ++end;
                setFormat(i, end - i, m_formats[Property]);
                i = end;
            } else {
                ++i;
            }
        } else if (QStringView(t).mid(i).startsWith(QLatin1String("<!--"))) {
            state = 1;
        } else if (c == '<' && i + 1 < n && (t[i + 1].isLetter() || QStringLiteral("/!?").contains(t[i + 1]))) {
            int end = i + 1;
            if (!t[end].isLetter())
                ++end;
            while (end < n && isNamePart(t[end]))
                ++end;
            setFormat(i, end - i, m_formats[Tag]);
            i = end;
            state = 2;
        } else if (c == '&') {
            int end = i + 1;
            while (end < n && (t[end].isLetterOrNumber() || t[end] == '#'))
                ++end;
            if (end < n && t[end] == ';' && end > i + 1) {
                setFormat(i, end + 1 - i, m_formats[Literal]);
                i = end + 1;
            } else {
                ++i;
            }
        } else {
            ++i;
        }
    }
    return state;
}

void Highlighter::highlightMarkdown(const QString &t) {
    static const QRegularExpression heading(QStringLiteral("^\\s{0,3}#{1,6}\\s"));
    static const QRegularExpression quote(QStringLiteral("^\\s{0,3}>"));
    static const QRegularExpression rule(QStringLiteral("^\\s{0,3}([-*_])(\\s*\\1){2,}\\s*$"));
    static const QRegularExpression marker(QStringLiteral("^\\s*([-*+]|\\d+[.)])\\s"));
    static const QRegularExpression strong(QStringLiteral("(\\*\\*|__)(?=\\S).+?(?<=\\S)\\1"));
    static const QRegularExpression emphasis(QStringLiteral("(?<![*_\\w])([*_])(?=[^\\s*_])[^*_]*?(?<=[^\\s*_])\\1(?![*_\\w])"));
    static const QRegularExpression link(QStringLiteral("!?\\[[^\\]]+\\]\\([^)]*\\)"));
    static const QRegularExpression code(QStringLiteral("`[^`]+`"));

    if (heading.match(t).hasMatch()) {
        setFormat(0, t.size(), m_formats[Heading]);
        return;
    }
    if (quote.match(t).hasMatch()) {
        setFormat(0, t.size(), m_formats[Comment]);
        return;
    }
    if (rule.match(t).hasMatch()) {
        setFormat(0, t.size(), m_formats[Fence]);
        return;
    }
    const auto bullet = marker.match(t);
    if (bullet.hasMatch())
        setFormat(bullet.capturedStart(1), bullet.capturedLength(1), m_formats[Keyword]);

    auto each = [&](const QRegularExpression &expression, Token token) {
        auto matches = expression.globalMatch(t);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), m_formats[token]);
        }
    };
    each(strong, Strong);
    each(emphasis, Emphasis);
    each(link, Function);
    each(code, String);
}
