#include "diff.h"

#include <QJsonArray>

namespace {
struct Op {
    char kind;
    int a, b;
};

QList<QStringView> linesOf(const QString &text) {
    QList<QStringView> lines = QStringView(text).split(u'\n');
    // A final line break ends the last line; it doesn't start an empty one.
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return lines;
}

// Myers' shortest edit script over a[0..n) and b[0..m), appended to ops with
// the given offsets. Past maxD edits it gives up on being minimal and reports
// everything removed and added, which is still a correct diff.
void myers(const QList<QStringView> &a, int aFrom, int n, const QList<QStringView> &b, int bFrom, int m,
           QList<Op> &ops) {
    const int maxD = qMin(n + m, 2000);
    const int off = maxD + 1;
    QList<int> v(2 * maxD + 3, 0);
    QList<QList<int>> trace;
    int found = -1;
    for (int d = 0; d <= maxD && found < 0; ++d) {
        trace.append(v.mid(off - d, 2 * d + 1));
        for (int k = -d; k <= d; k += 2) {
            int x = (k == -d || (k != d && v[off + k - 1] < v[off + k + 1])) ? v[off + k + 1] : v[off + k - 1] + 1;
            int y = x - k;
            while (x < n && y < m && a[aFrom + x] == b[bFrom + y]) {
                ++x;
                ++y;
            }
            v[off + k] = x;
            if (x >= n && y >= m) {
                found = d;
                break;
            }
        }
    }
    if (found < 0) {
        for (int i = 0; i < n; ++i)
            ops.append({'-', aFrom + i, -1});
        for (int i = 0; i < m; ++i)
            ops.append({'+', -1, bFrom + i});
        return;
    }

    QList<Op> reversed;
    int x = n, y = m;
    for (int d = found; d > 0; --d) {
        const QList<int> &before = trace[d];
        auto at = [&](int k) { return before[k + d]; };
        const int k = x - y;
        const int previousK = (k == -d || (k != d && at(k - 1) < at(k + 1))) ? k + 1 : k - 1;
        const int previousX = at(previousK), previousY = previousX - previousK;
        while (x > previousX && y > previousY) {
            reversed.append({' ', aFrom + x - 1, bFrom + y - 1});
            --x;
            --y;
        }
        if (x == previousX)
            reversed.append({'+', -1, bFrom + y - 1});
        else
            reversed.append({'-', aFrom + x - 1, -1});
        x = previousX;
        y = previousY;
    }
    while (x > 0 && y > 0) {
        reversed.append({' ', aFrom + x - 1, bFrom + y - 1});
        --x;
        --y;
    }
    for (int i = reversed.size() - 1; i >= 0; --i)
        ops.append(reversed[i]);
}
}

Diff::Result Diff::compare(const QString &before, const QString &after, int context) {
    const QList<QStringView> a = linesOf(before), b = linesOf(after);
    const int n = a.size(), m = b.size();

    // Most edits touch a small part of a note, so the shared start and end
    // are set aside before the real comparison.
    int prefix = 0;
    while (prefix < n && prefix < m && a[prefix] == b[prefix])
        ++prefix;
    int suffix = 0;
    while (suffix < n - prefix && suffix < m - prefix && a[n - 1 - suffix] == b[m - 1 - suffix])
        ++suffix;

    QList<Op> ops;
    for (int i = 0; i < prefix; ++i)
        ops.append({' ', i, i});
    myers(a, prefix, n - prefix - suffix, b, prefix, m - prefix - suffix, ops);
    for (int i = 0; i < suffix; ++i)
        ops.append({' ', n - suffix + i, m - suffix + i});

    // Keep each change and the lines within reach of it; a run of kept lines
    // is one hunk.
    QList<bool> keep(ops.size(), false);
    for (int i = 0; i < ops.size(); ++i) {
        if (ops[i].kind == ' ')
            continue;
        for (int j = qMax(0, i - context); j <= qMin(int(ops.size()) - 1, i + context); ++j)
            keep[j] = true;
    }

    Result result;
    int oldNumber = 0, newNumber = 0;
    bool open = false;
    for (int i = 0; i < ops.size(); ++i) {
        const Op &op = ops[i];
        if (op.kind != '+')
            ++oldNumber;
        if (op.kind != '-')
            ++newNumber;
        if (!keep[i]) {
            open = false;
            continue;
        }
        if (!open) {
            result.hunks.append(Hunk());
            open = true;
        }
        Line line;
        line.kind = op.kind;
        line.text = (op.kind == '+' ? b[op.b] : a[op.a]).toString();
        line.oldNumber = op.kind == '+' ? 0 : oldNumber;
        line.newNumber = op.kind == '-' ? 0 : newNumber;
        result.hunks.last().lines.append(line);
        result.added += op.kind == '+';
        result.removed += op.kind == '-';
    }
    return result;
}

// {"added": 1, "removed": 1, "hunks": [{"old": 4, "new": 4, "lines": [" same", "-was", "+is"]}]}
// where old and new are the line numbers the hunk starts at.
QJsonObject Diff::toJson(const Result &result) {
    QJsonArray hunks;
    for (const Hunk &hunk : result.hunks) {
        QJsonArray lines;
        int oldStart = 0, newStart = 0;
        for (const Line &line : hunk.lines) {
            if (!oldStart && line.oldNumber)
                oldStart = line.oldNumber;
            if (!newStart && line.newNumber)
                newStart = line.newNumber;
            lines.append(QChar(line.kind) + line.text);
        }
        hunks.append(QJsonObject{{"old", oldStart}, {"new", newStart}, {"lines", lines}});
    }
    return {{"added", result.added}, {"removed", result.removed}, {"hunks", hunks}};
}

Diff::Result Diff::fromJson(const QJsonObject &object) {
    Result result;
    for (const auto &value : object.value("hunks").toArray()) {
        const QJsonObject json = value.toObject();
        Hunk hunk;
        int oldNumber = json.value("old").toInt(), newNumber = json.value("new").toInt();
        for (const auto &entry : json.value("lines").toArray()) {
            const QString text = entry.toString();
            if (text.isEmpty())
                continue;
            Line line;
            const QChar kind = text.at(0);
            line.kind = kind == '+' ? '+' : kind == '-' ? '-' : ' ';
            line.text = text.mid(1);
            if (line.kind != '+')
                line.oldNumber = oldNumber++;
            if (line.kind != '-')
                line.newNumber = newNumber++;
            result.added += line.kind == '+';
            result.removed += line.kind == '-';
            hunk.lines.append(line);
        }
        if (!hunk.lines.isEmpty())
            result.hunks.append(hunk);
    }
    return result;
}
