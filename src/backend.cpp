#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLocale>
#include <QJsonObject>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QVariantMap>

#include "diff.h"
#include "filepicker.h"
#include "highlighter.h"
#include "jsonmodel.h"
#include "tablemodel.h"

Backend::Backend(FilePicker *picker, QObject *parent) : QObject(parent), m_picker(picker) {
    if (!m_picker)
        return;
    connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) { loadUrl(url); });
    connect(m_picker, &FilePicker::saveSelected, this, [this](const QUrl &url) { saveTo(url.toLocalFile()); });
    connect(m_picker, &FilePicker::exportSelected, this, [this](const QUrl &url) { exportTo(url.toLocalFile()); });
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) { setMessage(message, true); });
}

QString Backend::fileName() const {
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).fileName();
}

QUrl Backend::baseUrl() const {
    return m_path.isEmpty() ? QUrl() : QUrl::fromLocalFile(QFileInfo(m_path).absolutePath() + "/");
}

QString Backend::suffix() const {
    return m_path.isEmpty() ? QStringLiteral("ypts") : QFileInfo(m_path).suffix().toLower();
}

bool Backend::isNote() const {
    return suffix() == "ypts";
}

// The marker starts with a control character nobody types, so a line of a
// note is never mistaken for it.
const QString &Backend::historyMarker() {
    static const QString marker = QChar(0x1e) + QStringLiteral("yeptes history: one saved change per line, oldest first");
    return marker;
}

void Backend::splitHistory(const QString &contents, QString *text, QStringList *history) {
    // History lines are JSON, which escapes the control character, so the
    // last marker in the file is always the real one.
    const int at = contents.lastIndexOf('\n' + historyMarker());
    const QString after = at < 0 ? QString() : contents.mid(at + 1 + historyMarker().size());
    if (at < 0 || !(after.isEmpty() || after.startsWith('\n'))) {
        *text = contents;
        history->clear();
        return;
    }
    *text = contents.left(at);
    *history = after.split('\n', Qt::SkipEmptyParts);
}

QString Backend::joinHistory(const QString &text, const QStringList &history) {
    // A note without saved changes is just its text, unless that text would
    // itself read as a history marker when loaded again.
    if (history.isEmpty() && !text.contains(historyMarker()))
        return text;
    QString contents = text + '\n' + historyMarker() + '\n';
    for (const QString &line : history)
        contents += line + '\n';
    return contents;
}

void Backend::setMessage(const QString &message, bool error) {
    m_message = message;
    m_messageIsError = error;
    emit messageChanged();
}

void Backend::takeText(const QString &text) {
    if (text == m_text)
        return;
    const bool was = modified();
    m_text = text;
    emit textChanged();
    if (was != modified())
        emit modifiedChanged();
}

void Backend::setText(const QString &text) {
    if (m_document) {
        if (text != m_text) {
            m_document->setPlainText(text);
            syncFromDocument();
        }
    } else {
        takeText(text);
    }
}

// The document's text block by block rather than toPlainText(), which would
// turn every non-breaking space into a plain one.
void Backend::syncFromDocument() {
    if (!m_document)
        return;
    QString text;
    for (QTextBlock block = m_document->begin(); block.isValid(); block = block.next()) {
        if (block != m_document->begin())
            text += '\n';
        text += block.text();
    }
    takeText(text);
}

void Backend::attachDocument(QObject *quickDocument) {
    if (auto *document = qobject_cast<QQuickTextDocument *>(quickDocument))
        attachTextDocument(document->textDocument());
}

void Backend::attachTextDocument(QTextDocument *document) {
    if (!document || document == m_document)
        return;
    const bool clean = !modified();
    m_document = document;
    m_document->setPlainText(m_text);
    connect(m_document, &QTextDocument::contentsChanged, this, &Backend::syncFromDocument);
    syncFromDocument();
    // The document may normalize what it was given (a lone carriage return
    // becomes a line break); that alone must not count as an edit.
    if (clean && modified()) {
        m_savedText = m_text;
        emit modifiedChanged();
    }

    m_highlighter = new Highlighter(m_document, m_syntaxDirectory);
    m_highlighter->setAccent(QColor(m_accent));
    m_highlighter->setPalette(m_palette);
    applyHighlighting();
}

void Backend::applyHighlighting() {
    if (!m_highlighter)
        return;
    m_highlighter->setFencesEnabled(isNote());
    m_highlighter->setDocumentLanguage(m_codeView ? m_codeLanguage : QString());
}

QString Backend::languageForSuffix() const {
    const QString name = Highlighter::canonicalName(suffix());
    return Highlighter::availableLanguages(m_syntaxDirectory).contains(name) ? name : QString();
}

void Backend::setCodeLanguage(const QString &name) {
    if (name == m_codeLanguage)
        return;
    m_codeLanguage = name;
    applyHighlighting();
    emit codeLanguageChanged();
}

QString Backend::codeLanguageName() const {
    return Highlighter::displayName(m_codeLanguage);
}

void Backend::setCodeView(bool on) {
    if (on == m_codeView)
        return;
    m_codeView = on;
    applyHighlighting();
    emit codeViewChanged();
}

QVariantList Backend::codeLanguages() const {
    QVariantList languages;
    for (const QString &name : Highlighter::availableLanguages(m_syntaxDirectory))
        languages.append(QVariantMap{{"id", name}, {"label", Highlighter::displayName(name)}});
    return languages;
}

void Backend::setAccent(const QString &accent) {
    m_accent = accent;
    if (m_highlighter)
        m_highlighter->setAccent(QColor(accent));
}

void Backend::setPalette(const QHash<QString, QString> &palette) {
    m_palette = palette;
    if (m_highlighter)
        m_highlighter->setPalette(palette);
}

void Backend::replaceRange(int start, int end, const QString &replacement) {
    if (start < 0 || end < start || end > m_text.size())
        return;
    if (!m_document) {
        takeText(m_text.left(start) + replacement + m_text.mid(end));
        return;
    }
    QTextCursor cursor(m_document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
}

QVariantMap Backend::cursorInfo(int position) const {
    int line = 1, column = 1;
    if (m_document) {
        const QTextBlock block = m_document->findBlock(position);
        if (block.isValid()) {
            line = block.blockNumber() + 1;
            column = position - block.position() + 1;
        }
    }
    return {{"line", line}, {"column", column}};
}

void Backend::replaceAll(const QString &text, const QString &path, bool crlf, bool bom, const QStringList &history) {
    m_path = path;
    m_history = history;
    m_crlf = crlf;
    m_bom = bom;
    m_codeLanguage = languageForSuffix();
    applyHighlighting();
    emit codeLanguageChanged();
    // Loading through the document also drops the previous file's undo steps.
    if (m_document) {
        m_document->setPlainText(text);
        syncFromDocument();
    } else {
        takeText(text);
    }
    const bool was = modified();
    m_savedText = m_text;
    if (was)
        emit modifiedChanged();
    emit fileChanged();
    emit historyChanged();
    emit loaded();
}

void Backend::newNote() {
    replaceAll(QString(), QString(), false, false);
    setMessage(QString());
}

void Backend::open() {
    if (!m_picker)
        return;
    m_picker->openFile(m_path.isEmpty() ? QDir::homePath() : QFileInfo(m_path).absolutePath());
}

bool Backend::readFile(const QString &path, FileText *result, QString *error) {
    const QFileInfo info(path);
    const QString name = info.fileName();
    if (info.isDir()) {
        *error = tr("%1 is a folder, not a file.").arg(name);
        return false;
    }
    if (info.size() > maximumSize) {
        *error = tr("%1 is too large to open (the limit is %2 MB).").arg(name).arg(maximumSize / (1024 * 1024));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = tr("Could not open %1: %2").arg(name, file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        *error = tr("Could not open %1: %2").arg(name, file.errorString());
        return false;
    }

    // Only text that will be written back unchanged is opened: a file that
    // isn't UTF-8 would be damaged by the first save.
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(data);
    if (decoder.hasError() || data.contains('\0')) {
        *error = tr("%1 is not UTF-8 text, so it was not opened.").arg(name);
        return false;
    }
    result->bom = data.startsWith("\xEF\xBB\xBF");
    result->crlf = text.contains(QLatin1String("\r\n"));
    result->text = text;
    result->text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    return true;
}

bool Backend::load(const QString &path) {
    if (path.isEmpty())
        return false;
    const QString absolute = QFileInfo(path).absoluteFilePath();

    // Like any editor, naming a file that isn't there starts it.
    if (!QFileInfo::exists(absolute)) {
        replaceAll(QString(), absolute, false, false);
        setMessage(tr("%1 is a new file.").arg(fileName()));
        return true;
    }

    FileText file;
    QString error;
    if (!readFile(absolute, &file, &error)) {
        setMessage(error, true);
        return false;
    }
    QStringList history;
    if (QFileInfo(absolute).suffix().compare("ypts", Qt::CaseInsensitive) == 0)
        splitHistory(QString(file.text), &file.text, &history);
    replaceAll(file.text, absolute, file.crlf, file.bom, history);
    setMessage(QString());
    return true;
}

bool Backend::loadUrl(const QUrl &url) {
    if (!url.isLocalFile()) {
        setMessage(tr("Only files on this computer can be opened."), true);
        return false;
    }
    return load(url.toLocalFile());
}

// Where new notes are kept: Documents/Yeptes, made on demand.
static QString notesFolder() {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
        documents = QDir::homePath() + QStringLiteral("/Documents");
    return documents + QStringLiteral("/Yeptes");
}

QString Backend::notesFolderLabel() const {
    QString folder = QDir::cleanPath(notesFolder());
    const QString home = QDir::cleanPath(QDir::homePath());
    if (folder == home || folder.startsWith(home + QLatin1Char('/')))
        folder = QLatin1Char('~') + folder.mid(home.size());
    return folder + QLatin1Char('/');
}

bool Backend::inNotesFolder(const QString &path) const {
    auto resolved = [](const QString &p) {
        const QFileInfo info(p);
        return info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    };
    return resolved(path).startsWith(resolved(notesFolder()) + QLatin1Char('/'));
}

QVariantList Backend::notes() const {
    QVariantList list;
    const QFileInfoList files = QDir(notesFolder()).entryInfoList(
        {QStringLiteral("*.ypts")}, QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &file : files)
        list.append(QVariantMap{{"name", file.completeBaseName()},
                                {"path", file.absoluteFilePath()},
                                {"date", QLocale().toString(file.lastModified().date(), QLocale::ShortFormat)}});
    return list;
}

QStringList Backend::notesContaining(const QString &query) const {
    QStringList paths;
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return paths;
    const QFileInfoList files = QDir(notesFolder()).entryInfoList(
        {QStringLiteral("*.ypts")}, QDir::Files | QDir::Readable);
    for (const QFileInfo &file : files) {
        const QString path = file.absoluteFilePath();
        QString text;
        if (path == m_path) {
            text = m_text;
        } else {
            QFile handle(path);
            if (!handle.open(QIODevice::ReadOnly))
                continue;
            QStringList history;
            splitHistory(QString::fromUtf8(handle.readAll()), &text, &history);
        }
        if (file.completeBaseName().contains(needle, Qt::CaseInsensitive)
            || text.contains(needle, Qt::CaseInsensitive))
            paths.append(path);
    }
    return paths;
}

bool Backend::deleteNote(const QString &path) {
    const QFileInfo info(path);
    const QString name = info.fileName();
    // Only notes in the notes folder can be deleted from the list.
    if (info.absolutePath() != QFileInfo(notesFolder()).absoluteFilePath()
        || info.suffix().compare("ypts", Qt::CaseInsensitive) != 0 || !info.isFile()) {
        setMessage(tr("Could not delete %1").arg(name), true);
        return false;
    }
    if (!QFile::moveToTrash(info.absoluteFilePath())) {
        setMessage(tr("Could not move %1 to the trash").arg(name), true);
        return false;
    }
    if (info.absoluteFilePath() == m_path)
        newNote();
    setMessage(tr("Deleted %1").arg(name));
    emit notesChanged();
    return true;
}

void Backend::save() {
    if (!m_path.isEmpty()) {
        saveTo(m_path);
        return;
    }
    // A new note with nothing in it has nothing to save.
    if (m_text.trimmed().isEmpty())
        return;

    // Named after its first line, and never over a file that is already there.
    QString base = m_text.trimmed().section(QLatin1Char('\n'), 0, 0).trimmed();
    base.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N} _.-]")));
    base = base.simplified().left(32).trimmed();
    while (base.startsWith(QLatin1Char('.')))
        base.remove(0, 1);
    if (base.isEmpty())
        base = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh.mm.ss"));

    const QString folder = notesFolder();
    if (!QDir().mkpath(folder)) {
        setMessage(tr("Could not create %1").arg(folder), true);
        return;
    }
    QString path = folder + QLatin1Char('/') + base + QStringLiteral(".ypts");
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = folder + QLatin1Char('/') + base + QStringLiteral(" (%1).ypts").arg(n);
    saveTo(path);
}

void Backend::saveAs() {
    if (!m_picker)
        return;
    if (m_path.isEmpty())
        QDir().mkpath(notesFolder());
    const QFileInfo info(m_path);
    m_picker->saveFile(m_path.isEmpty() ? notesFolder() : info.absolutePath(),
                       m_path.isEmpty() ? QStringLiteral("untitled.ypts") : info.fileName());
}

bool Backend::saveTo(const QString &path) {
    if (path.isEmpty())
        return false;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    const QString name = QFileInfo(absolute).fileName();

    // A note records what changed since it was last saved. The first save of
    // a new note has nothing to compare against, so it records nothing.
    const bool note = QFileInfo(absolute).suffix().compare("ypts", Qt::CaseInsensitive) == 0;
    QStringList history;
    if (note) {
        history = m_history;
        if (!m_path.isEmpty() && m_text != m_savedText) {
            const Diff::Result change = Diff::compare(m_savedText, m_text);
            if (change.added + change.removed > 0) {
                QJsonObject entry = Diff::toJson(change);
                entry.insert("at", QDateTime::currentDateTime().toOffsetFromUtc(
                                       QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate));
                history.append(QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact)));
            }
        }
    }

    QString error;
    if (!writeFile(absolute, note ? joinHistory(m_text, history) : m_text, &error)) {
        setMessage(tr("Could not save %1: %2").arg(name, error), true);
        return false;
    }
    m_history = history;
    emit historyChanged();

    const bool was = modified();
    const bool moved = absolute != m_path;
    m_path = absolute;
    m_savedText = m_text;
    if (moved) {
        // Saved under a name that says its language: that language it is.
        // Otherwise the one picked for the Code view stays.
        const QString language = languageForSuffix();
        if (!language.isEmpty())
            setCodeLanguage(language);
        applyHighlighting();
        emit fileChanged();
    }
    if (was)
        emit modifiedChanged();
    setMessage(tr("Saved %1").arg(name));
    emit saved();
    return true;
}

QString Backend::rendered(const QString &view, int pixelSize) const {
    const bool markdown = view == "markdown";
    QTextDocument document;
    QFont font = QGuiApplication::font();
    font.setPixelSize(qMax(6, pixelSize));
    document.setDefaultFont(font);
    if (markdown)
        document.setMarkdown(m_text);
    else
        document.setHtml(stripScripts(m_text));

    const QColor accent(m_accent.isEmpty() ? QStringLiteral("#FFD60A") : m_accent);
    auto isCode = [](const QTextBlock &block) {
        return block.isValid() && (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)
                                   || block.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage));
    };

    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);

        // Qt lays Markdown out with no space between blocks; add the margins
        // a reader expects and set code and quotes apart.
        if (markdown) {
            QTextBlockFormat format = block.blockFormat();
            const bool code = isCode(block);
            if (format.headingLevel() > 0) {
                format.setTopMargin(block == document.begin() ? 0 : pixelSize * 1.1);
                format.setBottomMargin(pixelSize * 0.5);
            } else if (code) {
                format.setBackground(QColor(m_palette.value("lighter_background", "#1b1b21")));
                format.setTopMargin(isCode(block.previous()) ? 0 : pixelSize * 0.3);
                format.setBottomMargin(isCode(block.next()) ? 0 : pixelSize * 0.9);
            } else if (block.textList()) {
                format.setBottomMargin(block.next().isValid() && block.next().textList() ? pixelSize * 0.2 : pixelSize * 0.7);
            } else {
                format.setBottomMargin(pixelSize * 0.7);
            }
            cursor.setBlockFormat(format);
            if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
                QTextCharFormat quoted;
                quoted.setForeground(QColor(m_palette.value("dark_foreground", "#9a9aa6")));
                quoted.setFontItalic(true);
                cursor.select(QTextCursor::BlockUnderCursor);
                cursor.mergeCharFormat(quoted);
            }
        }

        // Links take the accent instead of the toolkit's default blue, which
        // is hard to read on the dark window.
        QList<QPair<int, int>> links;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.charFormat().isAnchor() && !fragment.charFormat().anchorHref().isEmpty())
                links.append({fragment.position(), fragment.length()});
        }
        for (const auto &[position, length] : links) {
            QTextCharFormat link;
            link.setForeground(accent);
            cursor.setPosition(position);
            cursor.setPosition(position + length, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(link);
        }
    }
    return document.toHtml();
}

// Written beside the target and renamed over it, so a failed save never
// leaves half a file. Where the folder can't take a second file but the file
// itself is writable, it is written in place instead.
bool Backend::writeFile(const QString &path, const QString &contents, QString *error) const {
    QString text = contents;
    if (m_crlf)
        text.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    QByteArray data = text.toUtf8();
    if (m_bom)
        data.prepend("\xEF\xBB\xBF");

    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

QStringList Backend::monospaceFamilies() const {
    QStringList families;
    const QStringList all = QFontDatabase::families();
    for (const QString &family : all)
        if (QFontDatabase::isFixedPitch(family))
            families.append(family);
    return families;
}

void Backend::exportAs(const QString &extension) {
    if (!m_picker)
        return;
    const QFileInfo info(m_path);
    m_picker->exportFile(m_path.isEmpty() ? QDir::homePath() : info.absolutePath(),
                         (m_path.isEmpty() ? QStringLiteral("untitled") : info.completeBaseName()) + extension);
}

QStringList Backend::fittingFormats() const {
    QStringList formats{QStringLiteral(".ypts"), QStringLiteral(".txt")};
    const QString text = m_text.trimmed();
    if (text.isEmpty())
        return formats;

    JsonNode root;
    QString error;
    if (JsonModel::parse(text, &root, &error))
        formats.append(QStringLiteral(".json"));

    // A table has at least two columns and two rows, and every row reaches the last column.
    for (const QString &suffix : {QStringLiteral("csv"), QStringLiteral("tsv")}) {
        TableModel table;
        table.setText(m_text, suffix);
        const int columns = table.columnCount();
        bool fits = columns >= 2 && table.rowCount() >= 2 && table.delimiter() == (suffix == "tsv" ? QLatin1Char('\t') : QLatin1Char(','));
        for (int row = 0; fits && row < table.rowCount(); ++row)
            fits = table.cellStart(row, columns - 1) >= 0;
        if (fits)
            formats.append(QLatin1Char('.') + suffix);
    }

    static const QRegularExpression html(
        QStringLiteral("<(!doctype|html|head|body|div|p|h[1-6]|span|a|ul|ol|li|table|section|article|main|img|br|script|style)\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    if (html.match(text).hasMatch())
        formats.append(QStringLiteral(".html"));

    static const QRegularExpression markdown(
        QStringLiteral("^(#{1,6}\\s|\\s*[-*+]\\s|\\s*\\d+[.)]\\s|>\\s?|```|\\s*[-*_]{3,}\\s*$)|\\[[^\\]]+\\]\\([^)]+\\)|\\*\\*[^*]+\\*\\*|`[^`]+`"),
        QRegularExpression::MultilineOption);
    if (markdown.match(text).hasMatch())
        formats.append(QStringLiteral(".md"));
    return formats;
}

bool Backend::exportTo(const QString &path) {
    if (path.isEmpty())
        return false;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    const QString name = QFileInfo(absolute).fileName();
    // Exporting over the note would replace it with a copy that has lost its history.
    if (absolute == m_path) {
        setMessage(tr("Export to a different file than the note itself."), true);
        return false;
    }
    QString error;
    if (!writeFile(absolute, m_text, &error)) {
        setMessage(tr("Could not save %1: %2").arg(name, error), true);
        return false;
    }
    setMessage(tr("Exported %1").arg(name));
    return true;
}

int Backend::checkboxAt(int position) const {
    if (position < 0 || position > m_text.size())
        return -1;
    const int lineStart = position == 0 ? 0 : m_text.lastIndexOf('\n', position - 1) + 1;
    int lineEnd = m_text.indexOf('\n', position);
    if (lineEnd < 0)
        lineEnd = m_text.size();
    const int box = Highlighter::checkboxStart(m_text.mid(lineStart, lineEnd - lineStart));
    // A position is between two characters, so either edge of the box counts.
    if (box < 0 || position < lineStart + box || position > lineStart + box + 3)
        return -1;
    return lineStart + box;
}

bool Backend::toggleCheckbox(int position) {
    const int box = checkboxAt(position);
    if (box < 0)
        return false;
    replaceRange(box + 1, box + 2, m_text[box + 1] == ' ' ? QStringLiteral("x") : QStringLiteral(" "));
    return true;
}

QVariantList Backend::diffRows() const {
    QVariantList rows;
    auto append = [&rows](const Diff::Result &change, const QString &at, bool unsaved) {
        rows.append(QVariantMap{{"kind", "entry"}, {"at", at}, {"unsaved", unsaved},
                                {"added", change.added}, {"removed", change.removed}});
        for (int i = 0; i < change.hunks.size(); ++i) {
            if (i > 0)
                rows.append(QVariantMap{{"kind", "gap"}});
            for (const Diff::Line &line : change.hunks[i].lines)
                rows.append(QVariantMap{{"kind", line.kind == '+' ? "add" : line.kind == '-' ? "remove" : "context"},
                                        {"text", line.text}, {"oldNumber", line.oldNumber}, {"newNumber", line.newNumber}});
        }
    };
    if (modified())
        append(Diff::compare(m_savedText, m_text), QString(), true);
    for (int i = m_history.size() - 1; i >= 0; --i) {
        const QJsonObject entry = QJsonDocument::fromJson(m_history[i].toUtf8()).object();
        // A line that isn't a change is kept in the file but has nothing to show.
        if (!entry.isEmpty())
            append(Diff::fromJson(entry), entry.value("at").toString(), false);
    }
    return rows;
}

// The HTML view shows a page, it doesn't run one.
QString Backend::stripScripts(const QString &html) {
    static const QRegularExpression block(
        QStringLiteral("<script\\b[^>]*>.*?</script\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression unclosed(
        QStringLiteral("<script\\b.*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QString result = html;
    result.remove(block);
    result.remove(unclosed);
    return result;
}
