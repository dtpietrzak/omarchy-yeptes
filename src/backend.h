#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class FilePicker;
class Highlighter;
class QTextDocument;

// The note being edited: its text, the file it belongs to, and loading and
// saving it. The editor's QTextDocument is the one copy of the text once it
// is attached; before that (and in tests) the text lives here.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int characterCount READ characterCount NOTIFY textChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(QUrl baseUrl READ baseUrl NOTIFY fileChanged)
    Q_PROPERTY(QString suffix READ suffix NOTIFY fileChanged)
    Q_PROPERTY(bool note READ isNote NOTIFY fileChanged)
    Q_PROPERTY(QString codeLanguage READ codeLanguage WRITE setCodeLanguage NOTIFY codeLanguageChanged)
    Q_PROPERTY(QString codeLanguageName READ codeLanguageName NOTIFY codeLanguageChanged)
    Q_PROPERTY(bool codeView READ codeView WRITE setCodeView NOTIFY codeViewChanged)
    Q_PROPERTY(int historyCount READ historyCount NOTIFY historyChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)

public:
    // Files past this size are refused rather than opened slowly.
    static constexpr qint64 maximumSize = 16 * 1024 * 1024;

    struct FileText {
        QString text;
        bool crlf = false, bom = false;
    };

    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    int characterCount() const { return m_text.size(); }
    QString filePath() const { return m_path; }
    QString fileName() const;
    QUrl baseUrl() const;
    // The file's extension in lower case; "ypts" for a note not saved yet.
    QString suffix() const;
    // A Yeptes note (.ypts, or one not saved yet) gets its fences colored and
    // keeps a history of its saves.
    bool isNote() const;
    // The language the Code view colors the text in, by canonical name, or
    // empty for none. Set from the file's extension when a file is loaded.
    QString codeLanguage() const { return m_codeLanguage; }
    void setCodeLanguage(const QString &name);
    QString codeLanguageName() const;
    // While the Code view shows, the editor colors the whole text.
    bool codeView() const { return m_codeView; }
    void setCodeView(bool on);
    // Where the syntax rules are read from; tests point it at the sources.
    void setSyntaxDirectory(const QString &directory) { m_syntaxDirectory = directory; }
    int historyCount() const { return m_history.size(); }
    bool modified() const { return m_text != m_savedText; }
    QString message() const { return m_message; }
    bool messageIsError() const { return m_messageIsError; }

    Q_INVOKABLE void newNote();
    Q_INVOKABLE void open();
    // Installed font families that are fixed-pitch.
    Q_INVOKABLE QStringList monospaceFamilies() const;
    // The .ypts files in Documents/Yeptes, newest first: name, path, date.
    Q_INVOKABLE QVariantList notes() const;
    // The notes folder as people write it: "~/Documents/Yeptes/".
    Q_INVOKABLE QString notesFolderLabel() const;
    // The paths of notes whose name or text contains the query, ignoring
    // case. The open note is searched as it is now, saved or not.
    Q_INVOKABLE QStringList notesContaining(const QString &query) const;
    // Whether a path is inside Documents/Yeptes.
    Q_INVOKABLE bool inNotesFolder(const QString &path) const;
    // Moves a note in Documents/Yeptes to the trash. If it is the open note,
    // the editor goes back to a new, empty one.
    Q_INVOKABLE bool deleteNote(const QString &path);
    Q_INVOKABLE bool load(const QString &path);
    Q_INVOKABLE bool loadUrl(const QUrl &url);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE bool saveTo(const QString &path);
    // Writes the note's text, without its history, to another file and
    // stays on the note.
    Q_INVOKABLE void exportAs(const QString &extension = QStringLiteral(".txt"));
    Q_INVOKABLE bool exportTo(const QString &path);
    // The export extensions the text is actually formatted as (".json" when
    // it parses, ".csv" when it is a table, and so on). Plain text and notes
    // fit anything, so ".txt" and ".ypts" are always there.
    Q_INVOKABLE QStringList fittingFormats() const;

    // Called once by the editor with its textDocument.
    Q_INVOKABLE void attachDocument(QObject *quickDocument);
    void attachTextDocument(QTextDocument *document);
    // Replaces part of the text as one undoable step.
    Q_INVOKABLE void replaceRange(int start, int end, const QString &replacement);
    // Where the checklist box around a position in the text starts, or -1.
    Q_INVOKABLE int checkboxAt(int position) const;
    // Ticks or unticks the checklist box at a position in the text; false
    // when there is no box there.
    Q_INVOKABLE bool toggleCheckbox(int position);
    // The Diff view's rows: unsaved changes first, then each saved change of
    // a note, newest first.
    Q_INVOKABLE QVariantList diffRows() const;
    // The languages the Code view can color in: id and name, sorted by id.
    Q_INVOKABLE QVariantList codeLanguages() const;
    // The 1-based line and column of a position in the text.
    Q_INVOKABLE QVariantMap cursorInfo(int position) const;
    // The note as rich text for the Markdown ("markdown") or HTML ("html")
    // view: scripts removed, links in the accent, Markdown given some air.
    Q_INVOKABLE QString rendered(const QString &view, int pixelSize) const;

    void setAccent(const QString &accent);
    void setPalette(const QHash<QString, QString> &palette);
    Highlighter *highlighter() const { return m_highlighter; }

    static bool readFile(const QString &path, FileText *result, QString *error);
    static QString stripScripts(const QString &html);

    // A .ypts file is the note's text, then (once it has been changed and
    // saved) a marker line and one JSON line per saved change.
    static const QString &historyMarker();
    static void splitHistory(const QString &contents, QString *text, QStringList *history);
    static QString joinHistory(const QString &text, const QStringList &history);

signals:
    void textChanged();
    void fileChanged();
    void modifiedChanged();
    void messageChanged();
    void historyChanged();
    void codeLanguageChanged();
    void codeViewChanged();
    void loaded();
    void saved();
    void notesChanged();

private:
    void replaceAll(const QString &text, const QString &path, bool crlf, bool bom, const QStringList &history = {});
    bool writeFile(const QString &path, const QString &contents, QString *error) const;
    void setMessage(const QString &message, bool error = false);
    void syncFromDocument();
    void takeText(const QString &text);
    void applyHighlighting();
    // The language a file's extension names, or empty when there are no rules.
    QString languageForSuffix() const;

    FilePicker *m_picker = nullptr;
    QPointer<QTextDocument> m_document;
    Highlighter *m_highlighter = nullptr;
    QString m_text, m_savedText, m_path, m_message, m_accent;
    QHash<QString, QString> m_palette;
    QString m_codeLanguage, m_syntaxDirectory = QStringLiteral(":/syntax");
    bool m_codeView = false;
    // The saved changes of a note, oldest first, exactly as they are in the file.
    QStringList m_history;
    bool m_messageIsError = false;
    // Line endings and the byte order mark are put back as they were found.
    bool m_crlf = false, m_bom = false;
};
