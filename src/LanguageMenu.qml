import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The Code view's languages, opening upwards from their button in the status
// bar. There are too many for one column, so they are laid out in a grid the
// arrow keys move around.
FocusScope {
    id: menu
    property var languages: []
    property string current: ""
    property int index: 0
    property real menuX: 10
    readonly property int columns: 3
    signal picked(string id)
    signal dismissed()

    onVisibleChanged: {
        if (!visible)
            return
        index = Math.max(0, languages.findIndex((language) => language.id === current))
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
            const count = menu.languages.length
            if (count === 0)
                return
            // Left and right swap places in a mirrored window.
            const step = Qt.application.layoutDirection === Qt.RightToLeft ? -1 : 1
            if (event.key === Qt.Key_Up)
                menu.index = (menu.index + count - menu.columns) % count
            else if (event.key === Qt.Key_Down)
                menu.index = (menu.index + menu.columns) % count
            else if (event.key === Qt.Key_Left)
                menu.index = (menu.index + count - step) % count
            else if (event.key === Qt.Key_Right)
                menu.index = (menu.index + count + step) % count
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)
                menu.picked(menu.languages[menu.index].id)
            else if (event.key === Qt.Key_Escape)
                menu.dismissed()
            else
                return
            event.accepted = true
        }
    }

    Rectangle {
        x: Math.max(10, Math.min(menu.menuX, menu.width - width - 10))
        y: parent.height - height - 34
        width: grid.implicitWidth + 12
        height: grid.implicitHeight + 12
        radius: theme.radius
        color: theme.base
        border.color: theme.line

        MouseArea { anchors.fill: parent }

        Grid {
            id: grid
            x: 6
            y: 6
            columns: menu.columns

            Repeater {
                model: menu.languages
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    required property int index
                    readonly property bool chosen: modelData.id === menu.current
                    width: 140
                    height: 30
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
                        x: 30
                        width: parent.width - 36
                        anchors.verticalCenter: parent.verticalCenter
                        text: option.modelData.label
                        elide: Text.ElideRight
                        color: theme.text
                        font.pixelSize: 13
                        font.weight: option.chosen ? Font.DemiBold : Font.Normal
                    }
                    HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: menu.picked(option.modelData.id) }
                }
            }
        }
    }
}
