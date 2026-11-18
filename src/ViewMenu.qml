import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The list of views, opening upwards from its button in the status bar, each
// with the shortcut that switches to it directly.
FocusScope {
    id: menu
    property var views: []
    property string current: ""
    property int index: 0
    property real menuX: 10
    signal picked(string id)
    signal dismissed()

    onVisibleChanged: {
        if (!visible)
            return
        index = Math.max(0, views.findIndex((view) => view.id === current))
        keys.forceActiveFocus()
    }

    // A click anywhere else closes the list.
    MouseArea {
        anchors.fill: parent
        onClicked: menu.dismissed()
        onWheel: (wheel) => wheel.accepted = true
    }

    Item {
        id: keys
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Up)
                menu.index = (menu.index + menu.views.length - 1) % menu.views.length
            else if (event.key === Qt.Key_Down)
                menu.index = (menu.index + 1) % menu.views.length
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)
                menu.picked(menu.views[menu.index].id)
            else if (event.key === Qt.Key_Escape)
                menu.dismissed()
            else
                return
            event.accepted = true
        }
    }

    Rectangle {
        x: Math.min(menu.menuX, menu.width - width - 10)
        y: parent.height - height - 34
        width: Math.max(230, column.implicitWidth + 12)
        height: column.implicitHeight + 12
        radius: theme.radius
        color: theme.base
        border.color: theme.line

        MouseArea { anchors.fill: parent }

        Column {
            id: column
            x: 6
            y: 6
            width: parent.width - 12

            Repeater {
                model: menu.views
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    required property int index
                    readonly property bool chosen: modelData.id === menu.current
                    width: column.width
                    implicitWidth: name.implicitWidth + (combo.text !== "" ? combo.implicitWidth : 0) + 76
                    height: 32
                    radius: Math.min(4, theme.radius)
                    color: index === menu.index ? Qt.alpha(theme.accent, 0.16)
                         : optionHover.hovered ? theme.raised : "transparent"

                    Label {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: "✓"
                        visible: option.chosen
                        color: theme.accent
                        font.pixelSize: 13
                    }
                    Label {
                        id: name
                        x: 30
                        anchors.verticalCenter: parent.verticalCenter
                        text: option.modelData.label
                        color: theme.text
                        font.pixelSize: 13
                        font.weight: option.chosen ? Font.DemiBold : Font.Normal
                        font.strikeout: !!option.modelData.struck
                        opacity: option.modelData.struck ? 0.6 : 1
                    }
                    Label {
                        id: combo
                        visible: text !== ""
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: option.modelData.keys
                        color: theme.dim
                        font.family: "monospace"
                        font.pixelSize: 12
                    }
                    HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: menu.picked(option.modelData.id) }
                }
            }
        }
    }
}
