pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    required property var themeObject
    required property string modeName
    required property bool trashView
    signal closeRequested()

    function alpha(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    Rectangle {
        anchors.fill: parent
        color: root.alpha(root.themeObject.darkBackground, 0.78)

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 720)
        height: Math.min(parent.height - 48, content.implicitHeight + 36)
        color: root.themeObject.background
        radius: root.themeObject.cornerRadius
        border.width: 1
        border.color: root.themeObject.accent

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 11

            Text {
                text: "SHIBUI · " + root.modeName + " MODE"
                color: root.themeObject.accent
                font.family: root.themeObject.fontFamily
                font.pixelSize: root.themeObject.fontSize
                font.bold: true
            }

            Rectangle {
                width: parent.width
                height: 1
                color: root.alpha(root.themeObject.foreground, 0.18)
            }

            Grid {
                id: bindings
                width: parent.width
                columns: 2
                columnSpacing: 24
                rowSpacing: 9

                Repeater {
                    model: [
                        { key: "j / k", action: "next / previous item" },
                        { key: "h / l / Enter", action: "parent / open item" },
                        { key: "[ / ]", action: "back / forward location" },
                        { key: "t / Ctrl-t", action: "folder / current location in new tab" },
                        { key: "gt / gT", action: "next / previous tab" },
                        { key: "Ctrl-w", action: "close current tab" },
                        { key: "b / B", action: "use / hide Places" },
                        { key: "Enter on Recent", action: "open desktop recent files" },
                        { key: "m", action: "bookmark current folder" },
                        { key: "a / r / d", action: "add / rename / remove in Places" },
                        { key: "J / K", action: "move bookmark in Places" },
                        { key: "u / e in Places", action: "unmount / eject device" },
                        { key: "Ctrl-k / c in Places", action: "connect to network location" },
                        { key: "x in Places", action: "disconnect network location" },
                        { key: "gg / G", action: "first / last item" },
                        { key: "Ctrl-d / Ctrl-u", action: "half page down / up" },
                        { key: "Space", action: "quick preview" },
                        { key: "o", action: "open with another application" },
                        { key: "z / Alt-Enter", action: "show properties" },
                        { key: "1–9 in Properties", action: "toggle permission bits" },
                        { key: "x / Ctrl-Space", action: "toggle selected item" },
                        { key: "v", action: "Visual range selection" },
                        { key: "Ctrl-a", action: "select all" },
                        { key: "Esc", action: "cancel cut / leave mode" },
                        { key: "n / Ctrl-Shift-n", action: "create folder" },
                        { key: "N", action: "new file from Templates" },
                        { key: "gn", action: "new folder with selected items" },
                        { key: "r / F2", action: root.trashView ? "restore selection" : "rename one item" },
                        { key: "R", action: "bulk rename selected items with preview" },
                        { key: "yy / Ctrl-c", action: "copy selection" },
                        { key: "yp / !", action: "copy path / terminal here" },
                        { key: "ac / ax", action: "create archive / extract here" },
                        { key: "dd / Ctrl-x", action: "cut selection" },
                        { key: "D", action: "move selection to Trash" },
                        { key: "T", action: "open Trash" },
                        { key: "E", action: "empty Trash (confirmed)" },
                        { key: "p / Ctrl-v", action: "paste here" },
                        { key: "u", action: "undo last mutation" },
                        { key: "r / s / n", action: "replace / skip / rename conflict" },
                        { key: "/", action: "filter this directory" },
                        { key: "f / Ctrl-p", action: "find anywhere below here" },
                        { key: "Alt-t / Alt-d", action: "finder type / date filter" },
                        { key: "Ctrl / Alt-Enter", action: "reveal / properties in finder" },
                        { key: "i / Ctrl-1 / 2", action: "toggle / choose list or grid" },
                        { key: ": / Ctrl-l", action: "edit location" },
                        { key: ".", action: "toggle hidden files" },
                        { key: "s / S", action: "next sort / reverse sort" },
                        { key: "~", action: "home directory" },
                        { key: "Ctrl-r", action: "refresh" },
                        { key: "?", action: "close this reference" }
                    ]

                    Row {
                        required property var modelData
                        width: (bindings.width - bindings.columnSpacing) / 2
                        height: Math.max(keyText.implicitHeight, actionText.implicitHeight)

                        Text {
                            id: keyText
                            width: Math.min(120, parent.width * 0.42)
                            text: parent.modelData.key
                            color: root.themeObject.foreground
                            font.family: root.themeObject.fontFamily
                            font.pixelSize: root.themeObject.fontSize
                        }

                        Text {
                            id: actionText
                            width: parent.width - keyText.width
                            text: parent.modelData.action
                            color: root.themeObject.muted
                            font.family: root.themeObject.fontFamily
                            font.pixelSize: root.themeObject.fontSize
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: "Text prompts own keyboard input. Conflict choices are shown when a transfer pauses; a applies Replace or Skip to the remaining conflicts."
                color: root.themeObject.muted
                font.family: root.themeObject.fontFamily
                font.pixelSize: Math.max(9, root.themeObject.fontSize - 1)
                wrapMode: Text.WordWrap
            }
        }
    }
}
