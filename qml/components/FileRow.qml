pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: root

    required property int index
    required property string name
    required property string filePath
    required property bool isDirectory
    required property bool isSymlink
    required property bool isBrokenSymlink
    required property string sizeText
    required property string typeText
    required property string modifiedText
    required property string iconSource
    required property bool cursor
    required property bool selected
    required property bool pendingMove
    required property var dragPathsProvider
    required property bool acceptsDrop
    required property string dropDestination
    required property var dropHandler
    required property var themeObject
    required property real nameColumnWidth
    required property real sizeColumnWidth
    required property real typeColumnWidth
    required property real modifiedColumnWidth

    signal chosen(int row, int modifiers)
    signal activated(int row)
    signal contextRequested(int row, real x, real y)
    signal newTabRequested(int row)
    signal dropEntered(string destination, bool move)
    signal dropExited()
    signal dragStateChanged(bool active, bool move)

    function alpha(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    function fileUrl(path) {
        return "file://" + encodeURIComponent(path).replace(/%2F/g, "/")
    }

    property var dragPaths: [filePath]
    readonly property string dragUriList: {
        var urls = []
        for (var index = 0; index < dragPaths.length; ++index)
            urls.push(fileUrl(dragPaths[index]))
        return urls.join("\r\n") + "\r\n"
    }
    property bool dragMove: true

    Drag.dragType: Drag.Automatic
    Drag.active: dragHandler.active
    Drag.hotSpot: Qt.point(width / 2, height / 2)
    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
    Drag.proposedAction: dragMove ? Qt.MoveAction : Qt.CopyAction
    Drag.imageSource: root.iconSource && root.iconSource !== "undefined" ? root.iconSource : ""
    Drag.mimeData: ({
        "text/uri-list": dragUriList,
        "x-special/gnome-copied-files": (dragMove ? "cut\n" : "copy\n") + dragUriList
    })
    Drag.onDragStarted: root.dragStateChanged(true, root.dragMove)
    Drag.onDragFinished: function(dropAction) {
        root.dragStateChanged(false, dropAction === Qt.MoveAction)
    }

    color: pendingMove
           ? alpha(themeObject.foreground, 0.025)
           : (selected
              ? themeObject.selection
              : (cursor
                 ? alpha(themeObject.foreground, themeObject.hoverFillAlpha)
                 : (pointer.containsMouse
                    ? alpha(themeObject.foreground, 0.035)
                    : "transparent")))
    border.width: dropTarget.containsDrag || cursor ? 1 : 0
    border.color: dropTarget.containsDrag
                  ? themeObject.accent
                  : (cursor
                  ? alpha(themeObject.foreground, themeObject.hoverBorderAlpha)
                  : "transparent")
    radius: themeObject.cornerRadius

    Behavior on color {
        ColorAnimation { duration: 60 }
    }

    Rectangle {
        visible: root.cursor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 2
        color: root.themeObject.accent
        radius: root.themeObject.cornerRadius
    }

    Image {
        id: icon
        x: Math.round(10 * root.themeObject.effectiveSpacingScale)
        anchors.verticalCenter: parent.verticalCenter
        width: Math.round(20 * root.themeObject.effectiveSpacingScale)
        height: width
        source: root.iconSource && root.iconSource !== "undefined" ? root.iconSource : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        smooth: true
        opacity: root.pendingMove ? 0.45 : 1
    }

    Text {
        id: nameText
        x: icon.x + icon.width + Math.round(9 * root.themeObject.effectiveSpacingScale)
        width: Math.max(20, root.nameColumnWidth - x - Math.round(10 * root.themeObject.effectiveSpacingScale))
        height: parent.height
        text: root.name
        color: root.isBrokenSymlink
               ? root.themeObject.errorColor
               : (root.pendingMove ? root.themeObject.muted : root.themeObject.foreground)
        font.family: root.themeObject.fontFamily
        font.pixelSize: root.themeObject.fontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: root.pendingMove ? 0.62 : 1
    }

    Text {
        x: root.nameColumnWidth
        width: root.sizeColumnWidth - Math.round(12 * root.themeObject.effectiveSpacingScale)
        height: parent.height
        text: root.sizeText
        color: root.themeObject.muted
        font.family: root.themeObject.fontFamily
        font.pixelSize: root.themeObject.fontSize
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: root.pendingMove ? 0.55 : 1
    }

    Text {
        x: root.nameColumnWidth + root.sizeColumnWidth + Math.round(18 * root.themeObject.effectiveSpacingScale)
        width: root.typeColumnWidth - Math.round(24 * root.themeObject.effectiveSpacingScale)
        height: parent.height
        text: root.typeText
        color: root.themeObject.muted
        font.family: root.themeObject.fontFamily
        font.pixelSize: root.themeObject.fontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: root.pendingMove ? 0.55 : 1
    }

    Text {
        x: root.nameColumnWidth + root.sizeColumnWidth + root.typeColumnWidth
           + Math.round(12 * root.themeObject.effectiveSpacingScale)
        width: root.modifiedColumnWidth - Math.round(22 * root.themeObject.effectiveSpacingScale)
        height: parent.height
        text: root.modifiedText
        color: root.themeObject.muted
        font.family: root.themeObject.fontFamily
        font.pixelSize: root.themeObject.fontSize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: root.pendingMove ? 0.55 : 1
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton)
                root.contextRequested(root.index, mouse.x, mouse.y)
            else if (mouse.button === Qt.MiddleButton)
                root.newTabRequested(root.index)
            else
                root.chosen(root.index, mouse.modifiers)
        }
        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton)
                root.activated(root.index)
        }
    }

    DragHandler {
        id: dragHandler
        enabled: root.filePath.length > 0
        target: null
        onActiveChanged: {
            if (active) {
                root.dragPaths = root.dragPathsProvider()
                root.dragMove = (centroid.modifiers & Qt.ControlModifier) === 0
            } else {
                root.dragPaths = [root.filePath]
            }
        }
    }

    DropArea {
        id: dropTarget
        anchors.fill: parent
        enabled: root.acceptsDrop
        onEntered: function(drag) {
            var move = drag.proposedAction === Qt.MoveAction
            drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
            root.dropEntered(root.dropDestination, move)
        }
        onPositionChanged: function(drag) {
            var move = drag.proposedAction === Qt.MoveAction
            drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
            root.dropEntered(root.dropDestination, move)
        }
        onExited: root.dropExited()
        onDropped: function(drop) {
            var move = drop.proposedAction === Qt.MoveAction
            if (root.dropHandler(drop.urls, root.dropDestination, move))
                drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
            else
                drop.accepted = false
            root.dropExited()
        }
    }
}
