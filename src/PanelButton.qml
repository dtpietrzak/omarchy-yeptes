import QtQuick
import QtQuick.Controls

// A small square button for the notes panel and its collapsed rail.
Rectangle {
    id: button
    property string text: ""
    property string tip: ""
    signal clicked()
    implicitWidth: 18
    implicitHeight: 18
    radius: Math.min(4, theme.radius)
    color: hover.hovered ? theme.raised : "transparent"

    Label { anchors.centerIn: parent; text: button.text; color: theme.subtext; font.pixelSize: 16 }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }
    ToolTip.visible: hover.hovered && button.tip !== ""
    ToolTip.delay: 600
    ToolTip.text: button.tip
}
