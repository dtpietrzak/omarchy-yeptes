import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The Diff view: what has changed since the last save, then every saved
// change a Yeptes note has recorded, newest first.
FocusScope {
    id: diff
    property int fontSize: 15
    property var rows: []
    property bool note: true
    readonly property int lineHeight: Math.round(fontSize * 1.55)

    FontMetrics {
        id: metrics
        font.family: "monospace"
        font.pixelSize: diff.fontSize
    }

    Label {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 560)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: diff.rows.length === 0
        text: diff.note ? qsTr("No changes yet. Every save of this note is recorded here.")
                        : qsTr("No unsaved changes. Only Yeptes notes (.ypts) keep a history of their saves.")
        color: "#8b8b96"
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: 10
        visible: diff.rows.length > 0
        focus: true
        clip: true
        model: diff.rows
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        Keys.onUpPressed: flick(0, 900)
        Keys.onDownPressed: flick(0, -900)

        delegate: Item {
            id: row
            required property var modelData
            readonly property string kind: modelData.kind
            width: list.width
            height: kind === "entry" ? 46 : kind === "gap" ? 16 : diff.lineHeight

            // The heading of one change: when it was saved, and its size.
            Row {
                visible: row.kind === "entry"
                x: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                spacing: 12
                Label {
                    text: {
                        translator.language
                        return row.modelData.unsaved ? qsTr("Unsaved changes")
                             : new Date(row.modelData.at).toLocaleString(Qt.locale(), Locale.ShortFormat)
                    }
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: row.modelData.unsaved ? theme.accent : "#e6e6ea"
                }
                Label {
                    text: "+" + (row.modelData.added || 0)
                    font.pixelSize: 13
                    color: "#9ece6a"
                }
                Label {
                    text: "−" + (row.modelData.removed || 0)
                    font.pixelSize: 13
                    color: "#f7768e"
                }
            }

            Label {
                visible: row.kind === "gap"
                x: 18
                anchors.verticalCenter: parent.verticalCenter
                text: "⋯"
                color: "#5f5f6a"
            }

            // Code reads left to right whatever the interface language.
            Rectangle {
                visible: row.kind === "add" || row.kind === "remove" || row.kind === "context"
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: true
                x: 8
                width: parent.width - 16
                height: parent.height
                color: row.kind === "add" ? Qt.alpha("#9ece6a", 0.13)
                     : row.kind === "remove" ? Qt.alpha("#f7768e", 0.13) : "transparent"

                Text {
                    x: 6
                    width: metrics.averageCharacterWidth * 5
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.oldNumber || ""
                    color: "#5f5f6a"
                    font.family: "monospace"
                    font.pixelSize: diff.fontSize - 2
                }
                Text {
                    x: 6 + metrics.averageCharacterWidth * 5.5
                    width: metrics.averageCharacterWidth * 5
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.newNumber || ""
                    color: "#5f5f6a"
                    font.family: "monospace"
                    font.pixelSize: diff.fontSize - 2
                }
                Text {
                    x: 6 + metrics.averageCharacterWidth * 12
                    width: parent.width - x - 8
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    text: (row.kind === "add" ? "+ " : row.kind === "remove" ? "- " : "  ") + (row.modelData.text || "")
                    color: row.kind === "add" ? "#b9f27c" : row.kind === "remove" ? "#ff9aa9" : "#b4b4bd"
                    font.family: "monospace"
                    font.pixelSize: diff.fontSize
                }
            }
        }
    }
}
