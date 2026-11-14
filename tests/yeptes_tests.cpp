#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest>

#include "backend.h"
#include "diff.h"
#include "filepicker.h"
#include "highlighter.h"
#include "jsonmodel.h"
#include "tablemodel.h"
#include "theme.h"
#include "translator.h"

// Answers the open and save dialogs with a path the test picked.
class FakePicker : public FilePicker {
public:
    QString answer;
    QString askedName;
    void openFile(const QString &) override { emit openSelected(QUrl::fromLocalFile(answer)); }
    void saveFile(const QString &, const QString &name) override {
        askedName = name;
        emit saveSelected(QUrl::fromLocalFile(answer));
    }
    void exportFile(const QString &, const QString &name) override {
        askedName = name;
        emit exportSelected(QUrl::fromLocalFile(answer));
    }
};

class YeptesTests : public QObject {
    Q_OBJECT

private slots:
    void savesAndLoadsANote()
    {
        QTemporaryDir dir;
        Backend backend;
        backend.setText("yep\n");
        QVERIFY(backend.modified());
        QVERIFY(backend.saveTo(dir.filePath("a.ypts")));
        QVERIFY(!backend.modified());
        QCOMPARE(read(dir.filePath("a.ypts")), QByteArray("yep\n"));
        // Nothing is left beside the file by the atomic save.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);

        Backend other;
        QVERIFY(other.load(dir.filePath("a.ypts")));
        QCOMPARE(other.text(), QString("yep\n"));
        QCOMPARE(other.fileName(), QString("a.ypts"));
        QVERIFY(other.isNote());
        QVERIFY(!other.modified());
    }

    void keepsLineEndingsAndByteOrderMark()
    {
        QTemporaryDir dir;
        const QByteArray original = "\xEF\xBB\xBFone\r\ntwo\r\n";
        write(dir.filePath("win.txt"), original);

        Backend backend;
        QVERIFY(backend.load(dir.filePath("win.txt")));
        QCOMPARE(backend.text(), QString("one\ntwo\n"));
        QVERIFY(!backend.isNote());
        backend.setText("one\ntwo\nthree\n");
        QVERIFY(backend.saveTo(backend.filePath()));
        QCOMPARE(read(dir.filePath("win.txt")), QByteArray("\xEF\xBB\xBFone\r\ntwo\r\nthree\r\n"));
    }

    void refusesWhatItWouldDamage()
    {
        QTemporaryDir dir;
        write(dir.filePath("latin1.txt"), "caf\xE9\n");
        write(dir.filePath("binary.bin"), QByteArray("ab\0cd", 5));

        Backend backend;
        backend.setText("mine");
        QVERIFY(!backend.load(dir.filePath("latin1.txt")));
        QVERIFY(backend.messageIsError());
        QVERIFY(!backend.load(dir.filePath("binary.bin")));
        QVERIFY(!backend.load(dir.path()));
        // A refused file leaves the note that was open alone.
        QCOMPARE(backend.text(), QString("mine"));
    }

    void startsAFileThatIsNotThere()
    {
        QTemporaryDir dir;
        Backend backend;
        QVERIFY(backend.load(dir.filePath("new.md")));
        QCOMPARE(backend.fileName(), QString("new.md"));
        QVERIFY(!backend.modified());
        QVERIFY(!QFile::exists(dir.filePath("new.md")));
    }

    void reportsAFailedSave()
    {
        QTemporaryDir dir;
        Backend backend;
        backend.setText("text");
        QVERIFY(!backend.saveTo(dir.filePath("missing/folder/a.txt")));
        QVERIFY(backend.messageIsError());
        QVERIFY(backend.modified());
    }

    void savesANewNoteThroughThePicker()
    {
        QTemporaryDir dir;
        FakePicker picker;
        picker.answer = dir.filePath("picked.ypts");
        Backend backend(&picker);
        QSignalSpy saved(&backend, &Backend::saved);
        backend.setText("hello");
        backend.saveAs();
        QCOMPARE(picker.askedName, QString("untitled.ypts"));
        QCOMPARE(saved.count(), 1);
        QCOMPARE(read(picker.answer), QByteArray("hello"));
        QCOMPARE(backend.filePath(), picker.answer);

        picker.answer = dir.filePath("other.txt");
        write(picker.answer, "from disk");
        backend.open();
        QCOMPARE(backend.text(), QString("from disk"));
    }

    void savingANewEmptyNoteDoesNothing()
    {
        FakePicker picker;
        Backend backend(&picker);
        QSignalSpy saved(&backend, &Backend::saved);
        backend.setText("  \n ");
        backend.save();
        QCOMPARE(saved.count(), 0);
        QVERIFY(picker.askedName.isEmpty());
        QVERIFY(backend.filePath().isEmpty());
    }

    void exportFormatsFitTheText()
    {
        Backend backend;
        backend.setText("just some words");
        QCOMPARE(backend.fittingFormats(), (QStringList{".ypts", ".txt"}));
        backend.setText("{\"a\": [1, 2]}");
        QVERIFY(backend.fittingFormats().contains(".json"));
        QVERIFY(!backend.fittingFormats().contains(".csv"));
        backend.setText("name,age\nAda,36\nGrace,45");
        QVERIFY(backend.fittingFormats().contains(".csv"));
        QVERIFY(!backend.fittingFormats().contains(".json"));
        QVERIFY(!backend.fittingFormats().contains(".tsv"));
        backend.setText("a\tb\n1\t2");
        QVERIFY(backend.fittingFormats().contains(".tsv"));
        backend.setText("# Title\n\n- one\n- two");
        QVERIFY(backend.fittingFormats().contains(".md"));
        backend.setText("<p>Hello</p>");
        QVERIFY(backend.fittingFormats().contains(".html"));
        QVERIFY(!backend.fittingFormats().contains(".md"));
    }

    void documentIsTheTextOnceAttached()
    {
        QTemporaryDir dir;
        // A non-breaking space must survive the editor's document.
        write(dir.filePath("a.txt"), "a\xC2\xA0""b\nsecond\n");
        Backend backend;
        QVERIFY(backend.load(dir.filePath("a.txt")));
        QTextDocument document;
        backend.attachTextDocument(&document);
        QVERIFY(!backend.modified());
        QCOMPARE(backend.text(), QString::fromUtf8("a\xC2\xA0""b\nsecond\n"));

        backend.replaceRange(4, 10, "third");
        QCOMPARE(backend.text(), QString::fromUtf8("a\xC2\xA0""b\nthird\n"));
        QVERIFY(backend.modified());
        QCOMPARE(backend.cursorInfo(6).value("line").toInt(), 2);
        QCOMPARE(backend.cursorInfo(6).value("column").toInt(), 3);
        document.undo();
        QVERIFY(!backend.modified());
    }

    void diffFindsTheChangedLines()
    {
        const Diff::Result change = Diff::compare("one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\n",
                                                  "one\ntwo\nTHREE\nfour\nfive\nsix\nseven\neight\nnine\n", 1);
        QCOMPARE(change.added, 2);
        QCOMPARE(change.removed, 1);
        QCOMPARE(change.hunks.size(), 2);
        QCOMPARE(change.hunks[0].lines.size(), 4);
        QCOMPARE(change.hunks[0].lines[1].kind, '-');
        QCOMPARE(change.hunks[0].lines[1].text, QString("three"));
        QCOMPARE(change.hunks[0].lines[1].oldNumber, 3);
        QCOMPARE(change.hunks[0].lines[2].kind, '+');
        QCOMPARE(change.hunks[0].lines[2].newNumber, 3);
        QCOMPARE(change.hunks[1].lines.last().text, QString("nine"));
        QCOMPARE(change.hunks[1].lines.last().newNumber, 9);

        // What is stored in a note reads back as the same change.
        const Diff::Result stored = Diff::fromJson(Diff::toJson(change));
        QCOMPARE(stored.added, 2);
        QCOMPARE(stored.hunks[0].lines[2].newNumber, 3);
        QCOMPARE(stored.hunks[1].lines.last().newNumber, 9);

        QCOMPARE(Diff::compare("same\n", "same\n").hunks.size(), 0);
        QCOMPARE(Diff::compare("", "a\nb\n").added, 2);
        QCOMPARE(Diff::compare("a\nb\nc\n", "c\na\nb\n").added, 1);
    }

    void noteKeepsEverySavedChange()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("n.ypts");
        Backend backend;
        backend.setText("first\nsecond\n");
        QVERIFY(backend.saveTo(path));
        // A new note's first save is just its text.
        QCOMPARE(read(path), QByteArray("first\nsecond\n"));
        QCOMPARE(backend.historyCount(), 0);

        backend.setText("first\nSECOND\n");
        QCOMPARE(backend.diffRows().first().toMap().value("unsaved").toBool(), true);
        QVERIFY(backend.saveTo(path));
        backend.setText("first\nSECOND\nthird\n");
        QVERIFY(backend.saveTo(path));
        QCOMPARE(backend.historyCount(), 2);
        QVERIFY(read(path).startsWith("first\nSECOND\nthird\n\n\x1eyeptes history"));

        Backend other;
        QVERIFY(other.load(path));
        QCOMPARE(other.text(), QString("first\nSECOND\nthird\n"));
        QCOMPARE(other.historyCount(), 2);
        QVERIFY(!other.modified());
        const QVariantList rows = other.diffRows();
        // Newest first: the added third line, then the changed second line.
        QCOMPARE(rows[0].toMap().value("kind").toString(), QString("entry"));
        QCOMPARE(rows[0].toMap().value("added").toInt(), 1);
        QCOMPARE(rows[0].toMap().value("removed").toInt(), 0);
        QVERIFY(QDateTime::fromString(rows[0].toMap().value("at").toString(), Qt::ISODate).isValid());
        QCOMPARE(rows.last().toMap().value("text").toString(), QString("SECOND"));
        int removed = 0;
        for (const QVariant &row : rows)
            removed += row.toMap().value("kind").toString() == "remove";
        QCOMPARE(removed, 1);

        // Saving without a change adds nothing, and the history survives.
        QVERIFY(other.saveTo(path));
        QCOMPARE(other.historyCount(), 2);
        // Saved as another kind of file, the history stays behind.
        QVERIFY(other.saveTo(dir.filePath("n.txt")));
        QCOMPARE(read(dir.filePath("n.txt")), QByteArray("first\nSECOND\nthird\n"));
        QCOMPARE(other.historyCount(), 0);
    }

    void historyMarkerInANoteIsJustText()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("m.ypts");
        const QString tricky = "before\n" + Backend::historyMarker() + "\nafter";
        Backend backend;
        backend.setText(tricky);
        QVERIFY(backend.saveTo(path));
        Backend other;
        QVERIFY(other.load(path));
        QCOMPARE(other.text(), tricky);
        QCOMPARE(other.historyCount(), 0);
    }

    void exportLeavesTheNoteAlone()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("n.ypts");
        FakePicker picker;
        Backend backend(&picker);
        backend.setText("a\n");
        QVERIFY(backend.saveTo(path));
        backend.setText("b\n");
        QVERIFY(backend.saveTo(path));

        picker.answer = dir.filePath("copy.txt");
        backend.exportAs();
        QCOMPARE(picker.askedName, QString("n.txt"));
        QCOMPARE(read(picker.answer), QByteArray("b\n"));
        QCOMPARE(backend.filePath(), path);
        QCOMPARE(backend.historyCount(), 1);
        // Exporting over the note itself would throw its history away.
        QVERIFY(!backend.exportTo(path));
        QVERIFY(backend.messageIsError());
        QVERIFY(read(path).contains("yeptes history"));
    }

    void checklistBoxesTick()
    {
        Backend backend;
        backend.setText("todo\n- [ ] milk\n  * [x] eggs\n- [] not a box\n1. [ ] one\n");
        QCOMPARE(backend.checkboxAt(0), -1);
        QCOMPARE(backend.checkboxAt(8), 7);
        QCOMPARE(backend.checkboxAt(12), -1);
        QVERIFY(backend.toggleCheckbox(8));
        QCOMPARE(backend.text(), QString("todo\n- [x] milk\n  * [x] eggs\n- [] not a box\n1. [ ] one\n"));
        QVERIFY(backend.toggleCheckbox(22));
        QVERIFY(backend.text().contains("  * [ ] eggs"));
        QVERIFY(!backend.toggleCheckbox(35));
        QVERIFY(backend.toggleCheckbox(backend.text().indexOf("[ ] one") + 1));
        QVERIFY(backend.text().endsWith("1. [x] one\n"));
    }

    void tableReadsDelimitedText()
    {
        TableModel model;
        const QString csv = "name,note,n\n\"Ana, B\",\"said \"\"hi\"\"\",1\nshort\n";
        model.setText(csv, "csv");
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.columnCount(), 3);
        QCOMPARE(model.cellText(1, 0), QString("Ana, B"));
        QCOMPARE(model.cellText(1, 1), QString("said \"hi\""));
        QCOMPARE(model.cellStart(2, 1), -1);
        QCOMPARE(model.columnChars(1), 9);

        // An edit rewrites only that cell, quoting it when it needs it.
        Backend backend;
        backend.setText(csv);
        backend.replaceRange(model.cellStart(1, 2), model.cellEnd(1, 2), model.field("2, or 3"));
        QCOMPARE(backend.text(), QString("name,note,n\n\"Ana, B\",\"said \"\"hi\"\"\",\"2, or 3\"\nshort\n"));
        QCOMPARE(model.field("plain"), QString("plain"));

        model.setText("a\tb \"q\"\tc\n1\t2\t3\n", "tsv");
        QCOMPARE(model.columnCount(), 3);
        QCOMPARE(model.cellText(0, 1), QString("b \"q\""));
        QCOMPARE(model.field("x\ty"), QString("x y"));
        // Without a telling extension, the first line decides.
        QCOMPARE(TableModel::sniffDelimiter("a;b;c\n1,5;2;3\n", "txt"), QChar(';'));
        model.setText("", "csv");
        QVERIFY(model.empty());
    }

    void htmlViewDropsScripts()
    {
        QCOMPARE(Backend::stripScripts("<p>a</p><SCRIPT src=x>alert(1)\n</script ><p>b</p>"), QString("<p>a</p><p>b</p>"));
        QCOMPARE(Backend::stripScripts("<p>a</p><script>never closed"), QString("<p>a</p>"));
    }

    void jsonTreeFoldsAndKeepsItsFolds()
    {
        JsonModel model;
        model.setText(R"({"name": "Yeptes", "tags": ["a", "b"], "deep": {"x": null}})");
        QVERIFY(model.valid());
        QCOMPARE(model.rowCount(), 7);
        QCOMPARE(value(model, 1, "key"), QVariant("name"));
        QCOMPARE(value(model, 1, "value"), QVariant("\"Yeptes\""));
        QCOMPARE(value(model, 2, "count"), QVariant(2));
        QCOMPARE(model.parentRow(3), 2);

        model.toggle(2);
        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(value(model, 2, "collapsed"), QVariant(true));
        // An edit reparses the text; the fold is still there afterwards.
        model.setText(R"({"name": "Yep", "tags": ["a", "b"], "deep": {"x": null}})");
        QCOMPARE(model.rowCount(), 5);
        model.toggle(2);
        QCOMPARE(model.rowCount(), 7);
        model.collapseAll();
        QCOMPARE(model.rowCount(), 4);
        model.expandAll();
        QCOMPARE(model.rowCount(), 7);
    }

    void jsonEditTouchesOnlyTheValue()
    {
        const QString text = "{\n  \"zebra\" : 1,\n\n  \"apple\":   \"old\"\n}\n";
        JsonModel model;
        model.setText(text);
        QCOMPARE(value(model, 2, "key"), QVariant("apple"));
        Backend backend;
        backend.setText(text);
        backend.replaceRange(value(model, 2, "start").toInt(), value(model, 2, "end").toInt(), model.literal("new \"one\""));
        // Key order and spacing are exactly as written.
        QCOMPARE(backend.text(), QString("{\n  \"zebra\" : 1,\n\n  \"apple\":   \"new \\\"one\\\"\"\n}\n"));

        QCOMPARE(model.literal(" 42 "), QString("42"));
        QCOMPARE(model.literal("true"), QString("true"));
        QCOMPARE(model.literal("\"kept\""), QString("\"kept\""));
        QCOMPARE(model.literal("[1, 2]"), QString("[1, 2]"));
        QCOMPARE(model.literal("plain words"), QString("\"plain words\""));
        QCOMPARE(model.literal(""), QString("\"\""));
    }

    void jsonErrorsNameTheirPlace()
    {
        JsonModel model;
        model.setText("{\n  \"a\": [1, 2,]\n}");
        QVERIFY(!model.valid());
        QCOMPARE(model.rowCount(), 0);
        QVERIFY2(model.error().startsWith("Line 2, column 14"), qPrintable(model.error()));
        model.setText("{\"a\": 1} trailing");
        QVERIFY(!model.valid());
        model.setText("");
        QVERIFY(!model.valid());
    }

    void fencesLoadTheirLanguageOnFirstUse()
    {
        QTextDocument document;
        Highlighter highlighter(&document, SYNTAX_DIR);
        // A document reports its edits to the highlighter once it has a
        // layout, as it does in the editor, and the first pass is queued.
        document.documentLayout();
        QTest::qWait(10);
        document.setPlainText("just a note\nconst plain = 1\n");
        QVERIFY(highlighter.loadedLanguages().isEmpty());
        QVERIFY(!colorAt(document, 1, 0).isValid());

        document.setPlainText("note\n``` json\n{\"key\": \"text\", \"n\": 12, \"b\": true}\n```\nconst after = 1\n");
        QCOMPARE(highlighter.loadedLanguages(), QStringList{"json"});
        QCOMPARE(colorAt(document, 2, 2), highlighter.color(Highlighter::Property));
        QCOMPARE(colorAt(document, 2, 9), highlighter.color(Highlighter::String));
        QCOMPARE(colorAt(document, 2, 22), highlighter.color(Highlighter::Number));
        QCOMPARE(colorAt(document, 2, 31), highlighter.color(Highlighter::Literal));
        // The fence ends; the line after it is plain again.
        QVERIFY(!colorAt(document, 4, 0).isValid());

        // Other files don't get colors, even with a fence in them.
        highlighter.setFencesEnabled(false);
        QVERIFY(!colorAt(document, 2, 2).isValid());

        // Checklist boxes are marked everywhere, since they can be clicked.
        document.setPlainText("- [ ] open\n- [x] done\n");
        QCOMPARE(colorAt(document, 0, 3), highlighter.color(Highlighter::Keyword));
        QVERIFY(!colorAt(document, 0, 7).isValid());
        QCOMPARE(colorAt(document, 1, 7), highlighter.color(Highlighter::Fence));
    }

    void fencesFollowTheirLanguage()
    {
        QTextDocument document;
        Highlighter highlighter(&document, SYNTAX_DIR);
        // A document reports its edits to the highlighter once it has a
        // layout, as it does in the editor, and the first pass is queued.
        document.documentLayout();
        QTest::qWait(10);
        highlighter.setAccent(QColor("#112233"));
        document.setPlainText("```js\nconst a = call(1) /* open\nstill */ let b\n```\n"
                              "``` C++\n#include <x>\nint main() { return 0; } // done\n```\n"
                              "```html\n<a href=\"x\">t</a>\n```\n"
                              "```md\n# Title\n```\n"
                              "```unknownlang\nconst a\n```\n");
        QCOMPARE(highlighter.loadedLanguages(), (QStringList{"cpp", "html", "javascript", "markdown"}));
        QCOMPARE(colorAt(document, 1, 0), QColor("#112233"));
        QCOMPARE(colorAt(document, 1, 10), highlighter.color(Highlighter::Function));
        QCOMPARE(colorAt(document, 1, 20), highlighter.color(Highlighter::Comment));
        // The comment carries over the line break, then the code resumes.
        QCOMPARE(colorAt(document, 2, 0), highlighter.color(Highlighter::Comment));
        QCOMPARE(colorAt(document, 2, 9), highlighter.color(Highlighter::Keyword));
        QCOMPARE(colorAt(document, 5, 0), highlighter.color(Highlighter::Property));
        QCOMPARE(colorAt(document, 6, 0), highlighter.color(Highlighter::Type));
        QCOMPARE(colorAt(document, 6, 13), highlighter.color(Highlighter::Keyword));
        QCOMPARE(colorAt(document, 6, 27), highlighter.color(Highlighter::Comment));
        QCOMPARE(colorAt(document, 9, 1), highlighter.color(Highlighter::Tag));
        QCOMPARE(colorAt(document, 9, 3), highlighter.color(Highlighter::Property));
        QCOMPARE(colorAt(document, 9, 9), highlighter.color(Highlighter::String));
        QCOMPARE(colorAt(document, 12, 2), highlighter.color(Highlighter::Heading));
        QVERIFY(!colorAt(document, 15, 0).isValid());
    }

    void codeViewColorsTheWholeText()
    {
        QTextDocument document;
        Highlighter highlighter(&document, SYNTAX_DIR);
        document.documentLayout();
        QTest::qWait(10);
        document.setPlainText("def f(): # note\n    return \"\"\"a\nb\"\"\"\n");
        QVERIFY(!colorAt(document, 0, 0).isValid());

        highlighter.setDocumentLanguage("python");
        QCOMPARE(colorAt(document, 0, 0), highlighter.color(Highlighter::Keyword));
        QCOMPARE(colorAt(document, 0, 4), highlighter.color(Highlighter::Function));
        QCOMPARE(colorAt(document, 0, 10), highlighter.color(Highlighter::Comment));
        // A string carries over the line break.
        QCOMPARE(colorAt(document, 2, 0), highlighter.color(Highlighter::String));

        highlighter.setDocumentLanguage(QString());
        QVERIFY(!colorAt(document, 0, 0).isValid());
        QCOMPARE(Highlighter::displayName("cpp"), QString("C++"));
        QVERIFY(Highlighter::availableLanguages(SYNTAX_DIR).contains("python"));
    }

    void codeLanguageFollowsTheFile()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.py"), "def f(): pass\n");
        write(dir.filePath("b.txt"), "plain\n");
        Backend backend;
        backend.setSyntaxDirectory(SYNTAX_DIR);
        QTextDocument document;
        document.documentLayout();
        backend.attachTextDocument(&document);
        QVERIFY(backend.load(dir.filePath("a.py")));
        QCOMPARE(backend.codeLanguage(), QString("python"));
        QCOMPARE(backend.codeLanguageName(), QString("Python"));
        QVERIFY(!backend.codeLanguages().isEmpty());

        // Only the Code view colors the whole text.
        QCOMPARE(backend.highlighter()->documentLanguage(), QString());
        backend.setCodeView(true);
        QCOMPARE(backend.highlighter()->documentLanguage(), QString("python"));
        backend.setCodeLanguage("rust");
        QCOMPARE(backend.highlighter()->documentLanguage(), QString("rust"));

        // An extension without rules gives plain text; the pick still works.
        QVERIFY(backend.load(dir.filePath("b.txt")));
        QCOMPARE(backend.codeLanguage(), QString());
        QCOMPARE(backend.highlighter()->documentLanguage(), QString());
        backend.setCodeLanguage("json");
        QCOMPARE(backend.highlighter()->documentLanguage(), QString("json"));

        // Saving under a name without rules keeps the picked language.
        QVERIFY(backend.saveTo(dir.filePath("c.ypts")));
        QCOMPARE(backend.codeLanguage(), QString("json"));
        QVERIFY(backend.saveTo(dir.filePath("d.go")));
        QCOMPARE(backend.codeLanguage(), QString("go"));
        backend.setCodeView(false);
        QCOMPARE(backend.highlighter()->documentLanguage(), QString());
    }

    void everyLanguageHasRules()
    {
        QTextDocument document;
        Highlighter highlighter(&document, SYNTAX_DIR);
        const QStringList files = QDir(SYNTAX_DIR).entryList({"*.json"});
        QVERIFY(files.size() >= 20);
        for (const QString &file : files)
            QVERIFY2(highlighter.language(QFileInfo(file).baseName()), qPrintable(file));
        for (const char *alias : {"js", "ts", "py", "c++", "c#", "sh", "md", "yml", "JSON", " Java "})
            QVERIFY2(highlighter.language(Highlighter::canonicalName(alias)), alias);
        QVERIFY(Highlighter::fenceLanguage("not a fence").isNull());
        QCOMPARE(Highlighter::fenceLanguage("``` json"), QString("json"));
        QVERIFY(Highlighter::fenceLanguage("```").isEmpty() && !Highlighter::fenceLanguage("```").isNull());
    }

    void catalogsAreCompleteJson()
    {
        const QDir dir(QString(SYNTAX_DIR) + "/../i18n");
        const QJsonObject danish = QJsonDocument::fromJson(read(dir.filePath("da.json"))).object();
        QVERIFY(danish.size() > 40);
        for (const QString &file : dir.entryList({"*.json"})) {
            const QJsonObject catalog = QJsonDocument::fromJson(read(dir.filePath(file))).object();
            QVERIFY2(!catalog.isEmpty(), qPrintable(file));
            if (file != "en.json")
                QCOMPARE(catalog.keys(), danish.keys());
        }
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    static QVariant value(const JsonModel &model, int row, const char *role)
    {
        return model.data(model.index(row), model.roleNames().key(role));
    }

    // The color the highlighter gave a character, invalid when it left it plain.
    static QColor colorAt(const QTextDocument &document, int line, int column)
    {
        QColor color;
        const QTextBlock block = document.findBlockByNumber(line);
        for (const auto &range : block.layout()->formats())
            if (column >= range.start && column < range.start + range.length
                && range.format.hasProperty(QTextFormat::ForegroundBrush))
                color = range.format.foreground().color();
        return color;
    }
};

QTEST_MAIN(YeptesTests)
#include "yeptes_tests.moc"
