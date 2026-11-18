import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Shapes
import QtCore

ApplicationWindow {
    id: win
    width: 1000
    height: 720
    minimumWidth: 560
    minimumHeight: 400
    visible: true
    title: qsTr("%1 — Yeptes").arg(win.displayName + (backend.modified ? " •" : ""))
    color: win.editorBackground

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.background: theme.base
    Material.foreground: theme.text
    Material.roundedScale: theme.radius

    readonly property string displayName: backend.fileName !== "" ? backend.fileName : qsTr("Untitled")
    readonly property var modes: [
        { id: "text", label: qsTr("Text"), keys: "Ctrl+1" },
        { id: "markdown", label: qsTr("Markdown"), keys: "Ctrl+2" },
        { id: "json", label: qsTr("JSON"), keys: "Ctrl+3" },
        { id: "html", label: qsTr("HTML"), keys: "Ctrl+4" },
        { id: "table", label: qsTr("Table"), keys: "Ctrl+5" },
        { id: "code", label: qsTr("Code"), keys: "Ctrl+6" },
        { id: "diff", label: qsTr("Diff"), keys: "Ctrl+7" }
    ]
    property string mode: "text"
    // Text and Code are the same editor; Code colors all of it in a language.
    readonly property bool editing: mode === "text" || mode === "code"
    readonly property string modeLabel: (modes.find((view) => view.id === mode) || modes[0]).label
    property bool helpVisible: false
    property bool viewMenuVisible: false
    property bool settingsVisible: false
    property bool exportMenuVisible: false
    property bool languageMenuVisible: false
    // "Plain text" first, then every language there are rules for.
    readonly property var languageViews: [{ id: "", label: qsTr("Plain text") }].concat(backend.codeLanguages())
    // The formats the text really is, worked out when the menu opens. Others
    // are struck through, but still can be picked.
    property var exportFits: []
    readonly property var exportViews: exportTypes.map((type) => Object.assign({}, type, { struck: type.id !== "" && exportFits.indexOf(type.id) < 0 }))
    readonly property var exportTypes: [
        { id: ".ypts", label: ".ypts", keys: "" },
        { id: ".txt", label: ".txt", keys: "" },
        { id: ".md", label: ".md", keys: "" },
        { id: ".json", label: ".json", keys: "" },
        { id: ".html", label: ".html", keys: "" },
        { id: ".csv", label: ".csv", keys: "" },
        { id: ".tsv", label: ".tsv", keys: "" },
        { id: "", label: qsTr("anything"), keys: "" }
    ]
    property int fontSize: 12
    property string fontFamily: "monospace"
    // The editor follows the Omarchy theme until a color is picked by hand.
    property bool themeBackground: true
    property bool themeText: true
    property color backgroundColor: "#1a1b26"
    property color textColor: "#c0caf5"
    readonly property color editorBackground: themeBackground ? theme.editorBackground : backgroundColor
    readonly property color editorText: themeText ? theme.editorText : textColor

    // Text appearance is remembered between runs.
    Settings {
        category: "appearance"
        property alias fontSize: win.fontSize
        property alias fontFamily: win.fontFamily
        property alias backgroundColor: win.backgroundColor
        property alias textColor: win.textColor
        property alias themeBackground: win.themeBackground
        property alias themeText: win.themeText
    }
    property bool wrap: true

    // The notes panel on the right: shown or collapsed, and how wide.
    property bool notesVisible: true
    property real notesWidth: 200
    property var noteList: []
    // The order you dragged the notes into (a JSON list of paths). Notes that
    // are not in it yet, such as a newly saved one, come first.
    property string noteOrder: "[]"
    function refreshNotes() {
        const all = backend.notes()
        let order = []
        try { order = JSON.parse(noteOrder) } catch (e) { order = [] }
        const place = {}
        order.forEach((path, i) => place[path] = i)
        const known = all.filter((note) => note.path in place).sort((a, b) => place[a.path] - place[b.path])
        noteList = all.filter((note) => !(note.path in place)).concat(known)
    }
    function moveNote(from, to) {
        const list = noteList.slice()
        list.splice(to, 0, list.splice(from, 1)[0])
        noteList = list
        noteOrder = JSON.stringify(list.map((note) => note.path))
    }
    Settings {
        id: panelSettings
        category: "panel"
        property bool notesVisible: true
        property alias notesWidth: win.notesWidth
        property alias noteOrder: win.noteOrder
    }
    onActiveChanged: if (active) refreshNotes()
    // A file opened from outside Documents/Yeptes starts with the panel
    // collapsed. That is not remembered: only toggling the panel is.
    property bool panelReady: false
    onNotesVisibleChanged: {
        if (notesVisible)
            refreshNotes()
        if (panelReady)
            panelSettings.notesVisible = notesVisible
    }
    Component.onCompleted: {
        notesVisible = panelSettings.notesVisible
            && !(backend.filePath !== "" && !backend.inNotesFolder(backend.filePath))
        panelReady = true
        refreshNotes()
    }
    // A cell or value being typed into keeps Escape and undo for itself.
    readonly property bool editingValue: jsonTree.editing || csvTable.editing
    // What to do once unsaved changes are saved or given up, and whether the
    // window may now close.
    property var pending: null
    property var afterSave: null
    property bool closing: false

    // Numbers take the language's digits and separators, and re-render when
    // the language changes.
    function number(value) {
        translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', 0)
    }

    // The JSON and Table views are built from the text, so they are rebuilt
    // whenever it changes under them.
    function syncView() {
        if (mode === "json")
            jsonModel.setText(backend.text)
        else if (mode === "table")
            tableModel.setText(backend.text, backend.suffix)
    }

    function openExportMenu() {
        viewMenuVisible = false
        settingsVisible = false
        languageMenuVisible = false
        exportFits = backend.fittingFormats()
        exportMenu.menuX = exportButton.mapToItem(null, 0, 0).x
        exportMenuVisible = true
    }

    function openLanguageMenu() {
        viewMenuVisible = false
        settingsVisible = false
        exportMenuVisible = false
        languageMenu.menuX = languageButton.mapToItem(null, 0, 0).x
        languageMenuVisible = true
    }

    function setMode(next) {
        viewMenuVisible = false
        settingsVisible = false
        exportMenuVisible = false
        languageMenuVisible = false
        mode = next
        backend.codeView = next === "code"
        syncView()
        focusContent()
    }

    function focusContent() {
        if (editing)
            editor.forceActiveFocus()
        else if (mode === "json")
            jsonTree.forceActiveFocus()
        else if (mode === "table")
            csvTable.forceActiveFocus()
        else
            pages.forceActiveFocus()
    }

    // Runs the action now, or after asking what to do with unsaved changes.
    function guard(action) {
        afterSave = null
        viewMenuVisible = false
        settingsVisible = false
        exportMenuVisible = false
        languageMenuVisible = false
        if (backend.modified) {
            pending = action
            confirm.empty = backend.filePath !== "" && backend.inNotesFolder(backend.filePath)
                && backend.text.trim() === ""
            confirm.open()
        } else {
            action()
        }
    }

    onClosing: (close) => {
        if (backend.modified && !win.closing) {
            close.accepted = false
            guard(() => { win.closing = true; win.close() })
        }
    }

    Connections {
        target: backend
        function onTextChanged() { win.syncView() }
        function onLoaded() {
            editor.cursorPosition = 0
            win.setMode("text")
        }
        function onNotesChanged() { win.refreshNotes() }
        function onSaved() {
            win.refreshNotes()
            const action = win.afterSave
            win.afterSave = null
            if (action)
                action()
        }
        function onMessageChanged() {
            statusBar.showMessage = backend.message !== ""
            if (backend.message !== "" && !backend.messageIsError)
                messageTimer.restart()
            else
                messageTimer.stop()
        }
    }

    // Letters and Space belong to the text, so the app's keys all carry Ctrl.
    Shortcut { sequences: ["Ctrl+/", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+B"; context: Qt.ApplicationShortcut; onActivated: win.notesVisible = !win.notesVisible }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: win.guard(() => backend.newNote()) }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: win.guard(() => backend.open()) }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveAs() }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: backend.note; onActivated: win.openExportMenu() }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: win.setMode("text") }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: win.setMode("markdown") }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; onActivated: win.setMode("json") }
    Shortcut { sequence: "Ctrl+4"; context: Qt.ApplicationShortcut; onActivated: win.setMode("html") }
    Shortcut { sequence: "Ctrl+5"; context: Qt.ApplicationShortcut; onActivated: win.setMode("table") }
    Shortcut { sequence: "Ctrl+6"; context: Qt.ApplicationShortcut; onActivated: win.setMode("code") }
    Shortcut { sequence: "Ctrl+7"; context: Qt.ApplicationShortcut; onActivated: win.setMode("diff") }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; context: Qt.ApplicationShortcut; onActivated: win.fontSize = Math.min(40, win.fontSize + 1) }
    Shortcut { sequence: "Ctrl+-"; context: Qt.ApplicationShortcut; onActivated: win.fontSize = Math.max(9, win.fontSize - 1) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: win.fontSize = 12 }
    Shortcut { sequence: "Alt+Z"; context: Qt.ApplicationShortcut; onActivated: win.wrap = !win.wrap }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !confirm.visible && (win.helpVisible || win.viewMenuVisible || win.settingsVisible || win.exportMenuVisible || win.languageMenuVisible || (win.mode !== "text" && !win.editingValue))
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (win.languageMenuVisible) {
                win.languageMenuVisible = false
                win.focusContent()
            } else if (win.exportMenuVisible) {
                win.exportMenuVisible = false
                win.focusContent()
            } else if (win.settingsVisible) {
                win.settingsVisible = false
                win.focusContent()
            } else if (win.viewMenuVisible) {
                win.viewMenuVisible = false
                win.focusContent()
            } else
                win.setMode("text")
        }
    }
    // The editor undoes on its own while it has the keys; these cover the views.
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.editing && !win.editingValue; onActivated: editor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: !win.editing && !win.editingValue; onActivated: editor.redo() }

    // The pieces of the status bar: a divider, and a label that acts as a button.
    component Divider: Rectangle {
        implicitWidth: 1
        implicitHeight: 12
        color: theme.line
    }
    // An optional line icon sits before the text: SVG path data on a 24px
    // grid, stroked in the text's color (paths from Lucide, ISC license).
    component StatusButton: Rectangle {
        id: statusButton
        property alias text: statusButtonLabel.text
        property string icon: ""
        property bool active: false
        property color textColor: theme.subtext
        // What the icon and text need; the row of buttons sizes them all alike.
        readonly property real contentWidth: statusButtonRow.implicitWidth
        // Highlighted, by the pointer or because its menu is open.
        readonly property bool lit: statusButtonHover.hovered || active
        // The button shown before this one. A divider sits between them,
        // and steps aside while either is highlighted.
        property Item previous: null
        signal clicked()
        implicitWidth: contentWidth + 12
        implicitHeight: 22
        radius: Math.min(4, theme.radius)
        color: statusButtonHover.hovered || active ? theme.raised : "transparent"

        Row {
            id: statusButtonRow
            anchors.centerIn: parent
            spacing: 5
            Item {
                visible: statusButton.icon !== ""
                width: 12
                height: 12
                anchors.verticalCenter: parent.verticalCenter
                Shape {
                    width: 24
                    height: 24
                    scale: 0.5
                    transformOrigin: Item.TopLeft
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: statusButton.textColor
                        strokeWidth: 2.2
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin
                        PathSvg { path: statusButton.icon }
                    }
                }
            }
            Label {
                id: statusButtonLabel
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 11
                color: statusButton.textColor
            }
        }
        HoverHandler { id: statusButtonHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: statusButton.clicked() }
        Divider {
            visible: statusButton.previous !== null && !statusButton.lit && !statusButton.previous.lit
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Text is written left to right whatever the interface language.
                ScrollView {
                    id: editorView
                    anchors.fill: parent
                    visible: win.editing
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true

                    TextArea {
                        id: editor
                        focus: true
                        background: null
                        leftPadding: 18
                        rightPadding: 18
                        topPadding: 14
                        bottomPadding: 14
                        font.family: win.fontFamily
                        font.pixelSize: win.fontSize
                        color: win.editorText
                        selectionColor: Qt.alpha(theme.accent, 0.35)
                        selectedTextColor: win.editorText
                        wrapMode: win.wrap ? TextArea.Wrap : TextArea.NoWrap
                        tabStopDistance: metrics.averageCharacterWidth * 4
                        selectByMouse: true
                        persistentSelection: true
                        textFormat: TextEdit.PlainText

                        FontMetrics { id: metrics; font: editor.font }
                        Component.onCompleted: {
                            backend.attachDocument(editor.textDocument)
                            editor.cursorPosition = 0
                            // The ScrollView is a focus scope of its own, so
                            // ask for the keys rather than rely on focus: true.
                            editor.forceActiveFocus()
                        }

                        // A click on the "[ ]" of a checklist line ticks it.
                        // The click has to land on the box itself, not just
                        // beside it, so placing the cursor there still works.
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: (point) => {
                                const at = point.position
                                const box = backend.checkboxAt(editor.positionAt(at.x, at.y))
                                if (box < 0)
                                    return
                                const left = editor.positionToRectangle(box)
                                const right = editor.positionToRectangle(box + 3)
                                if (at.x >= left.x && at.x <= right.x && at.y >= left.y && at.y <= left.y + left.height)
                                    backend.toggleCheckbox(box)
                            }
                        }
                    }
                }

                FocusScope {
                    id: pages
                    anchors.fill: parent
                    visible: win.mode === "markdown" || win.mode === "html" || win.mode === "diff"

                    // Rendered only while showing, so typing never pays for it.
                    Loader {
                        anchors.fill: parent
                        active: win.mode === "markdown"
                        sourceComponent: RenderedPage {
                            content: { backend.text; theme.accent; return backend.rendered("markdown", win.fontSize + 1) }
                            baseUrl: backend.baseUrl
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: win.mode === "html"
                        sourceComponent: RenderedPage {
                            content: { backend.text; theme.accent; return backend.rendered("html", win.fontSize + 1) }
                            baseUrl: backend.baseUrl
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: win.mode === "diff"
                        focus: true
                        sourceComponent: DiffList {
                            focus: true
                            fontSize: win.fontSize
                            note: backend.note
                            rows: { backend.text; backend.modified; backend.historyCount; return backend.diffRows() }
                        }
                    }
                }

                JsonTree {
                    id: jsonTree
                    anchors.fill: parent
                    visible: win.mode === "json"
                    fontSize: win.fontSize
                }

                CsvTable {
                    id: csvTable
                    anchors.fill: parent
                    visible: win.mode === "table"
                    fontSize: win.fontSize
                }

                DropArea {
                    anchors.fill: parent
                    onDropped: (drop) => {
                        if (!drop.hasUrls)
                            return
                        const url = drop.urls[0]
                        win.guard(() => backend.loadUrl(url))
                    }
                }
            }

                Rectangle {
                    id: notesHandle
                    visible: win.notesVisible
                    Layout.fillHeight: true
                    // A hairline to look at, with a wider strip to grab that
                    // overhangs both sides; it sits above them so it gets the pointer.
                    Layout.preferredWidth: 1
                    z: 2
                    color: handleDrag.active || handleHover.hovered ? theme.line : Qt.alpha(theme.line, 0.45)
                    Item {
                        anchors.centerIn: parent
                        width: 7
                        height: parent.height
                        HoverHandler { id: handleHover; cursorShape: Qt.SplitHCursor }
                        DragHandler {
                            id: handleDrag
                            target: null
                            cursorShape: Qt.SplitHCursor
                            property real startWidth: 0
                            onActiveChanged: if (active) startWidth = win.notesWidth
                            onTranslationChanged: {
                                if (!active)
                                    return
                                // The panel is on the right, so dragging left widens it
                                // (the other way round in a mirrored window).
                                const sign = Qt.application.layoutDirection === Qt.RightToLeft ? 1 : -1
                                win.notesWidth = Math.max(160, Math.min(win.width * 0.6, startWidth + sign * translation.x))
                            }
                        }
                    }
                }

                // With the panel collapsed, a slim rail keeps the buttons that
                // bring it back and start a new note, so no text runs underneath.
                Rectangle {
                    visible: !win.notesVisible
                    Layout.fillHeight: true
                    Layout.preferredWidth: 28
                    color: theme.chrome
                    Column {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 4
                        spacing: 2
                        PanelButton { text: "«"; tip: qsTr("Show notes"); onClicked: win.notesVisible = true }
                        PanelButton { text: "+"; tip: qsTr("New note (Ctrl+N)"); onClicked: win.guard(() => backend.newNote()) }
                    }
                }

                NotesPanel {
                    visible: win.notesVisible
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.max(160, Math.min(win.notesWidth, win.width * 0.6))
                    notes: win.noteList
                    fontFamily: win.fontFamily
                    current: backend.filePath
                    onPicked: (path) => win.guard(() => backend.load(path))
                    onCollapseRequested: win.notesVisible = false
                    onNewRequested: win.guard(() => backend.newNote())
                    onDeleteRequested: (path) => backend.deleteNote(path)
                    onMoved: (from, to) => win.moveNote(from, to)
                }
            }

            Rectangle {
                id: statusBar
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                color: theme.chrome
                // A note like "Saved" steps aside for the path again; an
                // error stays until the next action.
                property bool showMessage: false
                Timer { id: messageTimer; interval: 4000; onTriggered: statusBar.showMessage = false }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 0
                    // Puts the cog's middle 14px from the edge, in line with the
                    // buttons on the collapsed notes rail (28px wide) above it.
                    anchors.rightMargin: 4
                    spacing: 8

                    // The buttons sit side by side, all as wide as the widest one.
                    Row {
                        id: statusButtons
                        readonly property real buttonWidth: Math.max(viewButton.contentWidth,
                            languageButton.visible ? languageButton.contentWidth : 0,
                            exportButton.visible ? exportButton.contentWidth : 0,
                            openButton.contentWidth) + 12
                        StatusButton {
                            id: viewButton
                            implicitWidth: statusButtons.buttonWidth
                            text: win.modeLabel
                            icon: "M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z M14 2v4a2 2 0 0 0 2 2h4 M16 13H8 M16 17H8 M10 9H8"
                            active: win.viewMenuVisible
                            onClicked: { win.settingsVisible = false; win.exportMenuVisible = false; win.languageMenuVisible = false; win.viewMenuVisible = !win.viewMenuVisible }
                        }
                        StatusButton {
                            id: languageButton
                            previous: viewButton
                            implicitWidth: statusButtons.buttonWidth
                            visible: win.mode === "code"
                            text: backend.codeLanguage !== "" ? backend.codeLanguageName : qsTr("Plain text")
                            icon: "M16 18l6-6-6-6 M8 6l-6 6 6 6"
                            active: win.languageMenuVisible
                            onClicked: win.languageMenuVisible ? win.languageMenuVisible = false : win.openLanguageMenu()
                        }
                        StatusButton {
                            id: exportButton
                            previous: languageButton.visible ? languageButton : viewButton
                            implicitWidth: statusButtons.buttonWidth
                            visible: backend.note
                            text: qsTr("Export")
                            icon: "M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4 M17 8l-5-5-5 5 M12 3v12"
                            active: win.exportMenuVisible
                            onClicked: win.exportMenuVisible ? win.exportMenuVisible = false : win.openExportMenu()
                        }
                        StatusButton {
                            id: openButton
                            previous: exportButton.visible ? exportButton : languageButton.visible ? languageButton : viewButton
                            implicitWidth: statusButtons.buttonWidth
                            text: qsTr("Open")
                            icon: "M6 14l1.5-2.9A2 2 0 0 1 9.24 10H20a2 2 0 0 1 1.94 2.5l-1.54 6a2 2 0 0 1-1.95 1.5H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h3.9a2 2 0 0 1 1.69.9l.81 1.2a2 2 0 0 0 1.67.9H18a2 2 0 0 1 2 2v2"
                            onClicked: win.guard(() => backend.open())
                        }
                        // Right against Open, and out of the way while it is lit.
                        Divider {
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: openButton.lit ? 0 : 1
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: statusBar.showMessage && backend.message !== "" ? backend.message
                            : (backend.fileName !== "" ? backend.fileName : qsTr("Not saved yet"))
                              + (backend.modified ? " *" : "")
                        color: statusBar.showMessage && backend.messageIsError ? theme.red : theme.dim
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                    Label {
                        text: qsTr("%1 shortcuts").arg("Ctrl+/")
                        color: theme.faint
                        font.family: win.fontFamily
                        font.pixelSize: 10
                    }
                    Divider { visible: cursorLabel.visible }
                    // The line sits at the left and the column at the right of a
                    // space with room for two digits each, so neither twitches
                    // while typing; it only grows past that. The translated
                    // text is split after the line's comma.
                    Item {
                        id: cursorLabel
                        visible: win.editing
                        readonly property var at: backend.cursorInfo(editor.cursorPosition)
                        readonly property string template: qsTr("Ln %1, Col %2")
                        readonly property int split: template.indexOf(",", template.indexOf("%1")) + 1
                        implicitHeight: lineLabel.implicitHeight
                        Layout.preferredWidth: Math.max(lineLabel.implicitWidth + columnLabel.implicitWidth + 6,
                                                        cursorRoom.advanceWidth)
                        Label {
                            id: lineLabel
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: cursorLabel.split > 0 ? cursorLabel.template.slice(0, cursorLabel.split).arg(win.number(cursorLabel.at.line))
                                : cursorLabel.template.arg(win.number(cursorLabel.at.line)).arg(win.number(cursorLabel.at.column))
                            color: theme.faint
                            font.family: win.fontFamily
                            font.pixelSize: 10
                        }
                        Label {
                            id: columnLabel
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: cursorLabel.split > 0
                            text: cursorLabel.template.slice(cursorLabel.split).trim().arg(win.number(cursorLabel.at.column))
                            color: theme.faint
                            font: lineLabel.font
                        }
                        TextMetrics {
                            id: cursorRoom
                            font: lineLabel.font
                            text: cursorLabel.template.arg(win.number(88)).arg(win.number(88))
                        }
                    }
                    Divider {}
                    Label {
                        id: countLabel
                        text: qsTr("%n Chars", "", backend.characterCount)
                        color: theme.faint
                        font.family: win.fontFamily
                        font.pixelSize: 10
                        // Room for three digits before it grows, counting from the right.
                        Layout.preferredWidth: Math.max(implicitWidth, countRoom.advanceWidth)
                        horizontalAlignment: Text.AlignRight
                        TextMetrics {
                            id: countRoom
                            font: countLabel.font
                            text: qsTr("%n Chars", "", 888)
                        }
                    }
                    // Drawn rather than typed, so a color emoji font cannot tint it.
                    Rectangle {
                        implicitWidth: 20
                        implicitHeight: 22
                        radius: Math.min(4, theme.radius)
                        color: cogHover.hovered || win.settingsVisible ? theme.raised : "transparent"
                        Canvas {
                            readonly property color tint: theme.dim
                            onTintChanged: requestPaint()
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                // Drawn on a 14px grid, shrunk to fit.
                                ctx.scale(width / 14, height / 14)
                                ctx.fillStyle = tint
                                ctx.beginPath()
                                const teeth = 8
                                for (let i = 0; i < teeth * 2; ++i) {
                                    const angle = Math.PI * i / teeth - Math.PI / (teeth * 2) * 0
                                    const r = (i % 2 === 0) ? 6.8 : 5.0
                                    // Each tooth is a short flat run on the outer radius.
                                    const a1 = angle - Math.PI / (teeth * 4)
                                    const a2 = angle + Math.PI / (teeth * 4)
                                    ctx.lineTo(7 + r * Math.cos(a1), 7 + r * Math.sin(a1))
                                    ctx.lineTo(7 + r * Math.cos(a2), 7 + r * Math.sin(a2))
                                }
                                ctx.closePath()
                                ctx.moveTo(7 + 2.2, 7)
                                ctx.arc(7, 7, 2.2, 0, Math.PI * 2, true)
                                ctx.fill("evenodd")
                            }
                        }
                        HoverHandler { id: cogHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: { win.viewMenuVisible = false; win.exportMenuVisible = false; win.languageMenuVisible = false; win.settingsVisible = !win.settingsVisible } }
                    }
                }
            }
        }

        ViewMenu {
            anchors.fill: parent
            visible: win.viewMenuVisible
            views: win.modes
            current: win.mode
            onPicked: (id) => win.setMode(id)
            onDismissed: { win.viewMenuVisible = false; win.focusContent() }
        }

        ViewMenu {
            id: exportMenu
            anchors.fill: parent
            visible: win.exportMenuVisible
            views: win.exportViews
            current: "\u0000"
            onPicked: (id) => { win.exportMenuVisible = false; backend.exportAs(id) }
            onDismissed: { win.exportMenuVisible = false; win.focusContent() }
        }

        LanguageMenu {
            id: languageMenu
            anchors.fill: parent
            visible: win.languageMenuVisible
            languages: win.languageViews
            current: backend.codeLanguage
            onPicked: (id) => { win.languageMenuVisible = false; backend.codeLanguage = id; win.focusContent() }
            onDismissed: { win.languageMenuVisible = false; win.focusContent() }
        }

        SettingsMenu {
            anchors.fill: parent
            visible: win.settingsVisible
            fontSize: win.fontSize
            fontFamily: win.fontFamily
            backgroundColor: win.editorBackground
            textColor: win.editorText
            themeBackground: win.themeBackground
            themeText: win.themeText
            onSizeChanged: (size) => win.fontSize = size
            onFamilyPicked: (family) => win.fontFamily = family
            onBackgroundPicked: (value) => { win.themeBackground = false; win.backgroundColor = value }
            onTextPicked: (value) => { win.themeText = false; win.textColor = value }
            onThemeBackgroundPicked: win.themeBackground = true
            onThemeTextPicked: win.themeText = true
            onResetRequested: {
                win.fontSize = 12
                win.fontFamily = "monospace"
                win.themeBackground = true
                win.themeText = true
            }
            onDismissed: { win.settingsVisible = false; win.focusContent() }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onDismissed: win.helpVisible = false
        }

        ConfirmOverlay {
            id: confirm
            anchors.fill: parent
            visible: false
            name: win.displayName
            function open() { visible = true }
            function close() { visible = false; win.focusContent() }
            onCancelled: { win.pending = null; close() }
            onDeleteChosen: {
                const action = win.pending
                win.pending = null
                close()
                // Deleting the open note leaves a new, empty one, so the
                // action runs as if nothing were unsaved.
                if (backend.deleteNote(backend.filePath) && action)
                    action()
            }
            onDiscardChosen: {
                const action = win.pending
                win.pending = null
                close()
                if (action)
                    action()
            }
            onSaveChosen: {
                // The action runs when the save lands, which for a new note
                // is after the save dialog.
                win.afterSave = win.pending
                win.pending = null
                close()
                backend.save()
            }
        }
    }
}
