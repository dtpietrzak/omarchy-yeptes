import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A flat button drawn with plain rectangles, so it takes the window's corner
// radius and the accent directly. The focused one wears a ring.
Rectangle {
    id: button
    property string text: ""
    property bool primary: false
    signal clicked()

    implicitWidth: label.implicitWidth + 30
    implicitHeight: 36
    radius: Math.min(theme.radius, height / 2)
    color: primary ? theme.accent : hover.hovered ? "#26262d" : "transparent"
    border.width: activeFocus ? 2 : 1
    border.color: activeFocus ? (primary ? "#ffffff" : theme.accent) : primary ? "transparent" : "#34343d"
    activeFocusOnTab: true

    Label {
        id: label
        anchors.centerIn: parent
        text: button.text
        font.pixelSize: 14
        font.weight: button.primary ? Font.DemiBold : Font.Normal
        color: button.primary ? theme.accentForeground : "#e6e6ea"
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }
    Keys.onReturnPressed: button.clicked()
    Keys.onEnterPressed: button.clicked()
    Keys.onSpacePressed: button.clicked()
}
