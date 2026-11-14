// The display-free commands follow Hype's (MIT, David Heinemeier Hansson).
#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "backend.h"
#include "highlighter.h"
#include "jsonmodel.h"

namespace {
QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

int fail(const QString &message) {
    QTextStream(stderr) << "yeptes: " << message << '\n';
    return 1;
}

QString usage() {
    return QStringLiteral(
        "Yeptes, a plain text notepad.\n"
        "\n"
        "Usage:\n"
        "  yeptes [open] [file]            Open the window, with a file if given\n"
        "  yeptes check <file> [--json]    Check that a file opens, that JSON parses,\n"
        "                                  and which fences a .ypts note will color\n"
        "  yeptes skill [install]          Print the skill for coding agents, or install it\n"
        "  yeptes help                     Show this help\n"
        "  yeptes --version                Show the version\n");
}

int check(QStringList arguments) {
    const bool json = arguments.removeAll("--json") > 0;
    if (arguments.size() != 1)
        return fail("Use yeptes check <file> [--json].");
    const QString path = QFileInfo(arguments.first()).absoluteFilePath();
    if (!QFileInfo::exists(path))
        return fail(QStringLiteral("%1 does not exist.").arg(arguments.first()));

    Backend::FileText file;
    QString error;
    if (!Backend::readFile(path, &file, &error))
        return fail(error);

    const QString suffix = QFileInfo(path).suffix().toLower();
    // A note's saved changes ride at the end of its file; they aren't its text.
    QStringList history;
    if (suffix == "ypts")
        Backend::splitHistory(QString(file.text), &file.text, &history);
    const QStringList lines = file.text.split('\n');
    QJsonObject report{{"file", path},
                       {"lines", int(lines.size())},
                       {"characters", int(file.text.size())},
                       {"lineEndings", file.crlf ? "crlf" : "lf"}};
    QStringList problems;

    if (suffix == "json") {
        JsonNode root;
        QString jsonError;
        const bool valid = JsonModel::parse(file.text, &root, &jsonError);
        report.insert("json", QJsonObject{{"valid", valid}, {"error", jsonError}});
        if (!valid)
            problems.append(jsonError);
    }

    // The fences of a note, with whether Yeptes has colors for each language.
    if (suffix == "ypts") {
        const QDir syntax(":/syntax");
        QJsonArray fences;
        bool inside = false;
        for (int i = 0; i < lines.size(); ++i) {
            if (inside) {
                inside = !Highlighter::isFenceEnd(lines[i]);
                continue;
            }
            const QString language = Highlighter::fenceLanguage(lines[i]);
            if (language.isNull())
                continue;
            inside = true;
            fences.append(QJsonObject{{"line", i + 1},
                                      {"language", language},
                                      {"colored", !language.isEmpty() && syntax.exists(language + ".json")}});
        }
        if (inside)
            problems.append(QStringLiteral("The last fence is never closed."));
        report.insert("fences", fences);
        report.insert("savedChanges", int(history.size()));
    }

    report.insert("ok", problems.isEmpty());
    if (json) {
        out() << QJsonDocument(report).toJson(QJsonDocument::Indented);
    } else {
        out() << path << ": " << lines.size() << " lines, " << file.text.size() << " characters\n";
        for (const auto &value : report.value("fences").toArray()) {
            const QJsonObject fence = value.toObject();
            out() << "  line " << fence.value("line").toInt() << ": "
                  << (fence.value("language").toString().isEmpty() ? QStringLiteral("(no language)")
                                                                   : fence.value("language").toString())
                  << (fence.value("colored").toBool() ? "" : " (not colored)") << '\n';
        }
    }
    out().flush();
    for (const QString &problem : problems)
        fail(problem);
    return problems.isEmpty() ? 0 : 1;
}

int skill(QStringList arguments) {
    const bool json = arguments.removeAll("--json") > 0;
    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail("The bundled skill is missing from this build.");
    const QByteArray contents = bundled.readAll();

    if (arguments.isEmpty()) {
        out() << contents;
        out().flush();
        return 0;
    }
    if (arguments != QStringList{"install"})
        return fail("Use yeptes skill to print the skill, or yeptes skill install.");

    // One copy under ~/.agents, linked into ~/.claude when that exists.
    QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/yeptes");
    if (!home.mkpath(".agents/skills/yeptes"))
        return fail(QStringLiteral("Could not create %1.").arg(directory));
    QFile target(directory + "/SKILL.md");
    if (!target.open(QIODevice::WriteOnly) || target.write(contents) != contents.size())
        return fail(QStringLiteral("Could not write %1: %2").arg(target.fileName(), target.errorString()));
    target.close();

    QString link;
    if (home.exists(".claude")) {
        link = home.filePath(".claude/skills/yeptes");
        if (!QFileInfo(link).isSymLink() && !QFileInfo::exists(link)) {
            if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/yeptes", link))
                return fail(QStringLiteral("Could not link %1.").arg(link));
        }
    }

    if (json)
        out() << QJsonDocument(QJsonObject{{"ok", true}, {"installed", directory}, {"linked", link}})
                     .toJson(QJsonDocument::Indented);
    else
        out() << "Installed the skill in " << directory << (link.isEmpty() ? "" : " and linked it from " + link) << '\n';
    out().flush();
    return 0;
}
}

bool Cli::handles(const QStringList &arguments) {
    static const QStringList commands{"check", "skill", "help", "--help", "-h", "--version", "-v"};
    return !arguments.isEmpty() && commands.contains(arguments.first());
}

int Cli::run(const QStringList &arguments) {
    const QString command = arguments.first();
    const QStringList rest = arguments.mid(1);
    if (command == "check")
        return check(rest);
    if (command == "skill")
        return skill(rest);
    if (command == "--version" || command == "-v")
        out() << "yeptes " << QCoreApplication::applicationVersion() << '\n';
    else
        out() << usage();
    out().flush();
    return 0;
}
