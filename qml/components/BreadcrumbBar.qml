pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    required property string path
    required property var themeObject
    signal navigateRequested(string path)
    signal editRequested()

    readonly property real uiScale: themeObject.effectiveSpacingScale
    property var segments: buildSegments(path)

    function px(value) {
        return Math.max(1, Math.round(value * uiScale))
    }

    function alpha(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    function buildSegments(value) {
        if (!value || value === "/")
            return [{ label: "/", path: "/" }]

        var result = [{ label: "/", path: "/" }]
        var parts = value.split("/").filter(function(part) { return part.length > 0 })
        var current = ""
        for (var index = 0; index < parts.length; ++index) {
            current += "/" + parts[index]
            result.push({ label: parts[index], path: current })
        }
        return result
    }

    onPathChanged: Qt.callLater(function() {
        trail.contentX = Math.max(0, trail.contentWidth - trail.width)
    })

    Rectangle {
        anchors.fill: parent
        color: root.themeObject.darkBackground
    }

    Flickable {
        id: trail
        anchors.fill: parent
        anchors.leftMargin: root.px(6)
        anchors.rightMargin: root.px(6)
        clip: true
        contentWidth: crumbs.implicitWidth
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width

        Row {
            id: crumbs
            height: trail.height
            spacing: 0

            Repeater {
                model: root.segments

                Row {
                    id: crumb
                    required property var modelData
                    required property int index
                    height: trail.height

                    Text {
                        visible: crumb.index > 0
                        height: parent.height
                        width: root.px(18)
                        text: "›"
                        color: root.themeObject.muted
                        font.family: root.themeObject.fontFamily
                        font.pixelSize: root.themeObject.fontSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Rectangle {
                        id: segment
                        height: parent.height
                        width: label.implicitWidth + root.px(16)
                        radius: root.themeObject.cornerRadius
                        color: hover.containsMouse
                               ? root.alpha(root.themeObject.foreground, root.themeObject.hoverFillAlpha)
                               : "transparent"

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: crumb.modelData.label
                            color: root.themeObject.foreground
                            font.family: root.themeObject.fontFamily
                            font.pixelSize: root.themeObject.fontSize
                            elide: Text.ElideMiddle
                        }

                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.navigateRequested(crumb.modelData.path)
                            onDoubleClicked: root.editRequested()
                        }
                    }
                }
            }
        }
    }
}
