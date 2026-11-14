import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The notes kept in Documents/Yeptes, newest first.
Rectangle {
    id: panel
    property var notes: []
    property string current: ""
    signal picked(string path)
    signal collapseRequested()
    signal deleteRequested(string path)
    signal moved(int from, int to)
    color: "#141417"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            Layout.leftMargin: 14
            Layout.rightMargin: 6
            Item { Layout.fillWidth: true }
            Rectangle {
                implicitWidth: 24
                implicitHeight: 22
                radius: Math.min(4, theme.radius)
                color: collapseHover.hovered ? "#26262d" : "transparent"
                Label { anchors.centerIn: parent; text: "»"; color: "#c9c9d1"; font.pixelSize: 14 }
                HoverHandler { id: collapseHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: panel.collapseRequested() }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: panel.notes
            boundsBehavior: Flickable.StopAtBounds
            // Rows are moved by dragging them, not by flicking the list.
            interactive: false
            // The row being dragged and the place it would land.
            property int dragFrom: -1
            property int dragTo: -1
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
                color: chosen ? Qt.alpha(theme.accent, 0.16) : rowHover.hovered ? "#1d1d22" : "transparent"

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 14
                    width: parent.width - 28
                    spacing: 2
                    Label {
                        width: parent.width
                        text: row.modelData.name
                        color: "#e6e6ea"
                        font.pixelSize: 13
                        font.weight: row.chosen ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                    Label {
                        width: parent.width
                        text: row.modelData.date
                        color: "#8b8b96"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                DragHandler {
                    id: rowDrag
                    target: null
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
                text: qsTr("No notes yet. Press Ctrl+S on a new note to keep it here.")
                color: "#8b8b96"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
