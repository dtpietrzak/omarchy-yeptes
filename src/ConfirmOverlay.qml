import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Asks what to do with unsaved changes before they would be lost. Drawn the
// same way as the shortcuts overlay, so it follows the window's corners.
Rectangle {
    id: overlay
    property string name: ""
    signal saveChosen()
    signal discardChosen()
    signal cancelled()

    color: Qt.alpha("#0e0e10", 0.88)
    onVisibleChanged: if (visible) saveButton.forceActiveFocus()
    Keys.onEscapePressed: overlay.cancelled()

    // Swallows clicks and scrolling so nothing underneath reacts.
    MouseArea {
        anchors.fill: parent
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 460)
        height: content.implicitHeight + 40
        radius: theme.radius
        color: "#17171b"
        border.color: "#2a2a32"

        Column {
            id: content
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 10

            Label {
                width: parent.width
                text: qsTr("Save changes to %1?").arg(overlay.name)
                wrapMode: Text.Wrap
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#e6e6ea"
            }
            Label {
                width: parent.width
                text: qsTr("Your changes will be lost if you don't save them.")
                wrapMode: Text.Wrap
                color: "#b4b4bd"
            }
            Flow {
                width: parent.width
                topPadding: 8
                spacing: 8
                layoutDirection: Qt.RightToLeft

                PlainButton {
                    id: saveButton
                    text: qsTr("Save")
                    primary: true
                    onClicked: overlay.saveChosen()
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: discardButton
                }
                PlainButton {
                    id: discardButton
                    text: qsTr("Discard")
                    onClicked: overlay.discardChosen()
                    KeyNavigation.tab: saveButton
                    KeyNavigation.backtab: cancelButton
                }
                PlainButton {
                    id: cancelButton
                    text: qsTr("Cancel")
                    onClicked: overlay.cancelled()
                    KeyNavigation.tab: discardButton
                    KeyNavigation.backtab: saveButton
                }
            }
        }
    }
}
