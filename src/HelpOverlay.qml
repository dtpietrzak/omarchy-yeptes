import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Every shortcut, over the window. Keys are shown as they are on the
// keyboard; only what they do is translated.
Rectangle {
    id: overlay
    signal dismissed()
    color: Qt.alpha(theme.scrim, 0.88)

    readonly property var shortcuts: [
        { keys: "Ctrl+N", text: qsTr("New note") },
        { keys: "Ctrl+O", text: qsTr("Open a file") },
        { keys: "Ctrl+S", text: qsTr("Save") },
        { keys: "Ctrl+Shift+S", text: qsTr("Save as") },
        { keys: "Ctrl+B", text: qsTr("Show or hide the notes panel") },
        { keys: "Ctrl+E", text: qsTr("Export a note without its history") },
        { keys: "Ctrl+1 … Ctrl+7", text: qsTr("Text, Markdown, JSON, HTML, Table, Code, Diff view") },
        { keys: "Esc", text: qsTr("Back to the text") },
        { keys: "Ctrl+Z", text: qsTr("Undo") },
        { keys: "Ctrl+Shift+Z", text: qsTr("Redo") },
        { keys: "Ctrl++  Ctrl+-  Ctrl+0", text: qsTr("Larger, smaller, normal text") },
        { keys: "Alt+Z", text: qsTr("Wrap long lines") },
        { keys: "Ctrl+/  F1", text: qsTr("Show these shortcuts") },
        { keys: "Ctrl+Q", text: qsTr("Quit") },
        { keys: "↑ ↓", text: qsTr("JSON: move between values") },
        { keys: "← →  Space", text: qsTr("JSON: fold and unfold") },
        { keys: "Enter", text: qsTr("JSON: edit the value") },
        { keys: "E  C", text: qsTr("JSON: expand all, collapse all") },
        { keys: "↑ ↓ ← →  Enter", text: qsTr("Table: move between cells, edit the cell") },
        { keys: "- [ ]", text: qsTr("Click the box of a checklist line to tick it") }
    ]

    // Swallows clicks and scrolling so nothing underneath reacts.
    MouseArea {
        anchors.fill: parent
        onClicked: overlay.dismissed()
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        height: Math.min(parent.height - 48, content.implicitHeight + 48)
        radius: theme.radius
        color: theme.base
        border.color: theme.line

        Flickable {
            anchors.fill: parent
            anchors.margins: 24
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: content
                width: parent.width
                spacing: 7

                Label {
                    text: qsTr("Shortcuts")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: theme.text
                    bottomPadding: 8
                }
                Repeater {
                    model: overlay.shortcuts
                    delegate: Row {
                        required property var modelData
                        width: content.width
                        spacing: 16
                        Label {
                            width: 190
                            text: modelData.keys
                            font.family: "monospace"
                            font.pixelSize: 13
                            color: theme.accent
                        }
                        Label {
                            width: parent.width - 206
                            text: modelData.text
                            font.pixelSize: 13
                            color: theme.subtext
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}
