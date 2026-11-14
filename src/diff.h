#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// A line-by-line comparison of two versions of a note, as the hunks a reader
// would want to see: the changed lines with a little context around them.
namespace Diff {
struct Line {
    char kind = ' ';  // ' ' unchanged, '+' added, '-' removed
    QString text;
    int oldNumber = 0, newNumber = 0;
};

struct Hunk {
    QList<Line> lines;
};

struct Result {
    int added = 0, removed = 0;
    QList<Hunk> hunks;
};

Result compare(const QString &before, const QString &after, int context = 3);

// The form a change is kept in inside a .ypts file.
QJsonObject toJson(const Result &result);
Result fromJson(const QJsonObject &object);
}
