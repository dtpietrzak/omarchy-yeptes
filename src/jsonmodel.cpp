#include "jsonmodel.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace {
// Separates the keys of a path; a character JSON keys can't contain raw.
const QChar pathSeparator(0x1f);

class Parser {
public:
    explicit Parser(const QString &text) : t(text), n(text.size()) {}

    bool document(JsonNode *root) {
        skipSpace();
        if (p >= n)
            return fail(QCoreApplication::translate("JsonModel", "There is nothing here yet"));
        if (!value(root, 0))
            return false;
        skipSpace();
        if (p < n)
            return fail(QCoreApplication::translate("JsonModel", "Unexpected text after the end of the document"));
        return true;
    }

    QString message;
    int position = 0;

private:
    bool fail(const QString &text) {
        // Only the first problem is reported; it is the one to fix.
        if (message.isEmpty()) {
            message = text;
            position = qMin(p, n);
        }
        return false;
    }

    void skipSpace() {
        while (p < n && (t[p] == ' ' || t[p] == '\n' || t[p] == '\t' || t[p] == '\r'))
            ++p;
    }

    bool string() {
        ++p;
        while (p < n) {
            const QChar c = t[p];
            if (c == '"') {
                ++p;
                return true;
            }
            if (c.unicode() < 0x20)
                break;
            p += c == '\\' ? 2 : 1;
        }
        return fail(QCoreApplication::translate("JsonModel", "A string is missing its closing quote"));
    }

    bool value(JsonNode *node, int depth) {
        if (depth > 400)
            return fail(QCoreApplication::translate("JsonModel", "The document is nested too deeply"));
        if (p >= n)
            return fail(QCoreApplication::translate("JsonModel", "The document ends where a value should be"));
        node->start = p;
        const QChar c = t[p];
        if (c == '{' || c == '[') {
            const bool object = c == '{';
            const QChar close = object ? '}' : ']';
            node->kind = object ? JsonNode::Object : JsonNode::Array;
            ++p;
            skipSpace();
            if (p < n && t[p] == close) {
                node->end = ++p;
                return true;
            }
            while (true) {
                JsonNode child;
                skipSpace();
                if (object) {
                    if (p >= n || t[p] != '"')
                        return fail(QCoreApplication::translate("JsonModel", "Expected a key in double quotes"));
                    const int keyStart = p + 1;
                    if (!string())
                        return false;
                    child.key = t.mid(keyStart, p - 1 - keyStart);
                    skipSpace();
                    if (p >= n || t[p] != ':')
                        return fail(QCoreApplication::translate("JsonModel", "Expected a colon after the key"));
                    ++p;
                    skipSpace();
                }
                if (!value(&child, depth + 1))
                    return false;
                node->children.append(child);
                skipSpace();
                if (p < n && t[p] == ',') {
                    ++p;
                    continue;
                }
                if (p < n && t[p] == close) {
                    node->end = ++p;
                    return true;
                }
                return fail(object ? QCoreApplication::translate("JsonModel", "Expected a comma or a closing brace")
                                   : QCoreApplication::translate("JsonModel", "Expected a comma or a closing bracket"));
            }
        }
        if (c == '"') {
            node->kind = JsonNode::String;
            if (!string())
                return false;
            node->end = p;
            return true;
        }
        for (const auto &[word, kind] : {std::pair{"true", JsonNode::Boolean}, std::pair{"false", JsonNode::Boolean},
                                         std::pair{"null", JsonNode::Null}}) {
            if (QStringView(t).mid(p).startsWith(QLatin1String(word))) {
                node->kind = kind;
                node->end = p += qstrlen(word);
                return true;
            }
        }
        static const QRegularExpression number(QStringLiteral("-?(0|[1-9]\\d*)(\\.\\d+)?([eE][+-]?\\d+)?"));
        const auto match = number.match(t, p, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
        if (match.hasMatch()) {
            node->kind = JsonNode::Number;
            node->end = p = match.capturedEnd();
            return true;
        }
        return fail(QCoreApplication::translate("JsonModel", "Expected a value"));
    }

    const QString &t;
    const int n;
    int p = 0;
};

const char *kindName(JsonNode::Kind kind) {
    switch (kind) {
    case JsonNode::Object: return "object";
    case JsonNode::Array: return "array";
    case JsonNode::String: return "string";
    case JsonNode::Number: return "number";
    case JsonNode::Boolean: return "boolean";
    default: return "null";
    }
}

bool isContainer(const JsonNode &node) {
    return node.kind == JsonNode::Object || node.kind == JsonNode::Array;
}
}

bool JsonModel::parse(const QString &text, JsonNode *root, QString *error) {
    Parser parser(text);
    JsonNode parsed;
    if (parser.document(&parsed)) {
        *root = parsed;
        return true;
    }
    if (error) {
        const QStringView before = QStringView(text).left(parser.position);
        const int line = before.count('\n') + 1;
        const int column = parser.position - before.lastIndexOf('\n');
        *error = tr("Line %1, column %2: %3").arg(line).arg(column).arg(parser.message);
    }
    return false;
}

int JsonModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> JsonModel::roleNames() const {
    return {{DepthRole, "depth"}, {KeyRole, "key"}, {IndexedRole, "indexed"}, {ValueRole, "value"},
            {KindRole, "kind"}, {ContainerRole, "container"}, {CollapsedRole, "collapsed"},
            {CountRole, "count"}, {StartRole, "start"}, {EndRole, "end"}};
}

QVariant JsonModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    const JsonNode &node = *row.node;
    switch (role) {
    case DepthRole: return row.depth;
    case KeyRole: return row.key;
    case IndexedRole: return row.indexed;
    // A scalar shows exactly as it is written in the file, quotes included.
    case ValueRole: return isContainer(node) ? QString() : m_text.mid(node.start, node.end - node.start);
    case KindRole: return QString::fromLatin1(kindName(node.kind));
    case ContainerRole: return isContainer(node);
    case CollapsedRole: return row.collapsed;
    case CountRole: return int(node.children.size());
    case StartRole: return node.start;
    case EndRole: return node.end;
    }
    return {};
}

void JsonModel::append(QList<Row> &rows, const JsonNode &node, int depth, const QString &path,
                       const QString &key, bool indexed) const {
    Row row;
    row.node = &node;
    row.depth = depth;
    row.key = key;
    row.path = path;
    row.indexed = indexed;
    row.collapsed = isContainer(node) && m_collapsed.contains(path);
    rows.append(row);
    if (!row.collapsed)
        appendChildren(rows, row);
}

void JsonModel::appendChildren(QList<Row> &rows, const Row &parent) const {
    const JsonNode &node = *parent.node;
    const bool array = node.kind == JsonNode::Array;
    for (int i = 0; i < node.children.size(); ++i) {
        const JsonNode &child = node.children[i];
        const QString key = array ? QString::number(i) : child.key;
        append(rows, child, parent.depth + 1, parent.path + pathSeparator + key, key, array);
    }
}

void JsonModel::rebuild() {
    beginResetModel();
    m_rows.clear();
    if (m_valid)
        append(m_rows, m_root, 0, QString(), QString(), false);
    endResetModel();
}

void JsonModel::setText(const QString &text) {
    m_text = text;
    m_root = JsonNode();
    m_error.clear();
    m_valid = parse(m_text, &m_root, &m_error);
    rebuild();
    emit changed();
}

void JsonModel::toggle(int row) {
    if (row < 0 || row >= m_rows.size() || !isContainer(*m_rows[row].node))
        return;
    if (!m_rows[row].collapsed) {
        m_collapsed.insert(m_rows[row].path);
        int last = row;
        while (last + 1 < m_rows.size() && m_rows[last + 1].depth > m_rows[row].depth)
            ++last;
        if (last > row) {
            beginRemoveRows({}, row + 1, last);
            m_rows.erase(m_rows.begin() + row + 1, m_rows.begin() + last + 1);
            endRemoveRows();
        }
        m_rows[row].collapsed = true;
    } else {
        m_collapsed.remove(m_rows[row].path);
        m_rows[row].collapsed = false;
        QList<Row> children;
        appendChildren(children, m_rows[row]);
        if (!children.isEmpty()) {
            beginInsertRows({}, row + 1, row + children.size());
            for (int i = 0; i < children.size(); ++i)
                m_rows.insert(row + 1 + i, children[i]);
            endInsertRows();
        }
    }
    emit dataChanged(index(row), index(row), {CollapsedRole});
}

void JsonModel::expandAll() {
    m_collapsed.clear();
    rebuild();
}

// Folds everything below the top level, which stays open so there is still
// something to look at.
void JsonModel::collapseAll() {
    if (!m_valid)
        return;
    m_collapsed.clear();
    QList<Row> all;
    append(all, m_root, 0, QString(), QString(), false);
    for (const Row &row : all)
        if (row.depth > 0 && isContainer(*row.node))
            m_collapsed.insert(row.path);
    rebuild();
}

int JsonModel::parentRow(int row) const {
    if (row < 0 || row >= m_rows.size())
        return -1;
    for (int i = row - 1; i >= 0; --i)
        if (m_rows[i].depth < m_rows[row].depth)
            return i;
    return -1;
}

QString JsonModel::literal(const QString &input) const {
    const QString trimmed = input.trimmed();
    JsonNode node;
    if (parse(trimmed, &node, nullptr))
        return trimmed;

    QString quoted = QStringLiteral("\"");
    for (const QChar c : input) {
        switch (c.unicode()) {
        case '"': quoted += QLatin1String("\\\""); break;
        case '\\': quoted += QLatin1String("\\\\"); break;
        case '\n': quoted += QLatin1String("\\n"); break;
        case '\r': quoted += QLatin1String("\\r"); break;
        case '\t': quoted += QLatin1String("\\t"); break;
        default:
            if (c.unicode() < 0x20)
                quoted += QStringLiteral("\\u%1").arg(int(c.unicode()), 4, 16, QLatin1Char('0'));
            else
                quoted += c;
        }
    }
    return quoted + '"';
}
