import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A read-only rendering of the note, in a column narrow enough to read. The
// backend turns Markdown or HTML into the rich text shown here.
ScrollView {
    id: view
    property string content: ""
    property url baseUrl

    contentWidth: availableWidth
    contentHeight: page.implicitHeight + 56

    Item {
        width: view.availableWidth
        height: page.implicitHeight + 56

        TextEdit {
            id: page
            x: Math.max(28, (parent.width - width) / 2)
            y: 28
            width: Math.min(parent.width - 56, 860)
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.RichText
            text: view.content
            baseUrl: view.baseUrl
            wrapMode: TextEdit.Wrap
            color: "#e6e6ea"
            selectionColor: Qt.alpha(theme.accent, 0.35)
            selectedTextColor: "#ffffff"
            onLinkActivated: (link) => Qt.openUrlExternally(link)

            HoverHandler {
                cursorShape: page.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
            }
        }
    }
}
