import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The Table view: delimited text as a grid. The first row is the heading,
// and a cell is edited in place. Data reads left to right whatever the
// interface language.
FocusScope {
    id: table
    property int fontSize: 15
    property int currentRow: 0
    property int currentColumn: 0
    property bool editing: false
    readonly property int rowHeight: Math.round(fontSize * 1.9)

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    FontMetrics {
        id: metrics
        font.family: "monospace"
        font.pixelSize: table.fontSize
    }

    function move(rows, columns) {
        currentRow = Math.max(0, Math.min(view.rows - 1, currentRow + rows))
        currentColumn = Math.max(0, Math.min(view.columns - 1, currentColumn + columns))
        view.positionViewAtCell(Qt.point(currentColumn, currentRow), TableView.Contain)
    }

    function edit() {
        // A row shorter than the others has no characters to edit there.
        if (tableModel.cellStart(currentRow, currentColumn) >= 0)
            editing = true
    }

    // Writes the new value over the old one in the text. The table is rebuilt
    // from the text right after, so put the view back where it was.
    function commit(input) {
        const x = view.contentX, y = view.contentY
        editing = false
        backend.replaceRange(tableModel.cellStart(currentRow, currentColumn),
                             tableModel.cellEnd(currentRow, currentColumn), tableModel.field(input))
        view.contentX = x
        view.contentY = y
        view.forceActiveFocus()
    }

    function cancel() {
        editing = false
        view.forceActiveFocus()
    }

    Connections {
        target: tableModel
        function onChanged() {
            table.currentRow = Math.max(0, Math.min(tableModel.rowCount() - 1, table.currentRow))
            table.currentColumn = Math.max(0, Math.min(tableModel.columnCount() - 1, table.currentColumn))
        }
    }

    Label {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 560)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: tableModel.empty
        text: qsTr("There is nothing to show as a table yet.")
        color: theme.dim
    }

    TableView {
        id: view
        anchors.fill: parent
        anchors.margins: 14
        visible: !tableModel.empty
        focus: true
        clip: true
        model: tableModel
        boundsBehavior: Flickable.StopAtBounds
        columnWidthProvider: (column) => Math.round(Math.min(48, Math.max(4, tableModel.columnChars(column))) * metrics.averageCharacterWidth + 24)
        rowHeightProvider: () => table.rowHeight
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        Keys.onPressed: (event) => {
            if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier))
                return
            if (event.key === Qt.Key_Up) table.move(-1, 0)
            else if (event.key === Qt.Key_Down) table.move(1, 0)
            else if (event.key === Qt.Key_Left) table.move(0, -1)
            else if (event.key === Qt.Key_Right || event.key === Qt.Key_Tab) table.move(0, 1)
            else if (event.key === Qt.Key_Backtab) table.move(0, -1)
            else if (event.key === Qt.Key_Home) table.move(0, -view.columns)
            else if (event.key === Qt.Key_End) table.move(0, view.columns)
            else if (event.key === Qt.Key_PageUp) table.move(-Math.max(1, Math.floor(view.height / table.rowHeight) - 1), 0)
            else if (event.key === Qt.Key_PageDown) table.move(Math.max(1, Math.floor(view.height / table.rowHeight) - 1), 0)
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) table.edit()
            else return
            event.accepted = true
        }

        delegate: Rectangle {
            id: cell
            required property int row
            required property int column
            required property string display
            readonly property bool current: row === table.currentRow && column === table.currentColumn
            readonly property bool editing: current && table.editing

            implicitHeight: table.rowHeight
            color: current ? Qt.alpha(theme.accent, 0.16) : row === 0 ? theme.raised : "transparent"
            border.width: 1
            border.color: current ? theme.accent : theme.line

            Text {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                visible: !cell.editing
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: cell.display
                color: cell.row === 0 ? theme.text : theme.subtext
                font.family: "monospace"
                font.pixelSize: table.fontSize
                font.weight: cell.row === 0 ? Font.DemiBold : Font.Normal
            }
            TapHandler {
                onTapped: {
                    view.forceActiveFocus()
                    table.currentRow = cell.row
                    table.currentColumn = cell.column
                }
                onDoubleTapped: table.edit()
            }
            Loader {
                anchors.fill: parent
                active: cell.editing
                sourceComponent: TextInput {
                    leftPadding: 10
                    rightPadding: 10
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    text: cell.display
                    color: theme.text
                    selectionColor: Qt.alpha(theme.accent, 0.35)
                    selectedTextColor: theme.text
                    font.family: "monospace"
                    font.pixelSize: table.fontSize
                    selectByMouse: true
                    Component.onCompleted: { forceActiveFocus(); selectAll() }
                    onAccepted: table.commit(text)
                    Keys.onEscapePressed: table.cancel()
                    // Clicking away keeps the old value.
                    onActiveFocusChanged: if (!activeFocus && cell.editing) table.cancel()
                }
            }
        }
    }
}
