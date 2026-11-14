import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The JSON view: one row per value, objects and arrays fold, and a value is
// edited in place. JSON reads left to right whatever the interface language.
FocusScope {
    id: tree
    property int fontSize: 15
    property int editingRow: -1
    readonly property bool editing: editingRow >= 0
    readonly property int rowHeight: Math.round(fontSize * 1.9)

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    function toggle(row) {
        list.currentIndex = row
        jsonModel.toggle(row)
    }

    // Writes the new value over the old one in the text. The tree is rebuilt
    // from the text right after, so put the view back where it was.
    function commit(row, start, end, input) {
        const scroll = list.contentY
        editingRow = -1
        backend.replaceRange(start, end, jsonModel.literal(input))
        list.currentIndex = Math.min(row, list.count - 1)
        list.contentY = scroll
        list.forceActiveFocus()
    }

    function cancel() {
        editingRow = -1
        list.forceActiveFocus()
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 560)
        spacing: 10
        visible: !jsonModel.valid

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("This isn't valid JSON")
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: "#e6e6ea"
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: jsonModel.error
            color: "#f7768e"
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Press %1 to fix it in the text view.").arg("Esc")
            color: "#8b8b96"
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        visible: jsonModel.valid
        focus: true
        clip: true
        model: jsonModel
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        // A rebuilt tree always has a current row for the keys to act on.
        onCountChanged: if (currentIndex < 0 && count > 0) currentIndex = 0
        ScrollBar.vertical: ScrollBar {}

        Keys.onPressed: (event) => {
            const row = currentIndex
            const item = currentItem
            if (!item || (event.modifiers & (Qt.ControlModifier | Qt.AltModifier)))
                return
            if (event.key === Qt.Key_Space) {
                item.container ? tree.toggle(row) : tree.editingRow = row
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                item.container ? tree.toggle(row) : tree.editingRow = row
            } else if (event.key === Qt.Key_Left) {
                if (item.container && !item.collapsed)
                    tree.toggle(row)
                else if (jsonModel.parentRow(row) >= 0)
                    currentIndex = jsonModel.parentRow(row)
            } else if (event.key === Qt.Key_Right) {
                if (item.container && item.collapsed)
                    tree.toggle(row)
                else if (row + 1 < count)
                    currentIndex = row + 1
            } else if (event.key === Qt.Key_E) {
                jsonModel.expandAll()
                currentIndex = 0
            } else if (event.key === Qt.Key_C) {
                jsonModel.collapseAll()
                currentIndex = 0
            } else {
                return
            }
            event.accepted = true
        }

        delegate: Item {
            id: row
            required property int index
            required property int depth
            required property string key
            required property bool indexed
            required property string value
            required property string kind
            required property bool container
            required property bool collapsed
            required property int count
            required property int start
            required property int end
            readonly property bool editing: tree.editingRow === index
            readonly property color valueColor: kind === "string" ? "#9ece6a"
                                              : kind === "null" ? "#8b8b96" : "#ff9e64"

            width: list.width
            height: tree.rowHeight

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                radius: Math.min(4, theme.radius)
                color: row.ListView.isCurrentItem ? Qt.alpha(theme.accent, 0.16)
                     : rowHover.hovered ? "#17171b" : "transparent"
            }
            HoverHandler { id: rowHover }
            TapHandler {
                onTapped: {
                    list.forceActiveFocus()
                    row.container ? tree.toggle(row.index) : list.currentIndex = row.index
                }
                onDoubleTapped: if (!row.container) tree.editingRow = row.index
            }

            Row {
                id: cells
                x: 18 + row.depth * Math.round(tree.fontSize * 1.4)
                height: parent.height
                spacing: Math.round(tree.fontSize * 0.5)

                Item {
                    width: tree.fontSize
                    height: parent.height
                    Text {
                        anchors.centerIn: parent
                        visible: row.container
                        text: "›"
                        rotation: row.collapsed ? 0 : 90
                        color: "#9a9aa6"
                        font.pixelSize: Math.round(tree.fontSize * 1.5)
                    }
                }
                Text {
                    visible: row.depth > 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.key + ":"
                    color: row.indexed ? "#6b7089" : "#bb9af7"
                    font.family: "monospace"
                    font.pixelSize: tree.fontSize
                }
                Text {
                    visible: !row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, Math.max(40, row.width - cells.x - x - 24))
                    elide: Text.ElideRight
                    text: !row.container ? row.value
                        : (row.kind === "object" ? "{ } " : "[ ] ") + qsTr("%n items", "", row.count)
                    color: row.container ? "#6b7089" : row.valueColor
                    font.family: "monospace"
                    font.pixelSize: tree.fontSize
                }
                Loader {
                    active: row.editing
                    visible: active
                    anchors.verticalCenter: parent.verticalCenter
                    sourceComponent: Rectangle {
                        width: Math.max(160, row.width - cells.x - parent.x - 24)
                        height: tree.rowHeight - 4
                        radius: Math.min(4, theme.radius)
                        color: "#0e0e10"
                        border.color: theme.accent

                        TextInput {
                            id: input
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            text: row.value
                            color: "#e6e6ea"
                            selectionColor: Qt.alpha(theme.accent, 0.35)
                            selectedTextColor: "#ffffff"
                            font.family: "monospace"
                            font.pixelSize: tree.fontSize
                            selectByMouse: true
                            Component.onCompleted: { forceActiveFocus(); selectAll() }
                            onAccepted: tree.commit(row.index, row.start, row.end, text)
                            Keys.onEscapePressed: tree.cancel()
                            // Clicking away keeps the old value.
                            onActiveFocusChanged: if (!activeFocus && row.editing) tree.cancel()
                        }
                    }
                }
            }
        }
    }
}
