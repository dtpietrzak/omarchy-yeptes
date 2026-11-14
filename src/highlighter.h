#pragma once

#include <QColor>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

struct SyntaxString {
    QString open, close;
    QChar escape;
    bool multiline = false;
};

// One language's rules, read from src/syntax/<name>.json.
struct SyntaxLanguage {
    enum Kind { Code, Markup, Markdown };
    Kind kind = Code;
    bool caseInsensitive = false;
    bool keyStrings = false;       // A string before ':' is a key (JSON).
    bool colonProperties = false;  // A word before ':' is a property (CSS, YAML).
    bool preprocessor = false;     // A line starting with '#' is a directive (C).
    bool capitalTypes = false;     // A Capitalised word is a type.
    QString identifierExtras;      // Extra characters allowed inside a word.
    QString sigils;                // Prefixes that mark a variable, like '$'.
    QSet<QString> keywords, types, literals;
    QStringList lineComments;
    QList<QPair<QString, QString>> blockComments;
    QList<SyntaxString> strings;
};

// Colors the fenced code blocks of a Yeptes note. A language's rules are read
// the first time a fence names it, so a note pays only for what it uses.
class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    enum Token { Keyword, Type, Literal, Number, String, Comment, Function, Property, Tag,
                 Fence, Heading, Strong, Emphasis, TokenCount };

    explicit Highlighter(QTextDocument *document, const QString &syntaxDirectory = ":/syntax");
    ~Highlighter() override;

    // Fences are only colored in Yeptes notes; other files stay plain.
    void setFencesEnabled(bool enabled);
    bool fencesEnabled() const { return m_enabled; }
    void setAccent(const QColor &accent);
    // The Code view colors the whole document in one language, by its
    // canonical name. An empty name goes back to coloring fences only.
    void setDocumentLanguage(const QString &name);
    QString documentLanguage() const { return m_documentLanguage; }

    QColor color(Token token) const { return m_formats[token].foreground().color(); }
    QStringList loadedLanguages() const;
    // Loads a language by its canonical name; null when there are no rules.
    const SyntaxLanguage *language(const QString &name);

    // Maps what follows the backticks ("js", "C++", "py") to a rules file name.
    static QString canonicalName(const QString &fenceInfo);
    // The language a line opens a fence for, or a null string when it doesn't.
    static QString fenceLanguage(const QString &line);
    static bool isFenceEnd(const QString &line);
    // Where the "[ ]" of a checklist line starts, or -1 when the line has none.
    static int checkboxStart(const QString &line);
    // The languages there are rules for, by canonical name, sorted.
    static QStringList availableLanguages(const QString &syntaxDirectory = ":/syntax");
    // How a language is written for people: "C++" for "cpp".
    static QString displayName(const QString &name);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    int highlightIn(const SyntaxLanguage &language, const QString &text, int state);
    int highlightCode(const SyntaxLanguage &language, const QString &text, int state);
    int highlightMarkup(const QString &text, int state);
    void highlightMarkdown(const QString &text);
    void highlightCheckbox(const QString &text);

    QString m_directory;
    bool m_enabled = true;
    QString m_documentLanguage;
    QColor m_accent = QColor("#FFD60A");
    QTextCharFormat m_formats[TokenCount];
    QHash<QString, SyntaxLanguage *> m_languages;
    // Fence languages seen so far; a block's state stores an index into it.
    QStringList m_fences;
};
