#pragma once

#include <QAbstractTableModel>

// The Table view: delimited text (CSV, TSV) as rows of cells. Each cell
// remembers where it sits in the text, so editing one replaces only its own
// characters and leaves the rest of the file as written.
class TableModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(bool empty READ empty NOTIFY changed)

public:
    explicit TableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    bool empty() const { return m_rows.isEmpty(); }
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // suffix picks the separator ("tsv" is tabs, "csv" commas); for other
    // files it is whichever of tab, comma, or semicolon the first line uses most.
    Q_INVOKABLE void setText(const QString &text, const QString &suffix);
    // Where a cell's characters are in the text, or -1 when the row is too
    // short to have that cell.
    Q_INVOKABLE int cellStart(int row, int column) const;
    Q_INVOKABLE int cellEnd(int row, int column) const;
    Q_INVOKABLE QString cellText(int row, int column) const;
    // The widest value in a column, in characters, for sizing it.
    Q_INVOKABLE int columnChars(int column) const;
    // What to write into the file for what the user typed, quoted if needed.
    Q_INVOKABLE QString field(const QString &input) const;

    QChar delimiter() const { return m_delimiter; }
    static QChar sniffDelimiter(const QString &text, const QString &suffix);

signals:
    void changed();

private:
    struct Cell {
        QString value;
        int start = 0, end = 0;
    };

    QList<QList<Cell>> m_rows;
    QList<int> m_widths;
    QChar m_delimiter = ',';
};
