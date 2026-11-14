import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Text appearance, opening upwards from the cog in the status bar. The
// values live on the window; this only edits them.
FocusScope {
    id: menu
    property int fontSize: 12
    property string fontFamily: ""
    property color backgroundColor: "#1a1b26"
    property color textColor: "#c0caf5"
    readonly property var backgrounds: ["#0e0e10", "#000000", "#1a1b26", "#282a36", "#002b36", "#1e1e1e", "#fdf6e3", "#ffffff"]
    readonly property var foregrounds: ["#e6e6ea", "#ffffff", "#c0caf5", "#f8f8f2", "#93a1a1", "#a9b665", "#586e75", "#111111"]
    readonly property var families: [qsTr("Monospace")].concat(backend.monospaceFamilies())
    signal sizeChanged(int size)
    signal familyPicked(string family)
    signal backgroundPicked(color value)
    signal textPicked(color value)
    signal resetRequested()
    signal dismissed()

    onVisibleChanged: if (visible) forceActiveFocus()
    Keys.onEscapePressed: menu.dismissed()

    // A click anywhere else closes the panel.
    MouseArea {
        anchors.fill: parent
        onClicked: menu.dismissed()
        onWheel: (wheel) => wheel.accepted = true
    }

    component Swatches: Flow {
        id: swatches
        property var colors: []
        property color current
        signal picked(color value)
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: swatches.colors
            delegate: Rectangle {
                required property string modelData
                width: 24
                height: 24
                radius: 12
                color: modelData
                border.width: Qt.colorEqual(modelData, swatches.current) ? 2 : 1
                border.color: Qt.colorEqual(modelData, swatches.current) ? theme.accent : "#3a3a44"
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: swatches.picked(modelData) }
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: parent.height - height - 34
        width: 280
        height: content.implicitHeight + 28
        radius: theme.radius
        color: "#17171b"
        border.color: "#2a2a32"

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: content
            x: 14
            y: 14
            width: parent.width - 28
            spacing: 8

            Label { text: qsTr("Font size"); color: "#8b8b96"; font.pixelSize: 12 }
            RowLayout {
                spacing: 8
                PlainButton { text: "−"; onClicked: menu.sizeChanged(Math.max(9, menu.fontSize - 1)) }
                Label {
                    text: menu.fontSize
                    color: "#e6e6ea"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 32
                }
                PlainButton { text: "+"; onClicked: menu.sizeChanged(Math.min(40, menu.fontSize + 1)) }
            }

            Label { text: qsTr("Font type"); color: "#8b8b96"; font.pixelSize: 12; Layout.topMargin: 4 }
            ComboBox {
                id: familyBox
                Layout.fillWidth: true
                implicitHeight: 30
                topPadding: 0
                bottomPadding: 0
                font.pixelSize: 13
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: familyBox.width
                    height: 28
                    topPadding: 0
                    bottomPadding: 0
                    text: modelData
                    font.pixelSize: 13
                    highlighted: familyBox.highlightedIndex === index
                }
                model: menu.families
                currentIndex: Math.max(0, menu.families.indexOf(menu.fontFamily))
                onActivated: (i) => menu.familyPicked(i === 0 ? "monospace" : menu.families[i])
            }

            Label { text: qsTr("Background color"); color: "#8b8b96"; font.pixelSize: 12; Layout.topMargin: 4 }
            Swatches {
                colors: menu.backgrounds
                current: menu.backgroundColor
                onPicked: (value) => menu.backgroundPicked(value)
            }

            Label { text: qsTr("Font color"); color: "#8b8b96"; font.pixelSize: 12; Layout.topMargin: 4 }
            Swatches {
                colors: menu.foregrounds
                current: menu.textColor
                onPicked: (value) => menu.textPicked(value)
            }

            PlainButton {
                text: qsTr("Reset")
                Layout.topMargin: 4
                onClicked: menu.resetRequested()
            }
        }
    }
}
