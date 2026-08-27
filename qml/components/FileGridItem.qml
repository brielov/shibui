pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: root

    required property int index
    required property string name
    required property string filePath
    required property bool isDirectory
    required property bool isBrokenSymlink
    required property string iconSource
    required property string thumbnailSource
    required property bool cursor
    required property bool selected
    required property bool pendingMove
    required property var dragPathsProvider
    required property bool acceptsDrop
    required property string dropDestination
    required property var dropHandler
    required property var themeObject

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
    Drag.imageSource: root.thumbnailSource && root.thumbnailSource !== "undefined"
                      ? root.thumbnailSource
                      : (root.iconSource && root.iconSource !== "undefined"
                         ? root.iconSource : "")
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
                    ? alpha(themeObject.foreground, 0.035) : "transparent")))
    border.width: dropTarget.containsDrag || cursor ? 1 : 0
    border.color: dropTarget.containsDrag
                  ? themeObject.accent
                  : alpha(themeObject.foreground, themeObject.hoverBorderAlpha)
    radius: themeObject.cornerRadius

    Image {
        id: icon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.round(12 * root.themeObject.effectiveSpacingScale)
        width: Math.round(52 * root.themeObject.effectiveSpacingScale)
        height: width
        source: root.thumbnailSource && root.thumbnailSource !== "undefined"
                ? root.thumbnailSource
                : (root.iconSource && root.iconSource !== "undefined" ? root.iconSource : "")
        sourceSize: Qt.size(width, height)
        fillMode: root.thumbnailSource && root.thumbnailSource !== "undefined"
                  ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        smooth: true
        opacity: root.pendingMove ? 0.45 : 1
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: icon.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: Math.round(7 * root.themeObject.effectiveSpacingScale)
        anchors.rightMargin: Math.round(7 * root.themeObject.effectiveSpacingScale)
        anchors.topMargin: Math.round(6 * root.themeObject.effectiveSpacingScale)
        text: root.name
        color: root.isBrokenSymlink ? root.themeObject.errorColor
                                    : (root.pendingMove ? root.themeObject.muted
                                                        : root.themeObject.foreground)
        font.family: root.themeObject.fontFamily
        font.pixelSize: Math.max(9, root.themeObject.fontSize - 1)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
        opacity: root.pendingMove ? 0.62 : 1
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
