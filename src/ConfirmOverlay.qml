import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Asks what to do with unsaved changes before they would be lost. Drawn the
// same way as the shortcuts overlay, so it follows the window's corners.
Rectangle {
    id: overlay
    property string name: ""
    // A quick note whose text was all deleted: offer to delete the note
    // itself (to the trash) or go back to what was saved.
    property bool empty: false
    signal saveChosen()
    signal deleteChosen()
    signal discardChosen()
    signal cancelled()

    color: Qt.alpha(theme.scrim, 0.88)
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
        color: theme.base
        border.color: theme.line

        Column {
            id: content
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 10

            Label {
                width: parent.width
                text: overlay.empty ? qsTr("There's no text in %1").arg(overlay.name)
                                    : qsTr("Save changes to %1?").arg(overlay.name)
                wrapMode: Text.Wrap
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: theme.text
            }
            Label {
                width: parent.width
                text: overlay.empty ? qsTr("Do you want to delete this note, or revert it to how it was last saved?")
                                    : qsTr("Your changes will be lost if you don't save them.")
                wrapMode: Text.Wrap
                color: theme.subtext
            }
            Flow {
                width: parent.width
                topPadding: 8
                spacing: 8
                layoutDirection: Qt.RightToLeft

                PlainButton {
                    id: saveButton
                    text: overlay.empty ? qsTr("Delete") : qsTr("Save")
                    primary: true
                    onClicked: overlay.empty ? overlay.deleteChosen() : overlay.saveChosen()
                    KeyNavigation.tab: cancelButton
                    KeyNavigation.backtab: discardButton
                }
                PlainButton {
                    id: discardButton
                    text: overlay.empty ? qsTr("Revert") : qsTr("Discard")
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
