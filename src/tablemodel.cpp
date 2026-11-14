#include "tablemodel.h"

QChar TableModel::sniffDelimiter(const QString &text, const QString &suffix) {
    if (suffix == "tsv" || suffix == "tab")
        return '\t';
    if (suffix == "csv")
        return ',';
    const int lineEnd = text.indexOf('\n');
    const QStringView first = QStringView(text).left(lineEnd < 0 ? text.size() : lineEnd);
    QChar best = ',';
    int most = 0;
    for (const QChar candidate : {QChar('\t'), QChar(','), QChar(';')}) {
        const int count = first.count(candidate);
        if (count > most) {
            most = count;
            best = candidate;
        }
    }
    return best;
}

int TableModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int TableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_widths.size();
}

QVariant TableModel::data(const QModelIndex &index, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    return cellText(index.row(), index.column());
}

void TableModel::setText(const QString &text, const QString &suffix) {
    beginResetModel();
    m_rows.clear();
    m_widths.clear();
    m_delimiter = sniffDelimiter(text, suffix);
    // Tab-separated files have no quoting; a quote there is just a character.
    const bool quoting = m_delimiter != '\t';

    const int n = text.size();
    int i = 0;
    while (i < n) {
        QList<Cell> row;
        while (true) {
            Cell cell;
            cell.start = i;
            if (quoting && i < n && text[i] == '"') {
                ++i;
                while (i < n) {
                    if (text[i] == '"') {
                        if (i + 1 < n && text[i + 1] == '"') {
                            cell.value += '"';
                            i += 2;
                            continue;
                        }
                        ++i;
                        break;
                    }
                    cell.value += text[i++];
                }
                // Anything between the closing quote and the separator stays
                // part of the cell, as other readers treat it.
                while (i < n && text[i] != m_delimiter && text[i] != '\n')
                    cell.value += text[i++];
            } else {
                while (i < n && text[i] != m_delimiter && text[i] != '\n')
                    ++i;
                cell.value = text.mid(cell.start, i - cell.start);
            }
            cell.end = i;
            row.append(cell);
            if (i < n && text[i] == m_delimiter) {
                ++i;
                continue;
            }
            break;
        }
        ++i;  // Past the line break.
        m_rows.append(row);
        for (int column = 0; column < row.size(); ++column) {
            if (column >= m_widths.size())
                m_widths.append(0);
            m_widths[column] = qMax(m_widths[column], int(row[column].value.size()));
        }
    }
    endResetModel();
    emit changed();
}

int TableModel::cellStart(int row, int column) const {
    if (row < 0 || row >= m_rows.size() || column < 0 || column >= m_rows[row].size())
        return -1;
    return m_rows[row][column].start;
}

int TableModel::cellEnd(int row, int column) const {
    return cellStart(row, column) < 0 ? -1 : m_rows[row][column].end;
}

QString TableModel::cellText(int row, int column) const {
    return cellStart(row, column) < 0 ? QString() : m_rows[row][column].value;
}

int TableModel::columnChars(int column) const {
    return column >= 0 && column < m_widths.size() ? m_widths[column] : 0;
}

QString TableModel::field(const QString &input) const {
    QString value = input;
    if (m_delimiter == '\t') {
        // No quoting to protect them, so a tab or line break becomes a space.
        value.replace('\t', ' ').replace('\n', ' ');
        return value;
    }
    const bool quote = value.contains(m_delimiter) || value.contains('"') || value.contains('\n')
                    || value.startsWith(' ') || value.endsWith(' ');
    if (!quote)
        return value;
    value.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return '"' + value + '"';
}
