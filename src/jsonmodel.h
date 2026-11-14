#pragma once

#include <QAbstractListModel>
#include <QSet>

// A parsed JSON value that remembers where it sits in the text, so editing a
// value replaces only its own characters and leaves the rest of the file
// (key order, spacing, comments in other places) exactly as written.
struct JsonNode {
    enum Kind { Object, Array, String, Number, Boolean, Null };
    Kind kind = Null;
    QString key;
    int start = 0, end = 0;
    QList<JsonNode> children;
};

// The JSON view: the document as a flat list of visible rows, with objects
// and arrays that fold. The fold state is kept by path, so it survives the
// reparse that follows every edit.
class JsonModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    enum Roles { DepthRole = Qt::UserRole + 1, KeyRole, IndexedRole, ValueRole, KindRole,
                 ContainerRole, CollapsedRole, CountRole, StartRole, EndRole };

    explicit JsonModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    bool valid() const { return m_valid; }
    QString error() const { return m_error; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setText(const QString &text);
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE int parentRow(int row) const;
    // What to write into the file for what the user typed: the input itself
    // when it is valid JSON (42, true, "text", [1, 2]), otherwise the input
    // as a quoted string.
    Q_INVOKABLE QString literal(const QString &input) const;

    // Parses text into root; on failure returns false with a message that
    // names the line and column.
    static bool parse(const QString &text, JsonNode *root, QString *error);

signals:
    void changed();

private:
    struct Row {
        const JsonNode *node = nullptr;
        int depth = 0;
        QString key, path;
        bool indexed = false, collapsed = false;
    };

    void append(QList<Row> &rows, const JsonNode &node, int depth, const QString &path,
                const QString &key, bool indexed) const;
    void appendChildren(QList<Row> &rows, const Row &parent) const;
    void rebuild();

    QString m_text, m_error;
    bool m_valid = false;
    JsonNode m_root;
    QList<Row> m_rows;
    QSet<QString> m_collapsed;
};
