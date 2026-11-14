import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: win
    width: 1000
    height: 720
    minimumWidth: 560
    minimumHeight: 400
    visible: true
    title: qsTr("%1 — Yeptes").arg(win.displayName + (backend.modified ? " •" : ""))
    color: win.backgroundColor

    Material.theme: Material.Dark
    Material.accent: theme.accent
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
    property color backgroundColor: "#1a1b26"
    property color textColor: "#c0caf5"

    // Text appearance is remembered between runs.
    Settings {
        category: "appearance"
        property alias fontSize: win.fontSize
        property alias fontFamily: win.fontFamily
        property alias backgroundColor: win.backgroundColor
        property alias textColor: win.textColor
    }
    property bool wrap: true

    // The notes panel on the right: shown or collapsed, and how wide.
    property bool notesVisible: true
    property real notesWidth: 240
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
    component Divider: Label {
        text: "|"
        color: "#3a3a44"
        font.pixelSize: 12
    }
    component StatusButton: Rectangle {
        id: statusButton
        property alias text: statusButtonLabel.text
        property bool active: false
        property color textColor: "#c9c9d1"
        signal clicked()
        implicitWidth: statusButtonLabel.implicitWidth + 16
        implicitHeight: 22
        radius: Math.min(4, theme.radius)
        color: statusButtonHover.hovered || active ? "#26262d" : "transparent"

        Label {
            id: statusButtonLabel
            anchors.centerIn: parent
            font.pixelSize: 12
            color: statusButton.textColor
        }
        HoverHandler { id: statusButtonHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: statusButton.clicked() }
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
                        color: win.textColor
                        selectionColor: Qt.alpha(theme.accent, 0.35)
                        selectedTextColor: "#ffffff"
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
                    Layout.preferredWidth: 5
                    color: handleDrag.active || handleHover.hovered ? "#2a2a32" : "#1d1d22"
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

                // With the panel collapsed, a slim rail keeps the button that
                // brings it back, so no text runs underneath it.
                Rectangle {
                    visible: !win.notesVisible
                    Layout.fillHeight: true
                    Layout.preferredWidth: 38
                    color: "#141417"
                    Rectangle {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 6
                        width: 28
                        height: 26
                        radius: Math.min(4, theme.radius)
                        color: expandHover.hovered ? "#26262d" : "transparent"
                        Label { anchors.centerIn: parent; text: "«"; color: "#c9c9d1"; font.pixelSize: 15 }
                        HoverHandler { id: expandHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: win.notesVisible = true }
                    }
                }

                NotesPanel {
                    visible: win.notesVisible
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.max(160, Math.min(win.notesWidth, win.width * 0.6))
                    notes: win.noteList
                    current: backend.filePath
                    onPicked: (path) => win.guard(() => backend.load(path))
                    onCollapseRequested: win.notesVisible = false
                    onDeleteRequested: (path) => backend.deleteNote(path)
                    onMoved: (from, to) => win.moveNote(from, to)
                }
            }

            Rectangle {
                id: statusBar
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                color: "#141417"
                // A note like "Saved" steps aside for the path again; an
                // error stays until the next action.
                property bool showMessage: false
                Timer { id: messageTimer; interval: 4000; onTriggered: statusBar.showMessage = false }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 8

                    StatusButton {
                        text: win.modeLabel + "  ▴"
                        active: win.viewMenuVisible
                        onClicked: { win.settingsVisible = false; win.exportMenuVisible = false; win.languageMenuVisible = false; win.viewMenuVisible = !win.viewMenuVisible }
                    }
                    Divider { visible: win.mode === "code" }
                    StatusButton {
                        id: languageButton
                        visible: win.mode === "code"
                        text: (backend.codeLanguage !== "" ? backend.codeLanguageName : qsTr("Plain text")) + "  ▴"
                        active: win.languageMenuVisible
                        onClicked: win.languageMenuVisible ? win.languageMenuVisible = false : win.openLanguageMenu()
                    }
                    Divider { visible: backend.note }
                    StatusButton {
                        id: exportButton
                        visible: backend.note
                        text: qsTr("Export as…")
                        active: win.exportMenuVisible
                        onClicked: win.exportMenuVisible ? win.exportMenuVisible = false : win.openExportMenu()
                    }
                    Divider { visible: backend.note }
                    StatusButton {
                        text: qsTr("Open…")
                        onClicked: win.guard(() => backend.open())
                    }
                    Divider {}
                    Label {
                        Layout.fillWidth: true
                        text: statusBar.showMessage && backend.message !== "" ? backend.message
                            : (backend.fileName !== "" ? backend.fileName : qsTr("Not saved yet"))
                              + (backend.modified ? " *" : "")
                        color: statusBar.showMessage && backend.messageIsError ? "#f7768e" : "#8b8b96"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    Divider {}
                    Label {
                        text: qsTr("%1 for shortcuts").arg("Ctrl+/")
                        color: "#8b8b96"
                        font.pixelSize: 12
                    }
                    Divider { visible: cursorLabel.visible }
                    Label {
                        id: cursorLabel
                        visible: win.editing
                        readonly property var at: backend.cursorInfo(editor.cursorPosition)
                        text: qsTr("Ln %1, Col %2").arg(win.number(at.line)).arg(win.number(at.column))
                        color: "#8b8b96"
                        font.pixelSize: 12
                    }
                    Divider {}
                    Label {
                        text: qsTr("%n characters", "", backend.characterCount)
                        color: "#8b8b96"
                        font.pixelSize: 12
                    }
                    // Drawn rather than typed, so a color emoji font cannot tint it.
                    Rectangle {
                        implicitWidth: 30
                        implicitHeight: 22
                        radius: Math.min(4, theme.radius)
                        color: cogHover.hovered || win.settingsVisible ? "#26262d" : "transparent"
                        Canvas {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                ctx.fillStyle = "#8b8b96"
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
            backgroundColor: win.backgroundColor
            textColor: win.textColor
            onSizeChanged: (size) => win.fontSize = size
            onFamilyPicked: (family) => win.fontFamily = family
            onBackgroundPicked: (value) => win.backgroundColor = value
            onTextPicked: (value) => win.textColor = value
            onResetRequested: {
                win.fontSize = 12
                win.fontFamily = "monospace"
                win.backgroundColor = "#1a1b26"
                win.textColor = "#c0caf5"
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
