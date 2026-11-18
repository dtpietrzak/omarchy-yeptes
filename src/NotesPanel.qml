import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The notes kept in Documents/Yeptes, newest first.
Rectangle {
    id: panel
    property var notes: []
    property string current: ""
    // The editor's font, so the list reads as part of the same page.
    property string fontFamily: "monospace"
    // What the search box at the bottom holds. While it has something, only
    // the notes containing it show, and they can't be dragged into order.
    property string query: ""
    readonly property bool searching: query.trim() !== ""
    // Searched again when the list itself changes, such as after a save.
    readonly property var matches: { panel.notes; return searching ? backend.notesContaining(query) : [] }
    readonly property var shown: searching ? notes.filter((note) => matches.indexOf(note.path) >= 0) : notes
    signal picked(string path)
    signal collapseRequested()
    signal newRequested()
    signal deleteRequested(string path)
    signal moved(int from, int to)
    color: theme.chrome

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            Layout.leftMargin: 14
            Layout.rightMargin: 6
            Column {
                Layout.fillWidth: true
                spacing: 1
                Label {
                    width: parent.width
                    text: qsTr("Quick Notes")
                    color: theme.dim
                    font.family: panel.fontFamily
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    text: backend.notesFolderLabel()
                    color: theme.faint
                    font.family: panel.fontFamily
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }
            Column {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 4
                spacing: 2
                PanelButton { text: "»"; tip: qsTr("Hide notes"); onClicked: panel.collapseRequested() }
                PanelButton { text: "+"; tip: qsTr("New note (Ctrl+N)"); onClicked: panel.newRequested() }
            }
        }

        // Sets the heading apart from the notes, edge to edge.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: theme.line
            opacity: 0.45
        }

        // The list sits a shade lighter than the heading and the search
        // box, so the notes read as the panel's page.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Halfway between the dark heading and the theme's background.
            color: Qt.tint(theme.chrome, Qt.alpha(theme.base, 0.5))

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: panel.shown
                boundsBehavior: Flickable.StopAtBounds
                // Rows are moved by dragging them, not by flicking the list.
                interactive: false
                // The row being dragged and the place it would land.
                property int dragFrom: -1
                property int dragTo: -1
                // The row under the pointer and the open note; the hairlines on either
                // side of them step aside so their highlight reads as one block.
                property int hovered: -1
                readonly property int chosen: panel.shown.findIndex((note) => note.path === panel.current)
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool chosen: modelData.path === panel.current
                    width: ListView.view.width
                    height: 44
                    z: index === list.dragFrom ? 2 : 0
                    // Rows between the dragged one and its target slide out of the way.
                    readonly property real shift: {
                        if (list.dragFrom < 0)
                            return 0
                        if (index === list.dragFrom)
                            return Math.max(-index * height, Math.min((list.count - 1 - index) * height, rowDrag.activeTranslation.y))
                        if (list.dragFrom < list.dragTo && index > list.dragFrom && index <= list.dragTo)
                            return -height
                        if (list.dragFrom > list.dragTo && index >= list.dragTo && index < list.dragFrom)
                            return height
                        return 0
                    }
                    transform: Translate {
                        y: row.shift
                        Behavior on y { enabled: row.index !== list.dragFrom; NumberAnimation { duration: 120 } }
                    }
                    color: chosen ? Qt.alpha(theme.accent, 0.16) : rowHover.hovered ? theme.raised : "transparent"

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 14
                        width: parent.width - 28
                        spacing: 2
                        Label {
                            id: nameLabel
                            width: parent.width
                            // A long name is cut towards its end rather than at it,
                            // so the last five characters always show.
                            readonly property string name: row.modelData.name
                            readonly property string tail: name.slice(-5)
                            text: name.length <= 5 || fullMetrics.advanceWidth <= width ? name
                                : headMetrics.elidedText + tail
                            color: row.chosen ? theme.text : theme.subtext
                            font.family: panel.fontFamily
                            font.pixelSize: 11
                            font.weight: row.chosen ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            TextMetrics { id: fullMetrics; font: nameLabel.font; text: nameLabel.name }
                            TextMetrics { id: tailMetrics; font: nameLabel.font; text: nameLabel.tail }
                            TextMetrics {
                                id: headMetrics
                                font: nameLabel.font
                                text: nameLabel.name.slice(0, -5)
                                elide: Text.ElideRight
                                elideWidth: Math.max(0, nameLabel.width - tailMetrics.advanceWidth)
                            }
                        }
                        Label {
                            width: parent.width
                            text: row.modelData.date
                            color: theme.dim
                            font.family: panel.fontFamily
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                    // A hairline between notes, left off under the last one.
                    Rectangle {
                        visible: row.index < list.count - 1
                                 && list.hovered !== row.index && list.hovered !== row.index + 1
                                 && list.chosen !== row.index && list.chosen !== row.index + 1
                        anchors.bottom: parent.bottom
                        x: 14
                        width: parent.width - 28
                        height: 1
                        color: theme.line
                        opacity: 0.45
                    }
                    HoverHandler {
                        id: rowHover
                        cursorShape: Qt.PointingHandCursor
                        onHoveredChanged: {
                            if (hovered)
                                list.hovered = row.index
                            else if (list.hovered === row.index)
                                list.hovered = -1
                        }
                    }
                    DragHandler {
                        id: rowDrag
                        target: null
                        enabled: !panel.searching
                        acceptedButtons: Qt.LeftButton
                        xAxis.enabled: false
                        onActiveChanged: {
                            if (active) {
                                list.dragFrom = row.index
                                list.dragTo = row.index
                            } else if (list.dragFrom >= 0) {
                                const from = list.dragFrom, to = list.dragTo
                                list.dragFrom = -1
                                list.dragTo = -1
                                if (from !== to)
                                    panel.moved(from, to)
                            }
                        }
                        onActiveTranslationChanged: {
                            if (active)
                                list.dragTo = Math.max(0, Math.min(list.count - 1, Math.round(row.index + activeTranslation.y / row.height)))
                        }
                    }
                    TapHandler { onTapped: panel.picked(row.modelData.path) }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: rowMenu.popup()
                    }
                    Menu {
                        id: rowMenu
                        MenuItem {
                            implicitHeight: 28
                            topPadding: 0
                            bottomPadding: 0
                            font.pixelSize: 13
                            text: qsTr("Delete")
                            onTriggered: panel.deleteRequested(row.modelData.path)
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 28
                    visible: list.count === 0
                    text: panel.searching ? qsTr("No notes contain “%1”.").arg(panel.query.trim())
                                          : qsTr("No notes yet. Press Ctrl+S on a new note to keep it here.")
                    color: theme.dim
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: theme.line
            opacity: 0.45
        }

        // Search by what the notes say. Escape clears it.
        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 8
            implicitHeight: 24
            leftPadding: 8
            rightPadding: clear.visible ? 24 : 8
            topPadding: 0
            bottomPadding: 0
            verticalAlignment: TextInput.AlignVCenter
            font.family: panel.fontFamily
            font.pixelSize: 11
            color: theme.text
            selectionColor: Qt.alpha(theme.accent, 0.35)
            selectedTextColor: theme.text
            text: panel.query
            onTextEdited: panel.query = text
            Keys.onEscapePressed: (event) => {
                if (text === "") {
                    event.accepted = false
                    return
                }
                panel.query = ""
                text = ""
            }
            background: Rectangle {
                radius: Math.min(4, theme.radius)
                color: theme.base
                border.color: search.activeFocus ? theme.accent : theme.line
            }

            // Drawn by hand: Material's placeholder floats up above the box
            // on focus, and this one simply stays until there is text.
            Label {
                visible: search.text === "" && search.preeditText === ""
                anchors.left: parent.left
                anchors.leftMargin: search.leftPadding
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Search notes")
                color: theme.faint
                font: search.font
            }

            Rectangle {
                id: clear
                visible: search.text !== ""
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                radius: Math.min(4, theme.radius)
                color: clearHover.hovered ? theme.raised : "transparent"
                Label { anchors.centerIn: parent; text: "×"; color: theme.dim; font.pixelSize: 12 }
                HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { panel.query = ""; search.text = "" } }
            }
        }
    }
}
