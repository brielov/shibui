pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import "components"

Window {
    id: window

    width: 1080
    height: 700
    minimumWidth: Math.round(844 * uiScale)
    minimumHeight: 440
    visible: true
    color: theme.background
    title: fileModel.trashView
           ? "Trash — Shibui"
           : (fileModel.currentPath.length > 0 ? fileModel.currentPath + " — Shibui" : "Shibui")

    readonly property real uiScale: theme.effectiveSpacingScale
    readonly property int topHeight: Math.round(34 * uiScale)
    readonly property int columnHeaderHeight: Math.round(29 * uiScale)
    readonly property int statusHeight: Math.round(27 * uiScale)
    readonly property int tabBarHeight: root.tabs.length > 1 ? Math.round(27 * uiScale) : 0
    readonly property int transferHeight: Math.round(46 * uiScale)
    readonly property int rowHeight: Math.round(34 * uiScale)
    readonly property real sizeColumnWidth: Math.round(96 * uiScale)
    readonly property real typeColumnWidth: Math.round(178 * uiScale)
    readonly property real modifiedColumnWidth: Math.round(190 * uiScale)
    readonly property real sidebarWidth: root.sidebarVisible ? Math.round(190 * uiScale) : 0
    readonly property real contentWidth: width - sidebarWidth
    readonly property real nameColumnWidth: contentWidth - sizeColumnWidth - typeColumnWidth - modifiedColumnWidth
    readonly property int currentItemIndex: files.currentIndex
    readonly property bool filterModeActive: root.filterMode
    readonly property bool filterPromptFocused: filterInput.activeFocus
    readonly property bool operationPromptFocused: operationInput.activeFocus
    readonly property string operationPromptText: operationInput.text
    readonly property bool locationPromptFocused: locationInput.activeFocus
    readonly property string locationPromptText: locationInput.text
    readonly property bool keyReferenceVisible: root.helpVisible
    readonly property string pendingSequence: root.pendingKey
    readonly property int selectedItemCount: root.selectedCount
    readonly property bool visualModeActive: root.visualMode
    readonly property int clipboardItemCount: root.clipboardPaths.length
    readonly property string clipboardOperation: root.clipboardMode
    readonly property bool conflictPromptVisible: fileModel.transferConflictActive
    readonly property bool conflictRenamePromptFocused: conflictRenameInput.activeFocus
    readonly property bool trashConfirmationVisible: root.trashPromptVisible
    readonly property bool emptyTrashConfirmationVisible: root.emptyTrashPromptVisible
    readonly property bool emptyTrashPromptFocused: emptyTrashInput.activeFocus
    readonly property bool restoreOperationActive: root.restoreInProgress
    readonly property bool contextMenuVisible: contextMenu.visible
    readonly property bool dragOperationActive: root.dragActive
    readonly property string dropOperationIntent: root.dropIntent
    readonly property bool historyBackAvailable: root.backHistory.length > 0
    readonly property bool historyForwardAvailable: root.forwardHistory.length > 0
    readonly property bool placesSidebarVisible: root.sidebarVisible
    readonly property bool placesModeActive: root.placesMode
    readonly property int activePlaceIndex: root.activePlaceIndex
    readonly property int placesCurrentIndex: places.currentIndex
    readonly property bool bookmarkPromptFocused: bookmarkRenameInput.activeFocus
    readonly property bool finderModeActive: root.finderMode
    readonly property bool recentModeActive: root.recentMode
    readonly property bool finderPromptFocused: finderInput.activeFocus
    readonly property string finderPromptText: finderInput.text
    readonly property int finderResultCount: searchModel.count
    readonly property int finderCurrentIndex: finderResults.currentIndex
    readonly property int tabCount: root.tabs.length
    readonly property int activeTabIndex: root.currentTabIndex
    readonly property bool gridViewActive: root.gridMode
    readonly property bool previewVisible: previewModel.active
    readonly property bool openWithVisible: openWithModel.active
    readonly property bool propertiesVisible: propertiesModel.active
    readonly property bool networkPromptFocused: networkInput.activeFocus
    readonly property bool networkConnectActive: networkModel.connecting
    readonly property bool archiveOperationActive: archiveModel.active
    readonly property bool bulkRenameVisible: bulkRenameModel.active
    readonly property bool templateChooserVisible: templateModel.active

    function openExternalLocation(directory, selection) {
        root.pendingCursorPath = selection
        root.cursorPath = selection
        root.navigate(directory)
        Qt.callLater(root.restoreCursor)
        window.requestActivate()
    }

    function alpha(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    FocusScope {
        id: root
        anchors.fill: parent
        focus: true

        property bool filterMode: false
        property bool locationMode: false
        property bool sidebarVisible: true
        property bool placesMode: false
        property bool finderMode: false
        property bool recentMode: false
        property bool networkMode: false
        property var tabs: []
        property int currentTabIndex: -1
        property bool gridMode: false
        property bool bookmarkRenameMode: false
        property int bookmarkRenameRow: -1
        property string operationMode: ""
        property string operationTargetPath: ""
        property var archivePaths: []
        property var folderSelectionPaths: []
        property string folderGatherTargetPath: ""
        property bool helpVisible: false
        property bool visualMode: false
        property int visualAnchorIndex: -1
        property int visualExtentIndex: -1
        property int pointerSelectionAnchorIndex: -1
        property var visualBaseSelection: ({})
        property var selectedPaths: ({})
        property int selectedCount: 0
        property int selectionRevision: 0
        property var clipboardPaths: []
        property string clipboardMode: ""
        property string activeTransferDestination: ""
        property string noticeMessage: ""
        property bool noticeIsError: false
        property string pendingKey: ""
        property string cursorPath: initialSelectionPath
        property string pendingCursorPath: ""
        property int cursorRestoreAttempts: 0
        property string pendingSelectionPath: ""
        property bool conflictRenameMode: false
        property bool conflictApplyRemaining: false
        property bool conflictWasActive: false
        property bool trashPromptVisible: false
        property var trashPromptPaths: []
        property string trashFallbackPath: ""
        property bool restoreInProgress: false
        property bool emptyTrashPromptVisible: false
        property bool emptyTrashInProgress: false
        property bool dragActive: false
        property string dropIntent: ""
        property string dropDestination: ""
        property var backHistory: []
        property var forwardHistory: []
        property string historyCurrentPath: fileModel.currentPath
        property bool historyNavigation: false
        property var pendingHistoryEntry: null
        property real pendingScrollY: -1
        readonly property int activePlaceIndex: {
            var count = placesModel.count
            var current = fileModel.currentPath
            var bestRow = -1
            var bestLength = -1
            for (var row = 0; row < count; ++row) {
                if (recentMode && placesModel.isRecentAt(row))
                    return row
                if (fileModel.trashView && placesModel.isTrashAt(row))
                    return row
                if (recentMode || fileModel.trashView)
                    continue
                var path = placesModel.pathAt(row)
                if (path.length === 0)
                    continue
                var matches = path === "/" ? current.startsWith("/")
                                             : current === path || current.startsWith(path + "/")
                if (matches && path.length > bestLength) {
                    bestRow = row
                    bestLength = path.length
                }
            }
            return bestRow
        }
        readonly property string modeName: {
            if (fileModel.transferConflictActive)
                return "CONFLICT"
            if (archiveModel.active)
                return "ARCHIVE"
            if (bulkRenameModel.active)
                return "BULK"
            if (templateModel.active)
                return "TEMPLATE"
            if (networkMode)
                return "CONNECT"
            if (finderMode)
                return "FIND"
            if (recentMode)
                return "RECENT"
            if (bookmarkRenameMode)
                return "BOOKMARK"
            if (emptyTrashPromptVisible)
                return "EMPTY TRASH"
            if (trashPromptVisible)
                return "TRASH"
            if (placesMode)
                return "PLACES"
            if (locationMode)
                return "LOCATION"
            if (operationMode.length > 0)
                return operationMode.toUpperCase()
            if (filterMode)
                return "FILTER"
            return visualMode ? "VISUAL" : "NORMAL"
        }

        function showNotice(message, isError) {
            noticeMessage = message
            noticeIsError = isError
            noticeTimer.restart()
        }

        function styledFuzzyText(value, query) {
            var text = String(value)
            var needle = String(query).toLocaleLowerCase()
            var candidate = text.toLocaleLowerCase()
            var matched = ({})
            var position = -1
            for (var needleIndex = 0; needleIndex < needle.length; ++needleIndex) {
                position = candidate.indexOf(needle.charAt(needleIndex), position + 1)
                if (position < 0)
                    return escapeStyledText(text)
                matched[position] = true
            }

            var result = ""
            var accent = theme.accent.toString()
            for (var index = 0; index < text.length; ++index) {
                var character = escapeStyledText(text.charAt(index))
                result += matched[index]
                        ? "<font color=\"" + accent + "\"><b>" + character + "</b></font>"
                        : character
            }
            return result
        }

        function escapeStyledText(value) {
            return String(value).replace(/&/g, "&amp;")
                                .replace(/</g, "&lt;")
                                .replace(/>/g, "&gt;")
        }

        function copySelection(source) {
            var result = {}
            for (var path in source)
                result[path] = true
            return result
        }

        function applySelection(paths) {
            selectedPaths = paths
            selectedCount = Object.keys(paths).length
            selectionRevision += 1
        }

        function clearSelection() {
            applySelection({})
        }

        function isPathSelected(path, revision) {
            return revision >= 0 && selectedPaths[path] === true
        }

        function selectedPathList() {
            return Object.keys(selectedPaths)
        }

        function currentOperationPaths() {
            var paths = selectedPathList()
            if (paths.length > 0)
                return paths
            if (files.currentIndex >= 0)
                return [fileModel.pathAt(files.currentIndex)]
            return []
        }

        function stageClipboard(mode) {
            if (fileModel.trashView) {
                showNotice("Restore items before copying or cutting them.", true)
                return
            }
            var paths = currentOperationPaths()
            if (paths.length === 0) {
                showNotice("There is nothing to " + (mode === "copy" ? "copy." : "cut."), true)
                return
            }
            clipboardPaths = paths.slice()
            clipboardMode = mode
            fileModel.setFileClipboard(clipboardPaths, clipboardMode === "move")
            pendingKey = ""
            visualMode = false
            var verb = mode === "copy" ? "Yanked " : "Cut "
            showNotice(verb + paths.length + (paths.length === 1 ? " item." : " items."), false)
        }

        function pasteClipboard() {
            if (fileModel.trashView) {
                showNotice("Restore items instead of pasting into Trash.", true)
                return
            }
            syncClipboardFromDesktop()
            if (clipboardPaths.length === 0) {
                showNotice("The file clipboard is empty.", true)
                return
            }
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Another file operation is already active.", true)
                return
            }
            activeTransferDestination = fileModel.currentPath
            if (fileModel.startTransfer(clipboardPaths, activeTransferDestination,
                                        clipboardMode === "move")) {
                pendingKey = ""
                visualMode = false
                clearSelection()
            }
        }

        function syncClipboardFromDesktop() {
            clipboardPaths = fileModel.fileClipboardPaths
            clipboardMode = clipboardPaths.length === 0
                            ? "" : (fileModel.fileClipboardMove ? "move" : "copy")
        }

        function undoLastMutation() {
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            fileModel.undoLast()
            pendingKey = ""
            visualMode = false
            files.forceActiveFocus()
        }

        function removeCompletedCutPaths(completedSources) {
            if (clipboardMode !== "move")
                return
            var completed = {}
            for (var index = 0; index < completedSources.length; ++index)
                completed[completedSources[index]] = true
            var remaining = []
            for (var pathIndex = 0; pathIndex < clipboardPaths.length; ++pathIndex) {
                if (!completed[clipboardPaths[pathIndex]])
                    remaining.push(clipboardPaths[pathIndex])
            }
            clipboardPaths = remaining
            if (remaining.length === 0) {
                clipboardMode = ""
                fileModel.clearFileClipboardIfOwned()
            } else if (fileModel.ownsFileClipboard()) {
                fileModel.setFileClipboard(remaining, true)
            }
        }

        function removeClipboardPaths(removedPaths) {
            if (clipboardPaths.length === 0 || removedPaths.length === 0)
                return
            var removed = {}
            for (var index = 0; index < removedPaths.length; ++index)
                removed[removedPaths[index]] = true
            var remaining = []
            for (var pathIndex = 0; pathIndex < clipboardPaths.length; ++pathIndex) {
                if (!removed[clipboardPaths[pathIndex]])
                    remaining.push(clipboardPaths[pathIndex])
            }
            clipboardPaths = remaining
            if (remaining.length === 0) {
                clipboardMode = ""
                fileModel.clearFileClipboardIfOwned()
            } else if (fileModel.ownsFileClipboard()) {
                fileModel.setFileClipboard(remaining, clipboardMode === "move")
            }
        }

        function fallbackPathAfterRemoving(paths) {
            var removing = {}
            for (var pathIndex = 0; pathIndex < paths.length; ++pathIndex)
                removing[paths[pathIndex]] = true
            var start = Math.max(0, files.currentIndex)
            for (var next = start + 1; next < fileModel.count; ++next) {
                var nextPath = fileModel.pathAt(next)
                if (!removing[nextPath])
                    return nextPath
            }
            for (var previous = start - 1; previous >= 0; --previous) {
                var previousPath = fileModel.pathAt(previous)
                if (!removing[previousPath])
                    return previousPath
            }
            return ""
        }

        function enterTrashPrompt() {
            if (fileModel.trashView) {
                showNotice("Permanent deletion is not available.", true)
                return
            }
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            var paths = currentOperationPaths()
            if (paths.length === 0) {
                showNotice("There is nothing to move to Trash.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            helpVisible = false
            trashPromptPaths = paths.slice()
            trashFallbackPath = fallbackPathAfterRemoving(paths)
            trashPromptVisible = true
            files.forceActiveFocus()
        }

        function leaveTrashPrompt() {
            trashPromptVisible = false
            trashPromptPaths = []
            trashFallbackPath = ""
            files.forceActiveFocus()
        }

        function confirmTrash() {
            var paths = trashPromptPaths.slice()
            var fallback = trashFallbackPath
            trashPromptVisible = false
            trashPromptPaths = []
            trashFallbackPath = ""
            if (fileModel.startTrash(paths)) {
                pendingCursorPath = fallback
                clearSelection()
            }
            files.forceActiveFocus()
        }

        function restoreSelection() {
            if (!fileModel.trashView) {
                showNotice("Open Trash before restoring items.", true)
                return
            }
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            var paths = currentOperationPaths()
            if (paths.length === 0) {
                showNotice("There is nothing to restore.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            pendingCursorPath = fallbackPathAfterRemoving(paths)
            restoreInProgress = true
            if (fileModel.startRestore(paths)) {
                clearSelection()
            } else {
                restoreInProgress = false
                pendingCursorPath = ""
            }
            files.forceActiveFocus()
        }

        function enterEmptyTrashPrompt() {
            if (!fileModel.trashView) {
                showNotice("Open Trash before emptying it.", true)
                return
            }
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            if (fileModel.count === 0) {
                showNotice("Trash is already empty.", false)
                return
            }
            pendingKey = ""
            visualMode = false
            helpVisible = false
            emptyTrashInput.text = ""
            emptyTrashPromptVisible = true
            Qt.callLater(function() { emptyTrashInput.forceActiveFocus() })
        }

        function leaveEmptyTrashPrompt() {
            emptyTrashPromptVisible = false
            emptyTrashInput.text = ""
            files.forceActiveFocus()
        }

        function confirmEmptyTrash() {
            if (emptyTrashInput.text.trim().toUpperCase() !== "EMPTY")
                return
            emptyTrashPromptVisible = false
            emptyTrashInput.text = ""
            emptyTrashInProgress = true
            if (!fileModel.startEmptyTrash())
                emptyTrashInProgress = false
            files.forceActiveFocus()
        }

        function conflictItemName() {
            var path = fileModel.transferConflictTarget
            return path.substring(path.lastIndexOf("/") + 1)
        }

        function resolveConflict(action) {
            fileModel.resolveTransferConflict(action, "",
                                               conflictApplyRemaining)
        }

        function enterConflictRename() {
            conflictRenameMode = true
            conflictRenameInput.text = conflictItemName()
            conflictRenameInput.forceActiveFocus()
            conflictRenameInput.selectAll()
        }

        function leaveConflictRename() {
            conflictRenameMode = false
            files.forceActiveFocus()
        }

        function commitConflictRename() {
            if (fileModel.resolveTransferConflict("rename", conflictRenameInput.text, false)) {
                conflictRenameMode = false
                files.forceActiveFocus()
            }
        }

        function actionPath() {
            var paths = selectedPathList()
            if (paths.length === 1)
                return paths[0]
            if (paths.length > 1 || files.currentIndex < 0)
                return ""
            return fileModel.pathAt(files.currentIndex)
        }

        function toggleCurrentSelection() {
            if (files.currentIndex < 0)
                return
            visualMode = false
            var path = fileModel.pathAt(files.currentIndex)
            var next = copySelection(selectedPaths)
            if (next[path])
                delete next[path]
            else
                next[path] = true
            applySelection(next)
        }

        function choosePointer(row, modifiers) {
            var control = (modifiers & Qt.ControlModifier) !== 0
            var shift = (modifiers & Qt.ShiftModifier) !== 0
            selectIndex(row, false)
            if (shift) {
                var anchor = pointerSelectionAnchorIndex >= 0
                             ? pointerSelectionAnchorIndex : row
                var range = control ? copySelection(selectedPaths) : {}
                var first = Math.min(anchor, row)
                var last = Math.max(anchor, row)
                for (var index = first; index <= last; ++index)
                    range[fileModel.pathAt(index)] = true
                applySelection(range)
            } else if (control) {
                toggleCurrentSelection()
                pointerSelectionAnchorIndex = row
            } else {
                clearSelection()
                pointerSelectionAnchorIndex = row
            }
        }

        function openContextMenu(row, x, y) {
            selectIndex(row, false)
            var path = fileModel.pathAt(row)
            if (!selectedPaths[path])
                clearSelection()
            contextMenu.x = Math.min(x, width - contextMenu.implicitWidth)
            contextMenu.y = Math.min(y, height - contextMenu.implicitHeight)
            contextMenu.open()
        }

        function dragPathsFor(path, revision) {
            if (revision >= 0 && selectedPaths[path])
                return selectedPathList()
            return [path]
        }

        function localPathsFromUrls(urls) {
            var paths = []
            for (var index = 0; index < urls.length; ++index) {
                var value = urls[index].toString()
                if (value.indexOf("file://") !== 0)
                    continue
                var path = value.substring(7)
                if (path.indexOf("localhost/") === 0)
                    path = path.substring(9)
                try {
                    path = decodeURIComponent(path)
                } catch (error) {
                    continue
                }
                if (path.length > 0 && paths.indexOf(path) < 0)
                    paths.push(path)
            }
            return paths
        }

        function handleDrop(urls, destination, move) {
            if (fileModel.trashView || fileModel.transferActive || fileModel.trashActive)
                return false
            var paths = localPathsFromUrls(urls)
            if (paths.length === 0)
                return false
            activeTransferDestination = destination
            if (!fileModel.startTransfer(paths, destination, move))
                return false
            pendingKey = ""
            visualMode = false
            clearSelection()
            return true
        }

        function currentHistoryEntry() {
            return {
                path: fileModel.currentPath,
                cursor: cursorPath,
                scrollY: currentViewContentY()
            }
        }

        function goBackHistory() {
            if (backHistory.length === 0) {
                showNotice("No older location.", false)
                return
            }
            var target = backHistory[backHistory.length - 1]
            var nextBack = backHistory.slice(0, backHistory.length - 1)
            var nextForward = forwardHistory.slice()
            nextForward.push(currentHistoryEntry())
            historyNavigation = true
            pendingHistoryEntry = target
            if (fileModel.navigateTo(target.path)) {
                backHistory = nextBack
                forwardHistory = nextForward
            } else {
                historyNavigation = false
                pendingHistoryEntry = null
            }
        }

        function goForwardHistory() {
            if (forwardHistory.length === 0) {
                showNotice("No newer location.", false)
                return
            }
            var target = forwardHistory[forwardHistory.length - 1]
            var nextForward = forwardHistory.slice(0, forwardHistory.length - 1)
            var nextBack = backHistory.slice()
            nextBack.push(currentHistoryEntry())
            historyNavigation = true
            pendingHistoryEntry = target
            if (fileModel.navigateTo(target.path)) {
                forwardHistory = nextForward
                backHistory = nextBack
            } else {
                historyNavigation = false
                pendingHistoryEntry = null
            }
        }

        function selectAll() {
            visualMode = false
            var next = {}
            for (var index = 0; index < fileModel.count; ++index)
                next[fileModel.pathAt(index)] = true
            applySelection(next)
        }

        function updateVisualSelection() {
            if (!visualMode || visualAnchorIndex < 0 || files.currentIndex < 0)
                return
            var oldFirst = Math.min(visualAnchorIndex, visualExtentIndex)
            var oldLast = Math.max(visualAnchorIndex, visualExtentIndex)
            var newFirst = Math.min(visualAnchorIndex, files.currentIndex)
            var newLast = Math.max(visualAnchorIndex, files.currentIndex)
            var changed = false
            for (var oldIndex = oldFirst; oldIndex <= oldLast; ++oldIndex) {
                if (oldIndex >= newFirst && oldIndex <= newLast)
                    continue
                var oldPath = fileModel.pathAt(oldIndex)
                var keep = visualBaseSelection[oldPath] === true
                if (keep !== (selectedPaths[oldPath] === true)) {
                    if (keep) {
                        selectedPaths[oldPath] = true
                        selectedCount += 1
                    } else {
                        delete selectedPaths[oldPath]
                        selectedCount -= 1
                    }
                    changed = true
                }
            }
            for (var newIndex = newFirst; newIndex <= newLast; ++newIndex) {
                if (newIndex >= oldFirst && newIndex <= oldLast)
                    continue
                var newPath = fileModel.pathAt(newIndex)
                if (selectedPaths[newPath] !== true) {
                    selectedPaths[newPath] = true
                    selectedCount += 1
                    changed = true
                }
            }
            visualExtentIndex = files.currentIndex
            if (changed)
                selectionRevision += 1
        }

        function toggleVisualMode() {
            if (visualMode) {
                visualMode = false
                return
            }
            if (files.currentIndex < 0)
                return
            pendingKey = ""
            visualMode = true
            visualAnchorIndex = files.currentIndex
            visualExtentIndex = files.currentIndex
            visualBaseSelection = copySelection(selectedPaths)
            var path = fileModel.pathAt(files.currentIndex)
            if (selectedPaths[path] !== true) {
                selectedPaths[path] = true
                selectedCount += 1
                selectionRevision += 1
            }
        }

        function pruneSelection() {
            if (selectedCount === 0)
                return
            var next = {}
            var changed = false
            for (var path in selectedPaths) {
                if (fileModel.indexOfPath(path) >= 0)
                    next[path] = true
                else
                    changed = true
            }
            if (changed)
                applySelection(next)
        }

        function selectIndex(index, extendVisual) {
            if (fileModel.count <= 0) {
                files.currentIndex = -1
                cursorPath = ""
                return
            }
            files.currentIndex = Math.max(0, Math.min(index, fileModel.count - 1))
            grid.currentIndex = files.currentIndex
            cursorPath = fileModel.pathAt(files.currentIndex)
            if (gridMode)
                grid.positionViewAtIndex(files.currentIndex, GridView.Contain)
            else
                files.positionViewAtIndex(files.currentIndex, ListView.Contain)
            if (extendVisual === undefined || extendVisual)
                updateVisualSelection()
        }

        function moveCursor(delta) {
            if (fileModel.count <= 0)
                return
            var start = files.currentIndex < 0 ? 0 : files.currentIndex
            selectIndex(start + delta)
        }

        function restoreCursor() {
            if (fileModel.count <= 0) {
                files.currentIndex = -1
                return
            }
            var wantedPath = pendingCursorPath.length > 0 ? pendingCursorPath : cursorPath
            var index = wantedPath.length > 0 ? fileModel.indexOfPath(wantedPath) : -1
            if (index < 0 && pendingCursorPath.length > 0
                && cursorRestoreAttempts < 40) {
                cursorRestoreAttempts += 1
                cursorRestoreTimer.restart()
                return
            }
            if (index < 0)
                index = Math.max(0, Math.min(files.currentIndex, fileModel.count - 1))
            pendingCursorPath = ""
            cursorRestoreAttempts = 0
            cursorRestoreTimer.stop()
            selectIndex(index)
            if (pendingScrollY >= 0) {
                if (gridMode)
                    grid.contentY = pendingScrollY
                else
                    files.contentY = pendingScrollY
                pendingScrollY = -1
            }
        }

        function currentViewContentY() {
            return gridMode ? grid.contentY : files.contentY
        }

        function setGridMode(enabled) {
            if (gridMode === enabled)
                return
            var index = files.currentIndex
            gridMode = enabled
            grid.currentIndex = index
            if (index >= 0) {
                if (gridMode)
                    grid.positionViewAtIndex(index, GridView.Contain)
                else
                    files.positionViewAtIndex(index, ListView.Contain)
            }
            files.forceActiveFocus()
        }

        function activateCurrent() {
            if (files.currentIndex < 0)
                return
            pendingKey = ""
            fileModel.activate(files.currentIndex)
        }

        function openPreview() {
            if (files.currentIndex < 0)
                return
            if (!previewModel.open(fileModel.pathAt(files.currentIndex)))
                showNotice("This item is no longer available.", true)
            files.forceActiveFocus()
        }

        function openWithCurrent() {
            if (files.currentIndex < 0 || fileModel.isDirectoryAt(files.currentIndex)) {
                showNotice("Choose a file to open with another application.", true)
                return
            }
            if (!openWithModel.open(fileModel.pathAt(files.currentIndex))) {
                showNotice(openWithModel.errorMessage.length > 0
                           ? openWithModel.errorMessage : "This file cannot be opened here.", true)
                return
            }
            openWithApplications.currentIndex = -1
            files.forceActiveFocus()
        }

        function activateOpenWith() {
            var row = openWithApplications.currentIndex
            if (row < 0 || row >= openWithModel.count)
                return
            var path = openWithModel.path
            var desktopId = openWithModel.desktopIdAt(row)
            openWithModel.close()
            fileModel.openWith(path, desktopId)
            files.forceActiveFocus()
        }

        function openProperties() {
            if (files.currentIndex < 0)
                return
            if (!propertiesModel.open(fileModel.pathAt(files.currentIndex)))
                showNotice("This item is no longer available.", true)
            files.forceActiveFocus()
        }

        function closeProperties() {
            propertiesModel.close()
            if (finderMode)
                finderInput.forceActiveFocus()
            else
                files.forceActiveFocus()
        }

        function copyCurrentPaths() {
            var paths = currentOperationPaths()
            if (fileModel.copyPathsAsText(paths))
                showNotice("Copied " + paths.length + (paths.length === 1 ? " path." : " paths."), false)
            else
                showNotice("There is no path to copy.", true)
            pendingKey = ""
            files.forceActiveFocus()
        }

        function openTerminalHere() {
            var path = fileModel.currentPath
            if (files.currentIndex >= 0 && fileModel.isDirectoryAt(files.currentIndex))
                path = fileModel.pathAt(files.currentIndex)
            if (fileModel.openTerminal(path))
                showNotice("Opened terminal in " + path, false)
        }

        function navigate(path) {
            if (!path || (!fileModel.trashView && path === fileModel.currentPath))
                return
            recentMode = false
            if (fileModel.trashView) {
                pendingSelectionPath = ""
                fileModel.navigateTo(path)
                return
            }
            var prefix = path === "/" ? "/" : path + "/"
            if (fileModel.currentPath.indexOf(prefix) === 0) {
                var remainder = fileModel.currentPath.substring(prefix.length)
                var child = remainder.split("/")[0]
                pendingSelectionPath = path === "/" ? "/" + child : path + "/" + child
            } else {
                pendingSelectionPath = ""
            }
            fileModel.navigateTo(path)
        }

        function goParent() {
            if (fileModel.trashView) {
                pendingSelectionPath = ""
                fileModel.goParent()
                return
            }
            pendingSelectionPath = fileModel.currentPath
            fileModel.goParent()
        }

        function tabTitle(path, trash) {
            if (trash)
                return "Trash"
            if (path === "/")
                return "/"
            var trimmed = path.endsWith("/") ? path.substring(0, path.length - 1) : path
            var slash = trimmed.lastIndexOf("/")
            return slash >= 0 ? trimmed.substring(slash + 1) : trimmed
        }

        function makeTab(path, trash, cursor, scroll, back, forward) {
            return {path: path, trash: trash, title: tabTitle(path, trash),
                    cursor: cursor || "", scroll: scroll || 0,
                    back: back ? back.slice() : [], forward: forward ? forward.slice() : []}
        }

        function saveCurrentTab() {
            if (currentTabIndex < 0 || currentTabIndex >= tabs.length)
                return
            var next = tabs.slice()
            next[currentTabIndex] = makeTab(fileModel.currentPath, fileModel.trashView,
                                            cursorPath, currentViewContentY(),
                                            backHistory, forwardHistory)
            tabs = next
        }

        function updateCurrentTabLocation() {
            if (currentTabIndex < 0 || currentTabIndex >= tabs.length)
                return
            var next = tabs.slice()
            var current = next[currentTabIndex]
            next[currentTabIndex] = makeTab(fileModel.currentPath, fileModel.trashView,
                                            current.cursor, current.scroll,
                                            current.back, current.forward)
            tabs = next
        }

        function loadTab(index) {
            if (index < 0 || index >= tabs.length)
                return
            if (finderMode) {
                finderMode = false
                searchModel.cancel()
                finderInput.text = ""
            }
            recentMode = false
            if (previewModel.active)
                previewModel.close()
            if (openWithModel.active)
                openWithModel.close()
            placesMode = false
            filterMode = false
            locationMode = false
            operationMode = ""
            var previousTabIndex = currentTabIndex
            var previousBackHistory = backHistory.slice()
            var previousForwardHistory = forwardHistory.slice()
            var previousCursorPath = cursorPath
            var previousPendingCursorPath = pendingCursorPath
            var previousPendingScrollY = pendingScrollY
            var tab = tabs[index]
            currentTabIndex = index
            backHistory = tab.back ? tab.back.slice() : []
            forwardHistory = tab.forward ? tab.forward.slice() : []
            cursorPath = tab.cursor || ""
            pendingCursorPath = tab.cursor || ""
            pendingScrollY = tab.scroll || 0
            historyNavigation = true
            pendingHistoryEntry = {cursor: tab.cursor || "", scrollY: tab.scroll || 0}
            var alreadyThere = tab.trash === fileModel.trashView
                               && (tab.trash || tab.path === fileModel.currentPath)
            if (alreadyThere) {
                historyNavigation = false
                pendingHistoryEntry = null
                Qt.callLater(restoreCursor)
            } else {
                var changed = tab.trash ? fileModel.navigateToTrash()
                                        : fileModel.navigateTo(tab.path)
                if (!changed) {
                    currentTabIndex = previousTabIndex
                    backHistory = previousBackHistory
                    forwardHistory = previousForwardHistory
                    cursorPath = previousCursorPath
                    pendingCursorPath = previousPendingCursorPath
                    pendingScrollY = previousPendingScrollY
                    historyNavigation = false
                    pendingHistoryEntry = null
                    showNotice(fileModel.errorMessage, true)
                }
            }
            files.forceActiveFocus()
        }

        function switchTab(index) {
            if (index === currentTabIndex || index < 0 || index >= tabs.length)
                return
            saveCurrentTab()
            loadTab(index)
        }

        function openTab(path, trash, activate) {
            saveCurrentTab()
            var next = tabs.slice()
            next.push(makeTab(path, trash, "", 0, [], []))
            tabs = next
            if (activate)
                loadTab(next.length - 1)
            else
                showNotice("Opened " + tabTitle(path, trash) + " in a new tab.", false)
        }

        function openCurrentInNewTab(activate) {
            if (fileModel.trashView || files.currentIndex < 0
                || !fileModel.isDirectoryAt(files.currentIndex)) {
                showNotice("Choose a folder to open in a new tab.", true)
                return
            }
            openTab(fileModel.pathAt(files.currentIndex), false, activate)
        }

        function duplicateCurrentTab() {
            openTab(fileModel.currentPath, fileModel.trashView, true)
        }

        function closeTab(index) {
            if (tabs.length <= 1) {
                showNotice("Shibui keeps one tab open.", false)
                return
            }
            saveCurrentTab()
            var wasCurrent = index === currentTabIndex
            var next = tabs.slice()
            next.splice(index, 1)
            tabs = next
            if (!wasCurrent) {
                if (index < currentTabIndex)
                    currentTabIndex -= 1
                return
            }
            currentTabIndex = -1
            loadTab(Math.min(index, next.length - 1))
        }

        function cycleTab(delta) {
            if (tabs.length < 2)
                return
            var next = (currentTabIndex + delta + tabs.length) % tabs.length
            switchTab(next)
        }

        function enterFilterMode() {
            pendingKey = ""
            visualMode = false
            filterMode = true
            filterInput.text = fileModel.filterText
            filterInput.forceActiveFocus()
            filterInput.selectAll()
        }

        function leaveFilterMode(clearFilter) {
            if (clearFilter) {
                filterInput.text = ""
                fileModel.filterText = ""
            }
            filterMode = false
            files.forceActiveFocus()
            Qt.callLater(restoreCursor)
        }

        function enterFinderMode() {
            if (fileModel.trashView) {
                showNotice("Recursive finding is unavailable in Trash.", true)
                return
            }
            if (placesModel.networkUriForPath(fileModel.currentPath).length > 0) {
                showNotice("Recursive finding is unavailable on network shares.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            placesMode = false
            filterMode = false
            locationMode = false
            operationMode = ""
            finderMode = true
            finderInput.text = ""
            finderResults.currentIndex = -1
            searchModel.query = ""
            searchModel.typeFilter = "all"
            searchModel.modifiedWithinDays = 0
            if (!searchModel.start(fileModel.currentPath, fileModel.showHidden)) {
                finderMode = false
                showNotice(searchModel.errorMessage, true)
                files.forceActiveFocus()
                return
            }
            finderInput.forceActiveFocus()
        }

        function enterRecentMode() {
            pendingKey = ""
            visualMode = false
            placesMode = false
            finderMode = false
            filterMode = false
            locationMode = false
            operationMode = ""
            recentMode = true
            finderResults.currentIndex = -1
            recentModel.refresh()
            files.forceActiveFocus()
        }

        function leaveRecentMode() {
            recentMode = false
            files.forceActiveFocus()
        }

        function leaveFinderMode() {
            finderMode = false
            searchModel.cancel()
            finderInput.text = ""
            files.forceActiveFocus()
        }

        function enterNetworkMode(uri) {
            pendingKey = ""
            placesMode = false
            finderMode = false
            filterMode = false
            locationMode = false
            operationMode = ""
            networkMode = true
            networkInput.text = uri && uri.length > 0 ? uri : "smb://"
            networkInput.forceActiveFocus()
            networkInput.selectAll()
        }

        function leaveNetworkMode() {
            if (networkModel.connecting)
                networkModel.cancel()
            networkMode = false
            networkInput.text = ""
            files.forceActiveFocus()
        }

        function commitNetworkInput() {
            if (networkModel.promptActive) {
                networkModel.submitResponse(networkInput.text)
                networkInput.text = ""
                return
            }
            if (networkModel.connecting)
                return
            if (!networkModel.connectTo(networkInput.text)) {
                showNotice(networkModel.errorMessage, true)
                return
            }
            networkInput.text = ""
        }

        function moveFinderCursor(delta) {
            var count = recentMode ? recentModel.count : searchModel.count
            if (count <= 0)
                return
            var start = finderResults.currentIndex < 0 ? 0 : finderResults.currentIndex
            finderResults.currentIndex = Math.max(0, Math.min(count - 1,
                                                               start + delta))
            finderResults.positionViewAtIndex(finderResults.currentIndex, ListView.Contain)
        }

        function cycleFinderType() {
            var types = ["all", "files", "folders", "images", "documents",
                         "audio", "video", "archives"]
            var index = types.indexOf(searchModel.typeFilter)
            searchModel.typeFilter = types[(index + 1) % types.length]
        }

        function cycleFinderDate() {
            var dates = [0, 1, 7, 30, 365]
            var index = dates.indexOf(searchModel.modifiedWithinDays)
            searchModel.modifiedWithinDays = dates[(index + 1) % dates.length]
        }

        function finderDateLabel() {
            if (searchModel.modifiedWithinDays === 0)
                return "ANY TIME"
            if (searchModel.modifiedWithinDays === 1)
                return "TODAY"
            if (searchModel.modifiedWithinDays === 7)
                return "7 DAYS"
            if (searchModel.modifiedWithinDays === 30)
                return "30 DAYS"
            return "1 YEAR"
        }

        function activateFinderResult() {
            var row = finderResults.currentIndex
            var model = recentMode ? recentModel : searchModel
            if (row < 0 || row >= model.count)
                return
            var path = model.pathAt(row)
            var directory = model.isDirectoryAt(row)
            if (recentMode)
                leaveRecentMode()
            else
                leaveFinderMode()
            if (directory)
                navigate(path)
            else
                fileModel.activatePath(path)
        }

        function revealFinderResult() {
            var row = finderResults.currentIndex
            var model = recentMode ? recentModel : searchModel
            if (row < 0 || row >= model.count)
                return
            var path = model.pathAt(row)
            var slash = path.lastIndexOf("/")
            var parentPath = slash > 0 ? path.substring(0, slash) : "/"
            if (recentMode)
                leaveRecentMode()
            else
                leaveFinderMode()
            window.openExternalLocation(parentPath, path)
        }

        function openFinderProperties() {
            var row = finderResults.currentIndex
            var model = recentMode ? recentModel : searchModel
            if (row < 0 || row >= model.count)
                return
            if (!propertiesModel.open(model.pathAt(row)))
                showNotice("This item is no longer available.", true)
            else
                propertiesOverlay.forceActiveFocus()
        }

        function enterLocationMode() {
            if (fileModel.transferConflictActive || root.trashPromptVisible
                || root.emptyTrashPromptVisible)
                return
            pendingKey = ""
            visualMode = false
            filterMode = false
            operationMode = ""
            locationMode = true
            locationInput.text = fileModel.trashView ? "" : fileModel.currentPath
            locationInput.forceActiveFocus()
            locationInput.selectAll()
        }

        function leaveLocationMode() {
            locationMode = false
            fileModel.clearError()
            files.forceActiveFocus()
        }

        function commitLocation() {
            if (locationInput.text.trim().length === 0)
                return
            if (!fileModel.navigateTo(locationInput.text))
                return
            locationMode = false
            files.forceActiveFocus()
        }

        function enterPlacesMode() {
            sidebarVisible = true
            placesMode = true
            pendingKey = ""
            visualMode = false
            if (activePlaceIndex >= 0)
                places.currentIndex = activePlaceIndex
            else if (places.currentIndex < 0 && placesModel.count > 0)
                places.currentIndex = 0
            files.forceActiveFocus()
        }

        function leavePlacesMode() {
            placesMode = false
            files.forceActiveFocus()
        }

        function activatePlace(row) {
            if (row < 0 || row >= placesModel.count)
                return
            places.currentIndex = row
            if (placesModel.isRecentAt(row)) {
                enterRecentMode()
                return
            }
            if (placesModel.isNetworkAt(row)) {
                var networkPath = placesModel.pathAt(row)
                if (networkPath.length === 0) {
                    enterNetworkMode(placesModel.networkUriAt(row))
                    return
                }
                placesMode = false
                fileModel.navigateTo(networkPath)
                files.forceActiveFocus()
                return
            }
            if (placesModel.isDeviceAt(row) && !placesModel.isMountedAt(row)) {
                if (!placesModel.mountAt(row))
                    showNotice("This device cannot be mounted right now.", true)
                return
            }
            placesMode = false
            if (placesModel.isTrashAt(row))
                fileModel.navigateToTrash()
            else
                fileModel.navigateTo(placesModel.pathAt(row))
            files.forceActiveFocus()
        }

        function addCurrentBookmark() {
            if (fileModel.trashView) {
                showNotice("Trash cannot be bookmarked.", true)
                return
            }
            var networkUri = placesModel.networkUriForPath(fileModel.currentPath)
            var added = networkUri.length > 0
                    ? placesModel.addNetworkBookmark(networkUri, "")
                    : placesModel.addBookmark(fileModel.currentPath, "")
            if (added)
                showNotice("Bookmarked " + fileModel.currentPath, false)
            else
                showNotice("This location is already listed or unavailable.", true)
        }

        function enterBookmarkRename(row) {
            if (!placesModel.isBookmarkAt(row)) {
                showNotice("Choose a bookmark to rename.", true)
                return
            }
            places.currentIndex = row
            placesMode = false
            bookmarkRenameMode = true
            bookmarkRenameRow = row
            bookmarkRenameInput.text = placesModel.labelAt(row)
            bookmarkRenameInput.forceActiveFocus()
            bookmarkRenameInput.selectAll()
        }

        function leaveBookmarkRename() {
            bookmarkRenameMode = false
            bookmarkRenameRow = -1
            placesMode = true
            files.forceActiveFocus()
        }

        function commitBookmarkRename() {
            if (!placesModel.renameBookmark(bookmarkRenameRow, bookmarkRenameInput.text))
                return
            bookmarkRenameMode = false
            bookmarkRenameRow = -1
            placesMode = true
            files.forceActiveFocus()
        }

        function removeSelectedBookmark() {
            var row = places.currentIndex
            if (!placesModel.removeBookmark(row)) {
                showNotice("Choose a bookmark to remove.", true)
                return
            }
            places.currentIndex = Math.max(0, Math.min(row, placesModel.count - 2))
            showNotice("Bookmark removed.", false)
        }

        function moveSelectedBookmark(offset) {
            var row = places.currentIndex
            if (!placesModel.moveBookmark(row, offset)) {
                showNotice("The bookmark cannot move further.", true)
                return
            }
            places.currentIndex = Math.max(0, row + offset)
        }

        function cycleSort() {
            fileModel.setSort((fileModel.sortField + 1) % 4, false)
        }

        function enterCreateMode() {
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            if (fileModel.trashView) {
                showNotice("Folders cannot be created inside Trash.", true)
                return
            }
            noticeMessage = ""
            pendingKey = ""
            visualMode = false
            filterMode = false
            operationTargetPath = ""
            operationMode = "create"
            operationInput.text = ""
            fileModel.clearError()
            operationInput.forceActiveFocus()
        }

        function enterFolderWithSelectionMode() {
            if (fileModel.trashView || fileModel.transferActive || fileModel.trashActive) {
                showNotice("Move ordinary files after the active operation finishes.", true)
                return
            }
            var paths = selectedPathList()
            if (paths.length === 0) {
                showNotice("Select items to move into a new folder.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            filterMode = false
            folderSelectionPaths = paths.slice()
            operationTargetPath = ""
            operationMode = "gather"
            operationInput.text = ""
            fileModel.clearError()
            operationInput.forceActiveFocus()
        }

        function enterRenameMode() {
            if (fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            if (selectedCount > 1) {
                showNotice("Rename works on one item at a time.", true)
                return
            }
            if (files.currentIndex < 0) {
                showNotice("There is no item to rename.", true)
                return
            }
            var path = actionPath()
            if (path.length === 0)
                return
            pendingKey = ""
            noticeMessage = ""
            visualMode = false
            filterMode = false
            operationTargetPath = path
            operationMode = "rename"
            operationInput.text = path.substring(path.lastIndexOf("/") + 1)
            fileModel.clearError()
            operationInput.forceActiveFocus()
            operationInput.selectAll()
        }

        function enterArchiveCreateMode() {
            if (fileModel.trashView) {
                showNotice("Restore items before archiving them.", true)
                return
            }
            if (archiveModel.active || fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            var paths = currentOperationPaths()
            if (paths.length === 0) {
                showNotice("Choose at least one item to archive.", true)
                return
            }
            var name = paths.length === 1
                    ? paths[0].substring(paths[0].lastIndexOf("/") + 1) : "Archive"
            pendingKey = ""
            visualMode = false
            archivePaths = paths.slice()
            operationTargetPath = ""
            operationMode = "archive"
            operationInput.text = name + ".zip"
            operationInput.forceActiveFocus()
            operationInput.selectAll()
        }

        function enterBulkRenameMode() {
            if (fileModel.trashView) {
                showNotice("Restore items before renaming them.", true)
                return
            }
            if (archiveModel.active || fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            var paths = selectedPathList()
            if (paths.length < 2) {
                showNotice("Select at least two items for bulk rename.", true)
                return
            }
            if (!bulkRenameModel.begin(paths)) {
                showNotice("The selected items cannot be renamed together.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            bulkFindInput.forceActiveFocus()
        }

        function enterTemplateMode() {
            if (fileModel.trashView) {
                showNotice("Templates cannot be created inside Trash.", true)
                return
            }
            if (!templateModel.begin(fileModel.currentPath)) {
                showNotice("The current folder is unavailable.", true)
                return
            }
            pendingKey = ""
            visualMode = false
            templateList.currentIndex = -1
            templateNameInput.text = ""
            files.forceActiveFocus()
        }

        function createSelectedTemplate() {
            if (!templateModel.createFrom(templateList.currentIndex,
                                          templateNameInput.text))
                showNotice(templateModel.errorMessage, true)
        }

        function applyBulkRename() {
            if (archiveModel.active || fileModel.transferActive || fileModel.trashActive) {
                showNotice("Wait for the active file operation to finish.", true)
                return
            }
            if (!bulkRenameModel.apply()) {
                showNotice(bulkRenameModel.errorMessage.length > 0
                           ? bulkRenameModel.errorMessage
                           : "Resolve the preview errors before renaming.", true)
            }
        }

        function extractCurrentArchive() {
            if (fileModel.trashView) {
                showNotice("Restore archives before extracting them.", true)
                return
            }
            var paths = currentOperationPaths()
            if (paths.length !== 1 || !archiveModel.supportsArchive(paths[0])) {
                showNotice("Choose one supported archive to extract.", true)
                return
            }
            if (!archiveModel.extractArchive(paths[0], fileModel.currentPath))
                showNotice(archiveModel.errorMessage, true)
            pendingKey = ""
            visualMode = false
            files.forceActiveFocus()
        }

        function leaveOperationMode() {
            operationMode = ""
            operationTargetPath = ""
            archivePaths = []
            folderSelectionPaths = []
            fileModel.clearError()
            files.forceActiveFocus()
        }

        function commitOperation() {
            if (operationMode === "gather") {
                var separator = fileModel.currentPath.endsWith("/") ? "" : "/"
                var target = fileModel.currentPath + separator + operationInput.text
                if (!fileModel.startFolderWithSelection(operationInput.text,
                                                        folderSelectionPaths))
                    return
                folderGatherTargetPath = target
                activeTransferDestination = target
                operationMode = ""
                operationTargetPath = ""
                folderSelectionPaths = []
                clearSelection()
                files.forceActiveFocus()
                return
            }
            if (operationMode === "archive") {
                if (!archiveModel.createArchive(archivePaths, fileModel.currentPath,
                                                operationInput.text)) {
                    showNotice(archiveModel.errorMessage, true)
                    return
                }
                operationMode = ""
                operationTargetPath = ""
                archivePaths = []
                clearSelection()
                files.forceActiveFocus()
                return
            }
            var result = operationMode === "create"
                         ? fileModel.createDirectory(operationInput.text)
                         : fileModel.renamePath(operationTargetPath, operationInput.text)
            if (result.length === 0)
                return
            filterInput.text = ""
            fileModel.filterText = ""
            pendingCursorPath = result
            cursorPath = result
            clearSelection()
            operationMode = ""
            operationTargetPath = ""
            files.forceActiveFocus()
            Qt.callLater(restoreCursor)
        }

        Timer {
            id: noticeTimer
            interval: 3000
            onTriggered: root.noticeMessage = ""
        }

        Timer {
            id: cursorRestoreTimer
            interval: 25
            onTriggered: root.restoreCursor()
        }

        Keys.onPressed: function(event) {
            if (templateModel.active) {
                if (!templateNameInput.activeFocus) {
                    if (event.key === Qt.Key_Escape || event.key === Qt.Key_Q) {
                        templateModel.close()
                    } else if (event.key === Qt.Key_J || event.key === Qt.Key_Down) {
                        templateList.currentIndex = Math.min(templateModel.count - 1,
                                                              templateList.currentIndex + 1)
                    } else if (event.key === Qt.Key_K || event.key === Qt.Key_Up) {
                        templateList.currentIndex = Math.max(0, templateList.currentIndex - 1)
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (templateList.currentIndex >= 0) {
                            templateNameInput.text = templateModel.suggestedNameAt(
                                templateList.currentIndex)
                            templateNameInput.forceActiveFocus()
                            templateNameInput.selectAll()
                        }
                    }
                    event.accepted = true
                }
                return
            }
            if (bulkRenameModel.active) {
                if (!bulkFindInput.activeFocus && !bulkReplacementInput.activeFocus) {
                    if (event.key === Qt.Key_Escape || event.key === Qt.Key_Q)
                        bulkRenameModel.close()
                    else if ((event.modifiers & Qt.ControlModifier)
                             && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter))
                        applyBulkRename()
                    event.accepted = true
                }
                return
            }
            if (propertiesModel.active) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Q) {
                    closeProperties()
                } else if (event.key === Qt.Key_S && propertiesModel.directory) {
                    propertiesModel.calculateDirectorySize()
                } else if (event.key === Qt.Key_C && propertiesModel.sizing) {
                    propertiesModel.cancelDirectorySize()
                } else if (propertiesModel.permissionsEditable
                           && event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                    propertiesModel.togglePermissionBit(event.key - Qt.Key_1)
                }
                event.accepted = true
                return
            }
            if (openWithModel.active) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Q) {
                    openWithModel.close()
                } else if (event.key === Qt.Key_J || event.key === Qt.Key_Down) {
                    openWithApplications.currentIndex = Math.min(openWithModel.count - 1,
                                                                  openWithApplications.currentIndex + 1)
                } else if (event.key === Qt.Key_K || event.key === Qt.Key_Up) {
                    openWithApplications.currentIndex = Math.max(0,
                                                                  openWithApplications.currentIndex - 1)
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    activateOpenWith()
                } else if (event.key === Qt.Key_D) {
                    openWithModel.setDefault(openWithApplications.currentIndex)
                }
                event.accepted = true
                return
            }
            if (previewModel.active) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Space
                    || event.key === Qt.Key_Q) {
                    previewModel.close()
                    files.forceActiveFocus()
                }
                event.accepted = true
                return
            }
            if (filterInput.activeFocus || operationInput.activeFocus
                || locationInput.activeFocus || bookmarkRenameInput.activeFocus
                || finderInput.activeFocus
                || networkInput.activeFocus
                || conflictRenameInput.activeFocus
                || emptyTrashInput.activeFocus)
                return

            var shifted = (event.modifiers & Qt.ShiftModifier) !== 0
            var controlled = (event.modifiers & Qt.ControlModifier) !== 0
            var altered = (event.modifiers & Qt.AltModifier) !== 0
            var lowerG = event.key === Qt.Key_G && !shifted
            var upperG = event.key === Qt.Key_G && shifted

            if (fileModel.transferConflictActive) {
                if ((event.key === Qt.Key_R && !shifted) || event.text === "r")
                    resolveConflict("replace")
                else if ((event.key === Qt.Key_S && !shifted) || event.text === "s")
                    resolveConflict("skip")
                else if ((event.key === Qt.Key_N && !shifted) || event.text === "n")
                    enterConflictRename()
                else if ((event.key === Qt.Key_A && !shifted) || event.text === "a")
                    conflictApplyRemaining = !conflictApplyRemaining
                else if (event.key === Qt.Key_Escape)
                    resolveConflict("cancel")
                event.accepted = true
                return
            }

            if (trashPromptVisible) {
                if ((event.key === Qt.Key_Y && !shifted) || event.text === "y")
                    confirmTrash()
                else if (event.key === Qt.Key_Escape)
                    leaveTrashPrompt()
                event.accepted = true
                return
            }

            if (helpVisible) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Question
                    || (event.key === Qt.Key_Slash && shifted) || event.text === "?") {
                    helpVisible = false
                    event.accepted = true
                }
                return
            }

            if (recentMode) {
                if ((event.key === Qt.Key_J && !shifted) || event.text === "j"
                    || event.key === Qt.Key_Down) {
                    moveFinderCursor(1)
                } else if ((event.key === Qt.Key_K && !shifted) || event.text === "k"
                           || event.key === Qt.Key_Up) {
                    moveFinderCursor(-1)
                } else if (controlled && (event.key === Qt.Key_Return
                                          || event.key === Qt.Key_Enter)) {
                    revealFinderResult()
                } else if (altered && (event.key === Qt.Key_Return
                                       || event.key === Qt.Key_Enter)) {
                    openFinderProperties()
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                           || (event.key === Qt.Key_L && !shifted) || event.text === "l") {
                    activateFinderResult()
                } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Q
                           || (event.key === Qt.Key_H && !shifted) || event.text === "h") {
                    leaveRecentMode()
                }
                event.accepted = true
                return
            }

            if (placesMode) {
                if ((event.key === Qt.Key_J && shifted) || event.text === "J") {
                    moveSelectedBookmark(1)
                } else if ((event.key === Qt.Key_K && shifted) || event.text === "K") {
                    moveSelectedBookmark(-1)
                } else if ((event.key === Qt.Key_J && !shifted) || event.text === "j"
                    || event.key === Qt.Key_Down) {
                    places.currentIndex = Math.min(placesModel.count - 1,
                                                   places.currentIndex + 1)
                } else if ((event.key === Qt.Key_K && !shifted) || event.text === "k"
                           || event.key === Qt.Key_Up) {
                    places.currentIndex = Math.max(0, places.currentIndex - 1)
                } else if ((event.key === Qt.Key_L && !shifted) || event.text === "l"
                           || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    activatePlace(places.currentIndex)
                } else if ((event.key === Qt.Key_A && !shifted) || event.text === "a") {
                    addCurrentBookmark()
                } else if ((event.key === Qt.Key_R && !shifted) || event.text === "r") {
                    enterBookmarkRename(places.currentIndex)
                } else if ((event.key === Qt.Key_D && !shifted) || event.text === "d") {
                    removeSelectedBookmark()
                } else if ((event.key === Qt.Key_U && !shifted) || event.text === "u") {
                    if (!placesModel.unmountAt(places.currentIndex))
                        showNotice("Choose a mounted removable device.", true)
                } else if ((event.key === Qt.Key_E && !shifted) || event.text === "e") {
                    if (!placesModel.ejectAt(places.currentIndex))
                        showNotice("Choose an ejectable device.", true)
                } else if ((event.key === Qt.Key_C && !shifted) || event.text === "c") {
                    enterNetworkMode("")
                } else if ((event.key === Qt.Key_X && !shifted) || event.text === "x") {
                    if (!placesModel.isNetworkAt(places.currentIndex)
                        || !placesModel.isMountedAt(places.currentIndex)
                        || !networkModel.disconnectFrom(
                            placesModel.networkUriAt(places.currentIndex)))
                        showNotice("Choose an active network connection.", true)
                } else if (event.key === Qt.Key_Escape
                           || (event.key === Qt.Key_B && !shifted) || event.text === "b") {
                    leavePlacesMode()
                }
                event.accepted = true
                return
            }

            if (event.key === Qt.Key_Escape
                && (fileModel.transferActive || fileModel.trashActive
                    || archiveModel.active)) {
                if (archiveModel.active)
                    archiveModel.cancel()
                else if (fileModel.trashActive)
                    fileModel.cancelTrash()
                else
                    fileModel.cancelTransfer()
                event.accepted = true
                return
            }

            if (pendingKey.length > 0) {
                if (pendingKey === "g" && (lowerG || event.text === "g")) {
                    pendingKey = ""
                    selectIndex(0)
                    event.accepted = true
                    return
                } else if (pendingKey === "g"
                           && !shifted
                           && (event.key === Qt.Key_T || event.text === "t")) {
                    pendingKey = ""
                    cycleTab(1)
                    event.accepted = true
                    return
                } else if (pendingKey === "g"
                           && ((event.key === Qt.Key_T && shifted) || event.text === "T")) {
                    pendingKey = ""
                    cycleTab(-1)
                    event.accepted = true
                    return
                } else if (pendingKey === "g"
                           && ((event.key === Qt.Key_N && !shifted) || event.text === "n")) {
                    enterFolderWithSelectionMode()
                    event.accepted = true
                    return
                } else if (pendingKey === "y"
                           && ((event.key === Qt.Key_Y && !shifted) || event.text === "y")) {
                    stageClipboard("copy")
                    event.accepted = true
                    return
                } else if (pendingKey === "y"
                           && ((event.key === Qt.Key_P && !shifted) || event.text === "p")) {
                    copyCurrentPaths()
                    event.accepted = true
                    return
                } else if (pendingKey === "d"
                           && ((event.key === Qt.Key_D && !shifted) || event.text === "d")) {
                    stageClipboard("move")
                    event.accepted = true
                    return
                } else if (pendingKey === "a"
                           && ((event.key === Qt.Key_C && !shifted) || event.text === "c")) {
                    enterArchiveCreateMode()
                    event.accepted = true
                    return
                } else if (pendingKey === "a"
                           && ((event.key === Qt.Key_X && !shifted) || event.text === "x")) {
                    extractCurrentArchive()
                    event.accepted = true
                    return
                }
                pendingKey = ""
            }

            if ((event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier) && event.key === Qt.Key_N) {
                enterCreateMode()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C) {
                stageClipboard("copy")
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_X) {
                stageClipboard("move")
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_V) {
                pasteClipboard()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                selectAll()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Space) {
                toggleCurrentSelection()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_D) {
                moveCursor(Math.max(1, Math.floor(files.height / rowHeight / 2)))
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_U) {
                moveCursor(-Math.max(1, Math.floor(files.height / rowHeight / 2)))
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_R) {
                fileModel.refresh()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_W) {
                closeTab(currentTabIndex)
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_T) {
                duplicateCurrentTab()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_1) {
                setGridMode(false)
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_2) {
                setGridMode(true)
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier)
                       && (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)) {
                cycleTab((event.modifiers & Qt.ShiftModifier) !== 0
                         || event.key === Qt.Key_Backtab ? -1 : 1)
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_P) {
                enterFinderMode()
                event.accepted = true
            } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_K) {
                enterNetworkMode("")
                event.accepted = true
            } else if ((event.modifiers & Qt.AltModifier)
                       && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                openProperties()
                event.accepted = true
            } else if (((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_L)
                       || event.key === Qt.Key_Colon || event.text === ":") {
                enterLocationMode()
                event.accepted = true
            } else if (((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Left)
                       || event.key === Qt.Key_BracketLeft || event.text === "[") {
                goBackHistory()
                event.accepted = true
            } else if (((event.modifiers & Qt.AltModifier) && event.key === Qt.Key_Right)
                       || event.key === Qt.Key_BracketRight || event.text === "]") {
                goForwardHistory()
                event.accepted = true
            } else if ((event.key === Qt.Key_J && !shifted) || event.text === "j"
                       || event.key === Qt.Key_Down) {
                moveCursor(1)
                event.accepted = true
            } else if ((event.key === Qt.Key_K && !shifted) || event.text === "k"
                       || event.key === Qt.Key_Up) {
                moveCursor(-1)
                event.accepted = true
            } else if ((event.key === Qt.Key_H && !shifted) || event.text === "h"
                       || event.key === Qt.Key_Left || event.key === Qt.Key_Backspace) {
                goParent()
                event.accepted = true
            } else if ((event.key === Qt.Key_L && !shifted) || event.text === "l"
                       || event.key === Qt.Key_Right
                       || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                activateCurrent()
                event.accepted = true
            } else if (upperG || event.text === "G") {
                selectIndex(fileModel.count - 1)
                event.accepted = true
            } else if (lowerG || event.text === "g") {
                pendingKey = "g"
                event.accepted = true
            } else if ((event.key === Qt.Key_D && shifted) || event.text === "D") {
                enterTrashPrompt()
                event.accepted = true
            } else if ((event.key === Qt.Key_E && shifted) || event.text === "E") {
                if (fileModel.trashView)
                    enterEmptyTrashPrompt()
                event.accepted = fileModel.trashView
            } else if ((event.key === Qt.Key_Y && !shifted) || event.text === "y") {
                pendingKey = "y"
                event.accepted = true
            } else if ((event.key === Qt.Key_D && !shifted) || event.text === "d") {
                pendingKey = "d"
                event.accepted = true
            } else if ((event.key === Qt.Key_A && !shifted) || event.text === "a") {
                pendingKey = "a"
                event.accepted = true
            } else if ((event.key === Qt.Key_P && !shifted) || event.text === "p") {
                pasteClipboard()
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                selectIndex(fileModel.count - 1)
                event.accepted = true
            } else if (event.key === Qt.Key_Home) {
                selectIndex(0)
                event.accepted = true
            } else if (event.key === Qt.Key_PageDown) {
                moveCursor(Math.max(1, Math.floor(files.height / rowHeight)))
                event.accepted = true
            } else if (event.key === Qt.Key_PageUp) {
                moveCursor(-Math.max(1, Math.floor(files.height / rowHeight)))
                event.accepted = true
            } else if (event.key === Qt.Key_Space) {
                openPreview()
                event.accepted = true
            } else if ((event.key === Qt.Key_X && !shifted) || event.text === "x") {
                toggleCurrentSelection()
                event.accepted = true
            } else if ((event.key === Qt.Key_V && !shifted) || event.text === "v") {
                toggleVisualMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_N && shifted) || event.text === "N") {
                enterTemplateMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_N && !shifted) || event.text === "n") {
                enterCreateMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_B && shifted) || event.text === "B") {
                sidebarVisible = !sidebarVisible
                if (!sidebarVisible)
                    placesMode = false
                event.accepted = true
            } else if ((event.key === Qt.Key_B && !shifted) || event.text === "b") {
                enterPlacesMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_M && !shifted) || event.text === "m") {
                addCurrentBookmark()
                event.accepted = true
            } else if (!shifted && (event.key === Qt.Key_T || event.text === "t")) {
                openCurrentInNewTab(true)
                event.accepted = true
            } else if ((event.key === Qt.Key_F && !shifted) || event.text === "f") {
                enterFinderMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_I && !shifted) || event.text === "i") {
                setGridMode(!gridMode)
                event.accepted = true
            } else if ((event.key === Qt.Key_O && !shifted) || event.text === "o") {
                openWithCurrent()
                event.accepted = true
            } else if ((event.key === Qt.Key_Z && !shifted) || event.text === "z") {
                openProperties()
                event.accepted = true
            } else if (event.key === Qt.Key_Exclam || event.text === "!") {
                openTerminalHere()
                event.accepted = true
            } else if ((event.key === Qt.Key_R && shifted) || event.text === "R") {
                enterBulkRenameMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_R && !shifted) || event.text === "r"
                       || event.key === Qt.Key_F2) {
                if (fileModel.trashView)
                    restoreSelection()
                else
                    enterRenameMode()
                event.accepted = true
            } else if ((event.key === Qt.Key_U && !shifted) || event.text === "u") {
                undoLastMutation()
                event.accepted = true
            } else if ((event.key === Qt.Key_Slash && !shifted) || event.text === "/") {
                enterFilterMode()
                event.accepted = true
            } else if (event.key === Qt.Key_Period || event.text === ".") {
                cursorPath = files.currentIndex >= 0 ? fileModel.pathAt(files.currentIndex) : ""
                fileModel.showHidden = !fileModel.showHidden
                event.accepted = true
            } else if ((event.key === Qt.Key_T && shifted) || event.text === "T") {
                fileModel.navigateToTrash()
                event.accepted = true
            } else if ((event.key === Qt.Key_S && !shifted) || event.text === "s") {
                cycleSort()
                event.accepted = true
            } else if ((event.key === Qt.Key_S && shifted) || event.text === "S") {
                fileModel.setSort(fileModel.sortField, true)
                event.accepted = true
            } else if (event.key === Qt.Key_AsciiTilde || event.text === "~") {
                navigate(fileModel.homePath)
                event.accepted = true
            } else if (event.key === Qt.Key_Question || (event.key === Qt.Key_Slash && shifted)
                       || event.text === "?") {
                helpVisible = true
                pendingKey = ""
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                if (pendingKey.length > 0)
                    pendingKey = ""
                else if (visualMode)
                    visualMode = false
                else if (clipboardMode === "move") {
                    fileModel.clearFileClipboardIfOwned()
                    clipboardPaths = []
                    clipboardMode = ""
                    showNotice("Cut cancelled.", false)
                }
                else if (selectedCount > 0)
                    clearSelection()
                else if (fileModel.filterText.length > 0)
                    fileModel.filterText = ""
                event.accepted = true
            }
        }

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: window.topHeight
            color: theme.darkBackground

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: window.alpha(theme.foreground, 0.14)
                z: 1
            }

            BreadcrumbBar {
                id: breadcrumbs
                visible: !root.filterMode && !root.locationMode && !root.networkMode
                         && !root.finderMode && !root.recentMode && !root.bookmarkRenameMode
                         && root.operationMode.length === 0
                enabled: !fileModel.trashView
                anchors.fill: parent
                path: fileModel.trashView ? "/Trash" : fileModel.currentPath
                themeObject: theme
                onNavigateRequested: function(path) { root.navigate(path) }
                onEditRequested: root.enterLocationMode()
            }

            Rectangle {
                id: filterSurface
                visible: root.filterMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: slash
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: "/"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                TextInput {
                    id: filterInput
                    anchors.left: slash.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(8 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    onTextChanged: fileModel.filterText = text

                    Keys.onEscapePressed: function(event) {
                        root.leaveFilterMode(true)
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        root.leaveFilterMode(false)
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        root.leaveFilterMode(false)
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: operationSurface
                visible: root.operationMode.length > 0
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: operationPrefix
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: root.operationMode === "create" ? "mkdir"
                          : (root.operationMode === "archive" ? "archive"
                             : (root.operationMode === "gather" ? "folder + move" : "rename"))
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                TextInput {
                    id: operationInput
                    anchors.left: operationPrefix.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true

                    Keys.onEscapePressed: function(event) {
                        root.leaveOperationMode()
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        root.commitOperation()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        root.commitOperation()
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: locationSurface
                visible: root.locationMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: locationPrefix
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: ":"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                TextInput {
                    id: locationInput
                    anchors.left: locationPrefix.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(8 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true

                    Keys.onEscapePressed: function(event) {
                        root.leaveLocationMode()
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        root.commitLocation()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        root.commitLocation()
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: bookmarkRenameSurface
                visible: root.bookmarkRenameMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: bookmarkRenamePrefix
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: "bookmark"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                TextInput {
                    id: bookmarkRenameInput
                    anchors.left: bookmarkRenamePrefix.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true

                    Keys.onEscapePressed: function(event) {
                        root.leaveBookmarkRename()
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        root.commitBookmarkRename()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        root.commitBookmarkRename()
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: finderSurface
                visible: root.finderMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: finderPrefix
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: "find"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                TextInput {
                    id: finderInput
                    anchors.left: finderPrefix.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    onTextChanged: searchModel.query = text

                    Keys.onPressed: function(event) {
                        var control = (event.modifiers & Qt.ControlModifier) !== 0
                        var alt = (event.modifiers & Qt.AltModifier) !== 0
                        if (event.key === Qt.Key_Escape) {
                            root.leaveFinderMode()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down
                                   || (control && (event.key === Qt.Key_J
                                                   || event.key === Qt.Key_N))) {
                            root.moveFinderCursor(1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up
                                   || (control && (event.key === Qt.Key_K
                                                   || event.key === Qt.Key_P))) {
                            root.moveFinderCursor(-1)
                            event.accepted = true
                        } else if (control && (event.key === Qt.Key_Return
                                               || event.key === Qt.Key_Enter)) {
                            root.revealFinderResult()
                            event.accepted = true
                        } else if (alt && (event.key === Qt.Key_Return
                                           || event.key === Qt.Key_Enter)) {
                            root.openFinderProperties()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.activateFinderResult()
                            event.accepted = true
                        } else if (alt && event.key === Qt.Key_T) {
                            root.cycleFinderType()
                            event.accepted = true
                        } else if (alt && event.key === Qt.Key_D) {
                            root.cycleFinderDate()
                            event.accepted = true
                        }
                    }
                }
            }

            Rectangle {
                visible: root.recentMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    text: "recent files  ·  j/k move  ·  Enter open  ·  Esc close"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                id: networkSurface
                visible: root.networkMode
                anchors.fill: parent
                color: window.alpha(theme.foreground, theme.hoverFillAlpha)

                Text {
                    id: networkPrefix
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    text: networkModel.promptActive ? networkModel.promptText : "connect"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, parent.width * 0.46)
                }

                TextInput {
                    id: networkInput
                    anchors.left: networkPrefix.right
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(10 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    color: theme.foreground
                    selectionColor: theme.accent
                    selectedTextColor: theme.darkBackground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    echoMode: networkModel.promptSecret ? TextInput.Password : TextInput.Normal

                    Keys.onEscapePressed: function(event) {
                        root.leaveNetworkMode()
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        root.commitNetworkInput()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: function(event) {
                        root.commitNetworkInput()
                        event.accepted = true
                    }
                }
            }

        }

        Rectangle {
            id: tabBar
            visible: height > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            height: window.tabBarHeight
            color: theme.darkBackground

            ListView {
                id: tabList
                anchors.left: parent.left
                anchors.right: newTabButton.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                orientation: ListView.Horizontal
                model: root.tabs
                clip: true
                currentIndex: root.currentTabIndex

                delegate: Rectangle {
                    id: tabDelegate
                    required property int index
                    required property var modelData
                    width: Math.min(Math.round(190 * window.uiScale),
                                    Math.max(Math.round(110 * window.uiScale),
                                             tabList.width / Math.max(1, root.tabs.length)))
                    height: tabList.height
                    color: index === root.currentTabIndex
                           ? theme.background : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.right: tabClose.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        anchors.rightMargin: Math.round(5 * window.uiScale)
                        text: tabDelegate.modelData.title
                        color: tabDelegate.index === root.currentTabIndex
                               ? theme.foreground : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 1)
                        elide: Text.ElideRight
                    }

                    Text {
                        id: tabClose
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: Math.round(7 * window.uiScale)
                        text: "×"
                        color: tabClosePointer.containsMouse ? theme.foreground : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize

                        MouseArea {
                            id: tabClosePointer
                            anchors.fill: parent
                            anchors.margins: -Math.round(5 * window.uiScale)
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.closeTab(tabDelegate.index)
                        }
                    }

                    MouseArea {
                        anchors.left: parent.left
                        anchors.right: tabClose.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.MiddleButton)
                                root.closeTab(tabDelegate.index)
                            else
                                root.switchTab(tabDelegate.index)
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: tabDelegate.index === root.currentTabIndex
                               ? theme.accent : window.alpha(theme.foreground, 0.12)
                    }
                }
            }

            Rectangle {
                id: newTabButton
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.round(34 * window.uiScale)
                color: newTabPointer.containsMouse
                       ? window.alpha(theme.foreground, theme.hoverFillAlpha) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize + 2
                }
                MouseArea {
                    id: newTabPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.duplicateCurrentTab()
                }
            }
        }

        Rectangle {
            id: sidebar
            visible: root.sidebarVisible
            anchors.left: parent.left
            anchors.top: tabBar.bottom
            anchors.bottom: statusBar.top
            width: window.sidebarWidth
            color: theme.darkBackground

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: window.alpha(theme.foreground, 0.14)
            }

            Text {
                id: placesHeading
                anchors.left: parent.left
                anchors.right: hidePlaces.left
                anchors.top: parent.top
                height: window.columnHeaderHeight
                anchors.leftMargin: Math.round(10 * window.uiScale)
                text: "PLACES"
                color: root.placesMode ? theme.accent : theme.muted
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)
                font.bold: root.placesMode
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                id: hidePlaces
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.round(54 * window.uiScale)
                anchors.rightMargin: Math.round(9 * window.uiScale)
                text: "B HIDE"
                color: hidePlacesPointer.containsMouse ? theme.foreground : theme.muted
                font.family: theme.fontFamily
                font.pixelSize: Math.max(8, theme.fontSize - 2)
                verticalAlignment: Text.AlignVCenter

                MouseArea {
                    id: hidePlacesPointer
                    anchors.fill: parent
                    anchors.margins: -Math.round(4 * window.uiScale)
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.sidebarVisible = false
                        root.placesMode = false
                        files.forceActiveFocus()
                    }
                }
            }

            ListView {
                id: places
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: placesHeading.bottom
                anchors.bottom: parent.bottom
                anchors.margins: Math.round(5 * window.uiScale)
                clip: true
                model: placesModel
                currentIndex: placesModel.count > 0 ? 0 : -1
                reuseItems: true
                section.property: "placeSection"
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section
                    width: places.width
                    height: section === "PLACES"
                            ? Math.round(4 * window.uiScale)
                            : Math.round(23 * window.uiScale)

                    Text {
                        visible: parent.section !== "PLACES"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        anchors.rightMargin: Math.round(8 * window.uiScale)
                        anchors.bottomMargin: Math.round(4 * window.uiScale)
                        text: parent.section
                        color: window.alpha(theme.muted, 0.72)
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(8, theme.fontSize - 2)
                        font.letterSpacing: 0.5
                    }
                }

                delegate: Rectangle {
                    id: placeRow
                    required property int index
                    required property string placeLabel
                    required property string placePath
                    required property string placeKind
                    required property bool placeIsTrash
                    required property bool placeIsVolume
                    required property bool placeIsBookmark
                    required property string placeDevicePath
                    required property bool placeIsMounted
                    required property bool placeIsEjectable
                    required property bool placeIsNetwork
                    required property string placeNetworkUri
                    required property bool placeIsRecent

                    readonly property string placeIconName: {
                        if (placeIsTrash)
                            return "user-trash-symbolic"
                        if (placeIsRecent)
                            return "document-open-recent-symbolic"
                        if (placeIsNetwork)
                            return "network-server-symbolic"
                        if (placeKind === "device")
                            return "drive-removable-media-symbolic"
                        if (placeIsVolume)
                            return "drive-harddisk-symbolic"
                        if (placeIsBookmark)
                            return "starred-symbolic"
                        if (placeKind === "home")
                            return "user-home-symbolic"
                        if (placeLabel === "Documents")
                            return "folder-documents-symbolic"
                        if (placeLabel === "Downloads")
                            return "folder-download-symbolic"
                        if (placeLabel === "Pictures")
                            return "folder-pictures-symbolic"
                        if (placeLabel === "Music")
                            return "folder-music-symbolic"
                        if (placeLabel === "Videos")
                            return "folder-videos-symbolic"
                        if (placeLabel === "Templates")
                            return "folder-templates-symbolic"
                        return "folder-symbolic"
                    }
                    readonly property color placeIconColor:
                        ListView.isCurrentItem && root.placesMode
                        ? theme.foreground
                        : (placeRow.index === root.activePlaceIndex ? theme.accent : theme.muted)
                    readonly property bool placeIsActive:
                        placeRow.index === root.activePlaceIndex

                    width: places.width
                    height: Math.round(31 * window.uiScale)
                    color: placeDrop.containsDrag
                           ? window.alpha(theme.accent, 0.18)
                           : (ListView.isCurrentItem && root.placesMode
                              ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                              : (placeRow.placeIsActive
                                 ? window.alpha(theme.accent, 0.075)
                                 : (placePointer.containsMouse
                                 ? window.alpha(theme.foreground, 0.035) : "transparent"))
                              )
                    border.width: ListView.isCurrentItem && root.placesMode ? 1 : 0
                    border.color: window.alpha(theme.foreground, theme.hoverBorderAlpha)
                    radius: theme.cornerRadius

                    Rectangle {
                        visible: placeRow.placeIsActive
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: theme.accent
                        radius: theme.cornerRadius
                    }

                    Image {
                        id: placeIcon
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        width: Math.round(16 * window.uiScale)
                        height: width
                        source: "image://fileicon/theme/" + placeRow.placeIconName + "/"
                                + placeRow.placeIconColor.toString().replace("#", "")
                                + "?revision=" + theme.revision
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Math.round(34 * window.uiScale)
                        anchors.rightMargin: Math.round(8 * window.uiScale)
                        text: placeRow.placeLabel
                        color: ListView.isCurrentItem && root.placesMode
                               ? theme.foreground
                               : (placeRow.placeIsActive ? theme.accent : theme.muted)
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 1)
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: placePointer
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            places.currentIndex = placeRow.index
                            if (mouse.button === Qt.RightButton) {
                                var point = mapToItem(root, mouse.x, mouse.y)
                                placeMenu.row = placeRow.index
                                placeMenu.x = Math.round(point.x)
                                placeMenu.y = Math.round(point.y)
                                placeMenu.open()
                            } else {
                                root.activatePlace(placeRow.index)
                            }
                        }
                    }

                    DropArea {
                        id: placeDrop
                        anchors.fill: parent
                        enabled: !placeRow.placeIsTrash && placeRow.placePath.length > 0
                        onEntered: function(drag) {
                            var move = drag.proposedAction === Qt.MoveAction
                            drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
                            root.dropDestination = placeRow.placePath
                            root.dropIntent = move ? "MOVE" : "COPY"
                        }
                        onExited: {
                            root.dropDestination = ""
                            root.dropIntent = ""
                        }
                        onDropped: function(drop) {
                            var move = drop.proposedAction === Qt.MoveAction
                            if (root.handleDrop(drop.urls, placeRow.placePath, move))
                                drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
                            else
                                drop.accepted = false
                            root.dropDestination = ""
                            root.dropIntent = ""
                        }
                    }
                }
            }
        }

        Rectangle {
            id: columnHeader
            visible: !root.finderMode && !root.recentMode && !root.gridMode
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: tabBar.bottom
            height: window.columnHeaderHeight
            color: theme.background

            component HeaderCell: Item {
                id: headerCell
                required property string label
                required property int field

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(11 * window.uiScale)
                    anchors.rightMargin: Math.round(8 * window.uiScale)
                    text: headerCell.label
                          + (!fileModel.trashView && fileModel.sortField === headerCell.field
                             ? (fileModel.sortAscending ? "  ↑" : "  ↓") : "")
                    color: !fileModel.trashView && fileModel.sortField === headerCell.field
                           ? theme.accent : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    font.bold: !fileModel.trashView && fileModel.sortField === headerCell.field
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !fileModel.trashView
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: fileModel.setSort(headerCell.field, true)
                }
            }

            HeaderCell {
                x: 0
                width: window.nameColumnWidth
                height: parent.height
                label: "NAME"
                field: 0
            }
            HeaderCell {
                x: window.nameColumnWidth
                width: window.sizeColumnWidth
                height: parent.height
                label: "SIZE"
                field: 1
            }
            HeaderCell {
                x: window.nameColumnWidth + window.sizeColumnWidth
                width: window.typeColumnWidth
                height: parent.height
                label: fileModel.trashView ? "ORIGINAL LOCATION" : "TYPE"
                field: 2
            }
            HeaderCell {
                x: window.nameColumnWidth + window.sizeColumnWidth + window.typeColumnWidth
                width: window.modifiedColumnWidth
                height: parent.height
                label: fileModel.trashView ? "DELETED" : "MODIFIED"
                field: 3
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: window.alpha(theme.foreground, 0.12)
            }
        }

        ListView {
            id: files
            visible: !root.finderMode && !root.recentMode && !root.gridMode
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: columnHeader.bottom
            anchors.bottom: (fileModel.transferActive || fileModel.trashActive)
                            ? transferBar.top : statusBar.top
            anchors.leftMargin: Math.round(6 * window.uiScale)
            anchors.rightMargin: Math.round(6 * window.uiScale)
            clip: true
            model: fileModel
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: window.rowHeight * 4
            keyNavigationEnabled: false

            delegate: FileRow {
                width: files.width
                height: window.rowHeight
                cursor: ListView.isCurrentItem
                selected: root.isPathSelected(filePath, root.selectionRevision)
                pendingMove: root.clipboardMode === "move"
                             && root.clipboardPaths.indexOf(filePath) >= 0
                dragPathsProvider: function() {
                    return root.dragPathsFor(filePath, root.selectionRevision)
                }
                acceptsDrop: isDirectory && !fileModel.trashView
                dropDestination: filePath
                dropHandler: root.handleDrop
                themeObject: theme
                nameColumnWidth: window.nameColumnWidth - Math.round(12 * window.uiScale)
                sizeColumnWidth: window.sizeColumnWidth
                typeColumnWidth: window.typeColumnWidth
                modifiedColumnWidth: window.modifiedColumnWidth
                onChosen: function(row, modifiers) {
                    root.choosePointer(row, modifiers)
                    files.forceActiveFocus()
                }
                onActivated: function(row) {
                    root.selectIndex(row)
                    root.activateCurrent()
                    files.forceActiveFocus()
                }
                onContextRequested: function(row, x, y) {
                    var point = mapToItem(root, x, y)
                    root.openContextMenu(row, point.x, point.y)
                }
                onNewTabRequested: function(row) {
                    root.selectIndex(row, false)
                    root.openCurrentInNewTab(false)
                    files.forceActiveFocus()
                }
                onDropEntered: function(destination, move) {
                    root.dropDestination = destination
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onDropExited: {
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
                onDragStateChanged: function(active, move) {
                    root.dragActive = active
                    if (!active) {
                        root.dropDestination = ""
                        root.dropIntent = ""
                    } else {
                        root.dropIntent = move ? "MOVE" : "COPY"
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                z: -1
                enabled: !fileModel.trashView
                onEntered: function(drag) {
                    var move = drag.proposedAction === Qt.MoveAction
                    drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    root.dropDestination = fileModel.currentPath
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onPositionChanged: function(drag) {
                    var move = drag.proposedAction === Qt.MoveAction
                    drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    root.dropDestination = fileModel.currentPath
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onExited: {
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
                onDropped: function(drop) {
                    var move = drop.proposedAction === Qt.MoveAction
                    if (root.handleDrop(drop.urls, fileModel.currentPath, move))
                        drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    else
                        drop.accepted = false
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
            }

            Controls.ScrollBar.vertical: Controls.ScrollBar {
                id: scrollBar
                width: Math.round(7 * window.uiScale)
                policy: Controls.ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: scrollBar.width
                    radius: width / 2
                    color: scrollBar.pressed
                           ? theme.accent
                           : window.alpha(theme.foreground, scrollBar.hovered ? 0.42 : 0.24)
                }
                background: Item {}
            }
        }

        GridView {
            id: grid
            visible: !root.finderMode && !root.recentMode && root.gridMode
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: tabBar.bottom
            anchors.bottom: (fileModel.transferActive || fileModel.trashActive)
                            ? transferBar.top : statusBar.top
            anchors.margins: Math.round(6 * window.uiScale)
            clip: true
            model: fileModel
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: cellHeight * 2
            cellWidth: Math.round(148 * window.uiScale)
            cellHeight: Math.round(116 * window.uiScale)
            keyNavigationEnabled: false

            delegate: FileGridItem {
                width: grid.cellWidth - Math.round(7 * window.uiScale)
                height: grid.cellHeight - Math.round(7 * window.uiScale)
                cursor: index === files.currentIndex
                selected: root.isPathSelected(filePath, root.selectionRevision)
                pendingMove: root.clipboardMode === "move"
                             && root.clipboardPaths.indexOf(filePath) >= 0
                dragPathsProvider: function() {
                    return root.dragPathsFor(filePath, root.selectionRevision)
                }
                acceptsDrop: isDirectory && !fileModel.trashView
                dropDestination: filePath
                dropHandler: root.handleDrop
                themeObject: theme
                onChosen: function(row, modifiers) {
                    root.choosePointer(row, modifiers)
                    files.forceActiveFocus()
                }
                onActivated: function(row) {
                    root.selectIndex(row)
                    root.activateCurrent()
                    files.forceActiveFocus()
                }
                onContextRequested: function(row, x, y) {
                    var point = mapToItem(root, x, y)
                    root.openContextMenu(row, point.x, point.y)
                }
                onNewTabRequested: function(row) {
                    root.selectIndex(row, false)
                    root.openCurrentInNewTab(false)
                    files.forceActiveFocus()
                }
                onDropEntered: function(destination, move) {
                    root.dropDestination = destination
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onDropExited: {
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
                onDragStateChanged: function(active, move) {
                    root.dragActive = active
                    if (!active) {
                        root.dropDestination = ""
                        root.dropIntent = ""
                    } else {
                        root.dropIntent = move ? "MOVE" : "COPY"
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                z: -1
                enabled: !fileModel.trashView
                onEntered: function(drag) {
                    var move = drag.proposedAction === Qt.MoveAction
                    drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    root.dropDestination = fileModel.currentPath
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onPositionChanged: function(drag) {
                    var move = drag.proposedAction === Qt.MoveAction
                    drag.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    root.dropDestination = fileModel.currentPath
                    root.dropIntent = move ? "MOVE" : "COPY"
                }
                onExited: {
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
                onDropped: function(drop) {
                    var move = drop.proposedAction === Qt.MoveAction
                    if (root.handleDrop(drop.urls, fileModel.currentPath, move))
                        drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    else
                        drop.accepted = false
                    root.dropDestination = ""
                    root.dropIntent = ""
                }
            }

            Controls.ScrollBar.vertical: Controls.ScrollBar {}
        }

        Item {
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: root.gridMode ? tabBar.bottom : columnHeader.bottom
            anchors.bottom: statusBar.top
            visible: !root.finderMode && !root.recentMode && fileModel.count === 0

            Text {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 520)
                text: fileModel.loading
                      ? "Loading…"
                      : (fileModel.filterText.length > 0
                         ? "No matching files"
                         : (fileModel.trashView ? "Trash is empty" : "This directory is empty"))
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: theme.fontSize
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            id: finderBody
            visible: root.finderMode || root.recentMode
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: tabBar.bottom
            anchors.bottom: statusBar.top
            color: theme.background

            Rectangle {
                id: finderHeader
                objectName: "finderHeader"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: window.columnHeaderHeight
                        + (root.recentMode ? 0 : Math.round(27 * window.uiScale))
                color: theme.background

                Item {
                    id: finderScopeRow
                    objectName: "finderScopeRow"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: window.columnHeaderHeight

                    Text {
                        anchors.left: parent.left
                        anchors.right: finderScanState.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Math.round(11 * window.uiScale)
                        anchors.rightMargin: Math.round(12 * window.uiScale)
                        text: root.recentMode ? "DESKTOP RECENT FILES"
                                              : "BELOW  " + searchModel.basePath
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 1)
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    Text {
                        id: finderScanState
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        text: root.recentMode ? recentModel.count + " RECENT"
                              : (searchModel.scanning ? "SCANNING  " + searchModel.scannedCount
                                                     : searchModel.scannedCount + " SCANNED")
                        color: !root.recentMode && searchModel.scanning
                               ? theme.accent : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 1)
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Row {
                    id: finderFilters
                    objectName: "finderFilters"
                    visible: !root.recentMode
                    anchors.left: parent.left
                    anchors.top: finderScopeRow.bottom
                    anchors.leftMargin: Math.round(8 * window.uiScale)
                    anchors.topMargin: Math.round(3 * window.uiScale)
                    height: Math.round(20 * window.uiScale)
                    spacing: Math.round(7 * window.uiScale)

                    Rectangle {
                        width: Math.round(178 * window.uiScale)
                        height: parent.height
                        color: finderTypePointer.containsMouse
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : window.alpha(theme.foreground, 0.045)
                        border.width: 1
                        border.color: window.alpha(theme.foreground, 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "Alt-t  TYPE  " + searchModel.typeFilter.toUpperCase()
                            color: searchModel.typeFilter === "all" ? theme.muted : theme.accent
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(8, theme.fontSize - 2)
                        }
                        MouseArea {
                            id: finderTypePointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cycleFinderType()
                        }
                    }

                    Rectangle {
                        width: Math.round(164 * window.uiScale)
                        height: parent.height
                        color: finderDatePointer.containsMouse
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : window.alpha(theme.foreground, 0.045)
                        border.width: 1
                        border.color: window.alpha(theme.foreground, 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "Alt-d  DATE  " + root.finderDateLabel()
                            color: searchModel.modifiedWithinDays === 0 ? theme.muted : theme.accent
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(8, theme.fontSize - 2)
                        }
                        MouseArea {
                            id: finderDatePointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cycleFinderDate()
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: window.alpha(theme.foreground, 0.12)
                }
            }

            ListView {
                id: finderResults
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: finderHeader.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: Math.round(6 * window.uiScale)
                anchors.rightMargin: Math.round(6 * window.uiScale)
                anchors.topMargin: Math.round(6 * window.uiScale)
                model: root.recentMode ? recentModel : searchModel
                currentIndex: -1
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: searchRow
                    required property int index
                    required property string searchName
                    required property string searchPath
                    required property string searchRelativePath
                    required property bool searchIsDirectory
                    required property string searchIconSource

                    width: finderResults.width
                    height: Math.round(42 * window.uiScale)
                    color: ListView.isCurrentItem
                           ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                           : (searchPointer.containsMouse
                              ? window.alpha(theme.foreground, 0.035) : "transparent")
                    border.width: ListView.isCurrentItem ? 1 : 0
                    border.color: window.alpha(theme.foreground, theme.hoverBorderAlpha)
                    radius: theme.cornerRadius

                    Image {
                        id: searchIcon
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        width: Math.round(20 * window.uiScale)
                        height: width
                        source: searchRow.searchIconSource + "?revision=" + theme.revision
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        objectName: "finderResultName"
                        anchors.left: searchIcon.right
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        anchors.topMargin: Math.round(5 * window.uiScale)
                        text: root.recentMode
                              ? root.escapeStyledText(searchRow.searchName)
                              : root.styledFuzzyText(searchRow.searchName, finderInput.text)
                        textFormat: Text.StyledText
                        color: ListView.isCurrentItem ? theme.foreground : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Math.round(39 * window.uiScale)
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        anchors.bottomMargin: Math.round(4 * window.uiScale)
                        text: root.recentMode
                              ? root.escapeStyledText(searchRow.searchRelativePath)
                              : root.styledFuzzyText(searchRow.searchRelativePath,
                                                     finderInput.text)
                        textFormat: Text.StyledText
                        color: window.alpha(theme.muted, 0.74)
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(8, theme.fontSize - 2)
                        elide: Text.ElideMiddle
                    }

                    MouseArea {
                        id: searchPointer
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: finderResults.currentIndex = searchRow.index
                        onDoubleClicked: {
                            finderResults.currentIndex = searchRow.index
                            root.activateFinderResult()
                        }
                    }
                }

                Controls.ScrollBar.vertical: Controls.ScrollBar {}
            }

            Text {
                anchors.centerIn: finderResults
                visible: (root.recentMode ? recentModel.count : searchModel.count) === 0
                width: Math.min(parent.width - 40, 520)
                text: root.recentMode
                      ? (recentModel.errorMessage.length > 0
                         ? recentModel.errorMessage
                         : (recentModel.loading ? "Loading recent files…"
                                                : "No recent files"))
                      : (searchModel.errorMessage.length > 0
                      ? searchModel.errorMessage
                      : (searchModel.scanning ? "Scanning…"
                                              : (finderInput.text.length > 0
                                                 ? "No fuzzy matches" : "Nothing below this folder")))
                color: (root.recentMode ? recentModel.errorMessage.length > 0
                                        : searchModel.errorMessage.length > 0)
                       ? theme.errorColor : theme.muted
                font.family: theme.fontFamily
                font.pixelSize: theme.fontSize
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            id: transferBar
            visible: fileModel.transferActive || fileModel.trashActive
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.bottom: statusBar.top
            height: window.transferHeight
            color: theme.darkBackground

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: window.alpha(theme.foreground, 0.14)
            }

            Text {
                id: transferTitle
                anchors.left: parent.left
                anchors.right: transferAction.left
                anchors.top: parent.top
                anchors.leftMargin: Math.round(10 * window.uiScale)
                anchors.rightMargin: Math.round(12 * window.uiScale)
                anchors.topMargin: Math.round(6 * window.uiScale)
                text: fileModel.trashActive
                      ? (root.emptyTrashInProgress ? "EMPTY TRASH" : "TRASH")
                      : (fileModel.undoActive
                         ? "UNDO  →  PREVIOUS LOCATION"
                         : (root.restoreInProgress
                         ? "RESTORE  →  ORIGINAL LOCATION"
                         : (fileModel.transferMove ? "MOVE" : "COPY") + "  →  "
                           + fileModel.transferDestination))
                color: theme.foreground
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)
                elide: Text.ElideMiddle
            }

            Text {
                anchors.left: parent.left
                anchors.right: transferAction.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: Math.round(10 * window.uiScale)
                anchors.rightMargin: Math.round(12 * window.uiScale)
                anchors.bottomMargin: Math.round(6 * window.uiScale)
                text: fileModel.trashActive
                      ? (fileModel.trashCurrentPath.length > 0
                         ? fileModel.trashCurrentPath : "Preparing Trash")
                      : (fileModel.transferCurrentPath.length > 0
                         ? fileModel.transferCurrentPath : fileModel.transferPhase)
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 2)
                elide: Text.ElideMiddle
            }

            Text {
                id: transferAction
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Math.round(10 * window.uiScale)
                text: (fileModel.trashActive
                       ? Math.round(fileModel.trashProgress * 100) + "%"
                       : (fileModel.transferProgress < 0
                          ? fileModel.transferPhase.toUpperCase()
                          : Math.round(fileModel.transferProgress * 100) + "%"))
                      + "  ·  ESC CANCEL"
                color: theme.accent
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Math.round(6 * window.uiScale)
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (fileModel.trashActive)
                            fileModel.cancelTrash()
                        else
                            fileModel.cancelTransfer()
                    }
                }
            }

            Rectangle {
                visible: fileModel.trashActive || fileModel.transferProgress >= 0
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                height: 2
                width: parent.width * (fileModel.trashActive
                                       ? fileModel.trashProgress : fileModel.transferProgress)
                color: theme.accent
            }
        }

        Rectangle {
            id: statusBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: window.statusHeight
            color: theme.darkBackground

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: window.alpha(theme.foreground, 0.14)
            }

            Rectangle {
                id: modeIndicator
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.round(82 * window.uiScale)
                color: root.modeName !== "NORMAL"
                       ? window.alpha(theme.accent, 0.18)
                       : window.alpha(theme.foreground, 0.06)

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: root.modeName !== "NORMAL"
                           ? window.alpha(theme.accent, 0.65)
                           : window.alpha(theme.foreground, 0.14)
                }

                Text {
                    anchors.centerIn: parent
                    text: root.modeName
                    color: root.modeName !== "NORMAL" ? theme.accent : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    font.bold: true
                    font.letterSpacing: 0.7
                }
            }

            Text {
                anchors.left: modeIndicator.right
                anchors.right: statusRight.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Math.round(10 * window.uiScale)
                anchors.rightMargin: Math.round(12 * window.uiScale)
                text: fileModel.errorMessage.length > 0
                      ? fileModel.errorMessage
                      : (root.noticeMessage.length > 0
                         ? root.noticeMessage
                         : (archiveModel.active
                         ? archiveModel.description + "  ·  Esc to cancel"
                         : (root.recentMode
                         ? "j/k move  ·  Enter open  ·  Ctrl-Enter reveal  ·  Alt-Enter properties"
                         : (root.finderMode
                         ? "Type to find  ·  Ctrl-j/k move  ·  Enter open  ·  Ctrl-Enter reveal  ·  Alt-Enter properties"
                         : (root.dropDestination.length > 0
                         ? root.dropIntent + "  →  " + root.dropDestination
                         : (root.operationMode.length > 0
                         ? "Enter to confirm  ·  Esc to cancel"
                         : (root.pendingKey.length > 0
                         ? "pending: " + root.pendingKey
                         : (fileModel.trashView
                            ? "r restore  ·  E empty  ·  h return"
                            : (files.currentIndex >= 0
                               ? fileModel.pathAt(files.currentIndex) : fileModel.currentPath)))))))))
                color: (fileModel.errorMessage.length > 0
                        || (root.noticeMessage.length > 0 && root.noticeIsError))
                       ? theme.errorColor
                       : ((root.operationMode.length > 0 || root.pendingKey.length > 0)
                          ? theme.accent : theme.muted)
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)
                elide: Text.ElideMiddle
            }

            Text {
                id: statusRight
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Math.round(10 * window.uiScale)
                text: root.recentMode
                      ? recentModel.count + " RECENT"
                      : root.finderMode
                      ? searchModel.count + " RESULTS  ·  " + searchModel.scannedCount + " SCANNED"
                      : (fileModel.loading ? "LOADING  ·  " : "")
                      + fileModel.count + (fileModel.count === 1 ? " ITEM" : " ITEMS")
                      + (root.selectedCount > 0 ? "  ·  " + root.selectedCount + " SELECTED" : "")
                      + (root.clipboardPaths.length > 0
                         ? "  ·  " + root.clipboardPaths.length
                           + (root.clipboardMode === "move" ? " CUT" : " YANKED") : "")
                      + (fileModel.showHidden ? "  ·  HIDDEN" : "")
                      + (fileModel.filterText.length > 0 ? "  ·  FILTERED" : "")
                      + (fileModel.trashView ? "  ·  TRASH" : "")
                      + (fileModel.canUndo ? "  ·  u UNDO" : "")
                color: fileModel.loading ? theme.accent : theme.muted
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)
            }
        }

        Rectangle {
            id: trashOverlay
            anchors.fill: parent
            visible: root.trashPromptVisible
            z: 150
            color: window.alpha(theme.background, 0.82)

            MouseArea {
                anchors.fill: parent
                onClicked: files.forceActiveFocus()
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(40 * window.uiScale),
                                Math.round(580 * window.uiScale))
                height: Math.round(196 * window.uiScale)
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                Text {
                    id: trashTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: Math.round(18 * window.uiScale)
                    anchors.rightMargin: Math.round(18 * window.uiScale)
                    anchors.topMargin: Math.round(17 * window.uiScale)
                    text: "Move " + root.trashPromptPaths.length + " item"
                          + (root.trashPromptPaths.length === 1 ? "" : "s") + " to Trash?"
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                Text {
                    anchors.left: trashTitle.left
                    anchors.right: trashTitle.right
                    anchors.top: trashTitle.bottom
                    anchors.topMargin: Math.round(10 * window.uiScale)
                    text: root.trashPromptPaths.length === 1
                          ? root.trashPromptPaths[0]
                          : "The selected items will be removed from this folder."
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    elide: Text.ElideMiddle
                }

                Text {
                    anchors.left: trashTitle.left
                    anchors.right: trashTitle.right
                    anchors.top: trashTitle.bottom
                    anchors.topMargin: Math.round(38 * window.uiScale)
                    text: "Nothing is permanently deleted. Items can be restored from Trash."
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                    elide: Text.ElideRight
                }

                Row {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(18 * window.uiScale)
                    anchors.rightMargin: Math.round(18 * window.uiScale)
                    anchors.bottomMargin: Math.round(18 * window.uiScale)
                    height: Math.round(36 * window.uiScale)
                    spacing: Math.round(8 * window.uiScale)

                    Rectangle {
                        width: (parent.width - parent.spacing) / 2
                        height: parent.height
                        color: trashConfirmPointer.containsMouse
                               ? window.alpha(theme.errorColor, 0.16)
                               : window.alpha(theme.foreground, 0.045)
                        border.width: 1
                        border.color: trashConfirmPointer.containsMouse
                                      ? theme.errorColor
                                      : window.alpha(theme.foreground, 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "y  MOVE TO TRASH"
                            color: theme.errorColor
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            font.bold: true
                        }

                        MouseArea {
                            id: trashConfirmPointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.confirmTrash()
                        }
                    }

                    Rectangle {
                        width: (parent.width - parent.spacing) / 2
                        height: parent.height
                        color: trashCancelPointer.containsMouse
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : window.alpha(theme.foreground, 0.045)
                        border.width: 1
                        border.color: window.alpha(theme.foreground,
                                                   trashCancelPointer.containsMouse
                                                   ? theme.hoverBorderAlpha : 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "Esc  CANCEL"
                            color: theme.foreground
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            font.bold: true
                        }

                        MouseArea {
                            id: trashCancelPointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.leaveTrashPrompt()
                        }
                    }
                }
            }
        }

        component ContextMenuItem: Controls.MenuItem {
            id: contextItem
            height: Math.round(30 * window.uiScale)
            leftPadding: Math.round(10 * window.uiScale)
            rightPadding: Math.round(10 * window.uiScale)
            contentItem: Text {
                text: contextItem.text
                color: contextItem.enabled ? theme.foreground : theme.muted
                opacity: contextItem.enabled ? 1 : 0.45
                font.family: theme.fontFamily
                font.pixelSize: Math.max(9, theme.fontSize - 1)
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: contextItem.highlighted
                       ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                       : "transparent"
                radius: theme.cornerRadius
            }
        }

        Controls.Menu {
            id: contextMenu
            width: Math.round(220 * window.uiScale)
            padding: Math.round(5 * window.uiScale)
            modal: false

            background: Rectangle {
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.22)
                radius: theme.cornerRadius
            }

            ContextMenuItem {
                text: "OPEN                         l"
                enabled: files.currentIndex >= 0
                onTriggered: root.activateCurrent()
            }
            ContextMenuItem {
                text: "OPEN IN NEW TAB               t"
                enabled: files.currentIndex >= 0 && !fileModel.trashView
                         && fileModel.isDirectoryAt(files.currentIndex)
                onTriggered: root.openCurrentInNewTab(true)
            }
            ContextMenuItem {
                text: "QUICK LOOK                Space"
                enabled: files.currentIndex >= 0
                onTriggered: root.openPreview()
            }
            ContextMenuItem {
                text: "OPEN WITH                    o"
                enabled: files.currentIndex >= 0 && !fileModel.isDirectoryAt(files.currentIndex)
                onTriggered: root.openWithCurrent()
            }
            ContextMenuItem {
                text: "PROPERTIES            Alt-Enter"
                enabled: files.currentIndex >= 0
                onTriggered: root.openProperties()
            }
            ContextMenuItem {
                text: "COPY PATH                   yp"
                enabled: files.currentIndex >= 0
                onTriggered: root.copyCurrentPaths()
            }
            ContextMenuItem {
                text: "OPEN TERMINAL HERE            !"
                enabled: !fileModel.trashView
                onTriggered: root.openTerminalHere()
            }
            ContextMenuItem {
                text: fileModel.trashView ? "RESTORE                      r"
                                          : "RENAME                       r"
                enabled: files.currentIndex >= 0 && root.selectedCount <= 1
                onTriggered: fileModel.trashView ? root.restoreSelection()
                                                 : root.enterRenameMode()
            }
            ContextMenuItem {
                text: "BULK RENAME                   R"
                enabled: !fileModel.trashView && root.selectedCount >= 2
                         && !archiveModel.active && !fileModel.transferActive
                         && !fileModel.trashActive
                onTriggered: root.enterBulkRenameMode()
            }
            ContextMenuItem {
                text: "NEW FOLDER                   n"
                enabled: !fileModel.trashView
                onTriggered: root.enterCreateMode()
            }
            ContextMenuItem {
                text: "NEW FROM TEMPLATE             N"
                enabled: !fileModel.trashView
                onTriggered: root.enterTemplateMode()
            }
            ContextMenuItem {
                text: "NEW FOLDER WITH SELECTION     gn"
                enabled: !fileModel.trashView && root.selectedCount > 0
                onTriggered: root.enterFolderWithSelectionMode()
            }
            ContextMenuItem {
                text: "CREATE ARCHIVE               ac"
                enabled: !fileModel.trashView && files.currentIndex >= 0
                onTriggered: root.enterArchiveCreateMode()
            }
            ContextMenuItem {
                text: "EXTRACT HERE                 ax"
                enabled: !fileModel.trashView && root.currentOperationPaths().length === 1
                         && archiveModel.supportsArchive(root.currentOperationPaths()[0])
                onTriggered: root.extractCurrentArchive()
            }
            Controls.MenuSeparator {}
            ContextMenuItem {
                text: "COPY                         yy"
                enabled: !fileModel.trashView && files.currentIndex >= 0
                onTriggered: root.stageClipboard("copy")
            }
            ContextMenuItem {
                text: "CUT                          dd"
                enabled: !fileModel.trashView && files.currentIndex >= 0
                onTriggered: root.stageClipboard("move")
            }
            ContextMenuItem {
                text: "PASTE HERE                    p"
                enabled: !fileModel.trashView && root.clipboardPaths.length > 0
                onTriggered: root.pasteClipboard()
            }
            ContextMenuItem {
                text: "MOVE TO TRASH                 D"
                enabled: !fileModel.trashView && files.currentIndex >= 0
                onTriggered: root.enterTrashPrompt()
            }
            Controls.MenuSeparator {}
            ContextMenuItem {
                text: "UNDO                          u"
                enabled: fileModel.canUndo
                onTriggered: root.undoLastMutation()
            }
        }

        Controls.Menu {
            id: placeMenu
            property int row: -1
            width: Math.round(220 * window.uiScale)
            padding: Math.round(5 * window.uiScale)
            modal: false

            background: Rectangle {
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.22)
                radius: theme.cornerRadius
            }

            ContextMenuItem {
                text: "BOOKMARK CURRENT             m"
                enabled: !fileModel.trashView
                onTriggered: root.addCurrentBookmark()
            }
            Controls.MenuSeparator {}
            ContextMenuItem {
                text: "RENAME                       r"
                enabled: placesModel.isBookmarkAt(placeMenu.row)
                onTriggered: root.enterBookmarkRename(placeMenu.row)
            }
            ContextMenuItem {
                text: "MOVE UP                      K"
                enabled: placesModel.isBookmarkAt(placeMenu.row)
                onTriggered: root.moveSelectedBookmark(-1)
            }
            ContextMenuItem {
                text: "MOVE DOWN                    J"
                enabled: placesModel.isBookmarkAt(placeMenu.row)
                onTriggered: root.moveSelectedBookmark(1)
            }
            ContextMenuItem {
                text: "REMOVE                       d"
                enabled: placesModel.isBookmarkAt(placeMenu.row)
                onTriggered: root.removeSelectedBookmark()
            }
            Controls.MenuSeparator {}
            ContextMenuItem {
                text: "MOUNT                    Enter"
                enabled: placesModel.isDeviceAt(placeMenu.row)
                         && !placesModel.isMountedAt(placeMenu.row)
                onTriggered: placesModel.mountAt(placeMenu.row)
            }
            ContextMenuItem {
                text: "UNMOUNT                      u"
                enabled: placesModel.isDeviceAt(placeMenu.row)
                         && placesModel.isMountedAt(placeMenu.row)
                onTriggered: placesModel.unmountAt(placeMenu.row)
            }
            ContextMenuItem {
                text: "EJECT                        e"
                enabled: placesModel.isEjectableAt(placeMenu.row)
                onTriggered: placesModel.ejectAt(placeMenu.row)
            }
            Controls.MenuSeparator {}
            ContextMenuItem {
                text: "CONNECT                  Enter"
                enabled: placesModel.isNetworkAt(placeMenu.row)
                         && !placesModel.isMountedAt(placeMenu.row)
                onTriggered: root.enterNetworkMode(placesModel.networkUriAt(placeMenu.row))
            }
            ContextMenuItem {
                text: "DISCONNECT                   x"
                enabled: placesModel.isNetworkAt(placeMenu.row)
                         && placesModel.isMountedAt(placeMenu.row)
                onTriggered: networkModel.disconnectFrom(
                                 placesModel.networkUriAt(placeMenu.row))
            }
        }

        Rectangle {
            id: emptyTrashOverlay
            anchors.fill: parent
            visible: root.emptyTrashPromptVisible
            z: 160
            color: window.alpha(theme.background, 0.86)

            MouseArea {
                anchors.fill: parent
                onClicked: emptyTrashInput.forceActiveFocus()
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(40 * window.uiScale),
                                Math.round(580 * window.uiScale))
                height: Math.round(236 * window.uiScale)
                color: theme.darkBackground
                border.width: 1
                border.color: theme.errorColor
                radius: theme.cornerRadius

                Text {
                    id: emptyTrashTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(18 * window.uiScale)
                    text: "Permanently empty Trash?"
                    color: theme.errorColor
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                }

                Text {
                    anchors.left: emptyTrashTitle.left
                    anchors.right: emptyTrashTitle.right
                    anchors.top: emptyTrashTitle.bottom
                    anchors.topMargin: Math.round(8 * window.uiScale)
                    text: "This permanently removes every item in Trash and cannot be undone. Type EMPTY to continue."
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    anchors.left: emptyTrashTitle.left
                    anchors.right: emptyTrashTitle.right
                    anchors.top: emptyTrashTitle.bottom
                    anchors.topMargin: Math.round(58 * window.uiScale)
                    height: Math.round(38 * window.uiScale)
                    color: window.alpha(theme.foreground, theme.hoverFillAlpha)
                    border.width: 1
                    border.color: emptyTrashInput.text.trim().toUpperCase() === "EMPTY"
                                  ? theme.errorColor
                                  : window.alpha(theme.foreground, theme.hoverBorderAlpha)
                    radius: theme.cornerRadius

                    TextInput {
                        id: emptyTrashInput
                        anchors.fill: parent
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        color: theme.foreground
                        selectionColor: theme.errorColor
                        selectedTextColor: theme.darkBackground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true

                        Keys.onEscapePressed: function(event) {
                            root.leaveEmptyTrashPrompt()
                            event.accepted = true
                        }
                        Keys.onReturnPressed: function(event) {
                            root.confirmEmptyTrash()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: function(event) {
                            root.confirmEmptyTrash()
                            event.accepted = true
                        }
                    }
                }

                Row {
                    anchors.left: emptyTrashTitle.left
                    anchors.right: emptyTrashTitle.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Math.round(18 * window.uiScale)
                    height: Math.round(36 * window.uiScale)
                    spacing: Math.round(8 * window.uiScale)

                    Rectangle {
                        width: (parent.width - parent.spacing) / 2
                        height: parent.height
                        color: emptyTrashInput.text.trim().toUpperCase() === "EMPTY"
                               ? window.alpha(theme.errorColor, 0.16)
                               : window.alpha(theme.foreground, 0.035)
                        border.width: 1
                        border.color: emptyTrashInput.text.trim().toUpperCase() === "EMPTY"
                                      ? theme.errorColor
                                      : window.alpha(theme.foreground, 0.1)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "Enter  EMPTY TRASH"
                            color: emptyTrashInput.text.trim().toUpperCase() === "EMPTY"
                                   ? theme.errorColor : theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: emptyTrashInput.text.trim().toUpperCase() === "EMPTY"
                            hoverEnabled: enabled
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.confirmEmptyTrash()
                        }
                    }

                    Rectangle {
                        width: (parent.width - parent.spacing) / 2
                        height: parent.height
                        color: emptyTrashCancel.containsMouse
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : window.alpha(theme.foreground, 0.045)
                        border.width: 1
                        border.color: window.alpha(theme.foreground,
                                                   emptyTrashCancel.containsMouse
                                                   ? theme.hoverBorderAlpha : 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "Esc  CANCEL"
                            color: theme.foreground
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            font.bold: true
                        }

                        MouseArea {
                            id: emptyTrashCancel
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.leaveEmptyTrashPrompt()
                        }
                    }
                }
            }
        }

        Rectangle {
            id: previewOverlay
            anchors.fill: parent
            visible: previewModel.active
            z: 145
            color: window.alpha(theme.background, 0.84)

            MouseArea {
                anchors.fill: parent
                onClicked: previewModel.close()
            }

            Rectangle {
                id: previewCard
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(70 * window.uiScale),
                                Math.round(920 * window.uiScale))
                height: Math.min(parent.height - Math.round(70 * window.uiScale),
                                 Math.round(610 * window.uiScale))
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                MouseArea { anchors.fill: parent }

                Text {
                    id: previewTitle
                    anchors.left: parent.left
                    anchors.right: previewClose.left
                    anchors.top: parent.top
                    height: Math.round(40 * window.uiScale)
                    anchors.leftMargin: Math.round(14 * window.uiScale)
                    anchors.rightMargin: Math.round(10 * window.uiScale)
                    text: previewModel.name
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }

                Text {
                    id: previewClose
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: Math.round(76 * window.uiScale)
                    height: previewTitle.height
                    anchors.rightMargin: Math.round(8 * window.uiScale)
                    text: "Space  ×"
                    color: previewClosePointer.containsMouse ? theme.foreground : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    MouseArea {
                        id: previewClosePointer
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: previewModel.close()
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewTitle.bottom
                    height: 1
                    color: window.alpha(theme.foreground, 0.14)
                }

                Image {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewTitle.bottom
                    anchors.bottom: previewPath.top
                    anchors.margins: Math.round(16 * window.uiScale)
                    visible: (previewModel.kind === "image" || previewModel.kind === "pdf")
                             && previewModel.imageSource.length > 0
                    source: previewModel.imageSource
                    sourceSize: Qt.size(width, height)
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Controls.ScrollView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewTitle.bottom
                    anchors.bottom: previewPath.top
                    anchors.margins: Math.round(16 * window.uiScale)
                    visible: previewModel.kind === "text" && !previewModel.loading
                    clip: true

                    Text {
                        width: parent.width
                        text: previewModel.text
                        color: theme.foreground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, Math.round(420 * window.uiScale))
                    spacing: Math.round(12 * window.uiScale)
                    visible: previewModel.loading || previewModel.kind === "unsupported"

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.round(64 * window.uiScale)
                        height: width
                        source: previewModel.iconSource
                        sourceSize: Qt.size(width, height)
                    }
                    Text {
                        width: parent.width
                        text: previewModel.loading ? "Loading preview…"
                                                   : "No quick preview for this file"
                        color: previewModel.loading ? theme.accent : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Text {
                    id: previewPath
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Math.round(35 * window.uiScale)
                    anchors.leftMargin: Math.round(14 * window.uiScale)
                    anchors.rightMargin: Math.round(14 * window.uiScale)
                    text: previewModel.path
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
            }
        }

        Rectangle {
            id: propertiesOverlay
            anchors.fill: parent
            visible: propertiesModel.active
            focus: visible
            z: 147
            color: window.alpha(theme.background, 0.86)

            MouseArea {
                anchors.fill: parent
                onClicked: root.closeProperties()
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(70 * window.uiScale),
                                Math.round(690 * window.uiScale))
                height: Math.min(parent.height - Math.round(70 * window.uiScale),
                                 Math.round(590 * window.uiScale))
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                MouseArea { anchors.fill: parent }

                Text {
                    id: propertiesTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.round(48 * window.uiScale)
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    text: propertiesModel.loading ? "PROPERTIES"
                                                  : "PROPERTIES  ·  " + propertiesModel.name
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }

                Controls.ScrollView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: propertiesTitle.bottom
                    anchors.bottom: propertiesFooter.top
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    clip: true

                    Column {
                        width: parent.width
                        spacing: 1

                        Repeater {
                            model: [
                                {label: "NAME", value: propertiesModel.name},
                                {label: "PATH", value: propertiesModel.path},
                                {label: "TYPE", value: propertiesModel.type},
                                {label: "MIME", value: propertiesModel.mimeType},
                                {label: "SIZE", value: propertiesModel.directory
                                                       && propertiesModel.recursiveSize.length > 0
                                                       ? propertiesModel.recursiveSize
                                                       : propertiesModel.size},
                                {label: "MODIFIED", value: propertiesModel.modified},
                                {label: "CREATED", value: propertiesModel.created},
                                {label: "ACCESSED", value: propertiesModel.accessed},
                                {label: "OWNER", value: propertiesModel.owner},
                                {label: "GROUP", value: propertiesModel.group},
                                {label: "PERMISSIONS", value: propertiesModel.permissions},
                                {label: "SYMLINK TARGET", value: propertiesModel.symlinkTarget},
                                {label: "FILESYSTEM FREE", value: propertiesModel.filesystemFree}
                            ]

                            Rectangle {
                                required property int index
                                required property var modelData
                                width: parent.width
                                height: Math.round(34 * window.uiScale)
                                color: index % 2 === 0
                                       ? window.alpha(theme.foreground, 0.025) : "transparent"

                                Text {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.round(145 * window.uiScale)
                                    anchors.leftMargin: Math.round(8 * window.uiScale)
                                    text: parent.modelData.label
                                    color: theme.muted
                                    font.family: theme.fontFamily
                                    font.pixelSize: Math.max(8, theme.fontSize - 2)
                                }
                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: Math.round(158 * window.uiScale)
                                    anchors.rightMargin: Math.round(8 * window.uiScale)
                                    text: parent.modelData.value || "—"
                                    color: theme.foreground
                                    font.family: theme.fontFamily
                                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: propertiesFooter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: propertiesModel.permissionsEditable
                            ? Math.round(79 * window.uiScale)
                            : Math.round(46 * window.uiScale)
                    color: "transparent"

                    Row {
                        id: permissionBits
                        visible: propertiesModel.permissionsEditable
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: Math.round(18 * window.uiScale)
                        anchors.rightMargin: Math.round(18 * window.uiScale)
                        height: Math.round(31 * window.uiScale)
                        spacing: Math.round(4 * window.uiScale)

                        Repeater {
                            model: ["r", "w", "x", "r", "w", "x", "r", "w", "x"]

                            Rectangle {
                                id: permissionBit
                                required property int index
                                required property string modelData
                                readonly property bool enabledBit:
                                    (propertiesModel.permissionMode & (1 << (8 - index))) !== 0
                                width: (permissionBits.width - permissionBits.spacing * 8) / 9
                                height: permissionBits.height
                                color: enabledBit ? window.alpha(theme.accent, 0.16)
                                                  : window.alpha(theme.foreground, 0.035)
                                border.width: 1
                                border.color: enabledBit ? theme.accent
                                                         : window.alpha(theme.foreground, 0.14)
                                radius: theme.cornerRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: (permissionBit.index + 1) + " " + permissionBit.modelData
                                    color: permissionBit.enabledBit ? theme.accent : theme.muted
                                    font.family: theme.fontFamily
                                    font.pixelSize: Math.max(8, theme.fontSize - 2)
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: propertiesModel.togglePermissionBit(permissionBit.index)
                                }
                            }
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: Math.round(42 * window.uiScale)
                        text: propertiesModel.directory
                              ? (propertiesModel.sizing
                                 ? "c cancel folder sizing  ·  Esc close"
                                 : "s calculate folder size  ·  Esc close")
                              : "Esc close"
                        color: propertiesModel.sizing ? theme.accent : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 2)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: propertiesModel.loading
                    text: "Reading properties…"
                    color: theme.accent
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                }
            }
        }

        Rectangle {
            id: openWithOverlay
            anchors.fill: parent
            visible: openWithModel.active
            z: 148
            color: window.alpha(theme.background, 0.86)

            MouseArea {
                anchors.fill: parent
                onClicked: openWithModel.close()
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(70 * window.uiScale),
                                Math.round(560 * window.uiScale))
                height: Math.min(parent.height - Math.round(90 * window.uiScale),
                                 Math.round(470 * window.uiScale))
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                MouseArea { anchors.fill: parent }

                Text {
                    id: openWithTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.round(48 * window.uiScale)
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    text: "OPEN  " + openWithModel.path.substring(
                              openWithModel.path.lastIndexOf("/") + 1) + "  WITH"
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }

                ListView {
                    id: openWithApplications
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: openWithTitle.bottom
                    anchors.bottom: openWithFooter.top
                    anchors.margins: Math.round(10 * window.uiScale)
                    model: openWithModel
                    currentIndex: -1
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        id: applicationRow
                        required property int index
                        required property string applicationName
                        required property string applicationDesktopId
                        required property bool applicationIsDefault
                        width: openWithApplications.width
                        height: Math.round(38 * window.uiScale)
                        color: ListView.isCurrentItem
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : (applicationPointer.containsMouse
                                  ? window.alpha(theme.foreground, 0.035) : "transparent")
                        border.width: ListView.isCurrentItem ? 1 : 0
                        border.color: window.alpha(theme.foreground, theme.hoverBorderAlpha)
                        radius: theme.cornerRadius

                        Text {
                            anchors.left: parent.left
                            anchors.right: defaultApplicationLabel.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Math.round(10 * window.uiScale)
                            anchors.rightMargin: Math.round(8 * window.uiScale)
                            text: applicationRow.applicationName
                            color: ListView.isCurrentItem ? theme.foreground : theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: theme.fontSize
                            elide: Text.ElideRight
                        }
                        Text {
                            id: defaultApplicationLabel
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: Math.round(10 * window.uiScale)
                            text: applicationRow.applicationIsDefault ? "DEFAULT" : ""
                            color: theme.accent
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(8, theme.fontSize - 2)
                            font.bold: true
                        }
                        MouseArea {
                            id: applicationPointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: openWithApplications.currentIndex = applicationRow.index
                            onDoubleClicked: {
                                openWithApplications.currentIndex = applicationRow.index
                                root.activateOpenWith()
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: openWithApplications
                    visible: openWithModel.count === 0
                    text: openWithModel.loading ? "Finding compatible applications…"
                                                : openWithModel.errorMessage
                    color: openWithModel.errorMessage.length > 0 ? theme.errorColor : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                }

                Rectangle {
                    id: openWithFooter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Math.round(42 * window.uiScale)
                    color: "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "j/k choose  ·  Enter open  ·  d make default  ·  Esc close"
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 2)
                    }
                }
            }
        }

        Rectangle {
            id: templateOverlay
            anchors.fill: parent
            visible: templateModel.active
            z: 149
            color: window.alpha(theme.background, 0.88)

            MouseArea { anchors.fill: parent }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(70 * window.uiScale),
                                Math.round(620 * window.uiScale))
                height: Math.min(parent.height - Math.round(90 * window.uiScale),
                                 Math.round(520 * window.uiScale))
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                Text {
                    id: templateTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.round(48 * window.uiScale)
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    text: "NEW FROM TEMPLATE"
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }

                ListView {
                    id: templateList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: templateTitle.bottom
                    anchors.bottom: templateNameLabel.top
                    anchors.leftMargin: Math.round(12 * window.uiScale)
                    anchors.rightMargin: Math.round(12 * window.uiScale)
                    anchors.bottomMargin: Math.round(12 * window.uiScale)
                    model: templateModel
                    currentIndex: -1
                    clip: true
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property int index
                        required property string templateName
                        required property string templatePath
                        required property string templateRelativePath
                        width: templateList.width
                        height: Math.round(42 * window.uiScale)
                        color: ListView.isCurrentItem
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : (templatePointer.containsMouse
                                  ? window.alpha(theme.foreground, 0.035) : "transparent")
                        border.width: ListView.isCurrentItem ? 1 : 0
                        border.color: window.alpha(theme.foreground, theme.hoverBorderAlpha)
                        radius: theme.cornerRadius

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: Math.round(10 * window.uiScale)
                            anchors.rightMargin: Math.round(10 * window.uiScale)
                            anchors.topMargin: Math.round(5 * window.uiScale)
                            text: parent.templateName
                            color: ListView.isCurrentItem ? theme.foreground : theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: theme.fontSize
                            elide: Text.ElideRight
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Math.round(10 * window.uiScale)
                            anchors.rightMargin: Math.round(10 * window.uiScale)
                            anchors.bottomMargin: Math.round(4 * window.uiScale)
                            text: parent.templateRelativePath
                            color: window.alpha(theme.muted, 0.72)
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(8, theme.fontSize - 2)
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            id: templatePointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                templateList.currentIndex = parent.index
                                templateNameInput.text = templateModel.suggestedNameAt(parent.index)
                            }
                            onDoubleClicked: {
                                templateList.currentIndex = parent.index
                                templateNameInput.text = templateModel.suggestedNameAt(parent.index)
                                templateNameInput.forceActiveFocus()
                                templateNameInput.selectAll()
                            }
                        }
                    }
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                }

                Text {
                    anchors.centerIn: templateList
                    visible: templateModel.count === 0
                    text: templateModel.loading ? "Reading Templates…"
                                                : templateModel.errorMessage
                    color: templateModel.errorMessage.length > 0 ? theme.errorColor : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                }

                Text {
                    id: templateNameLabel
                    anchors.left: parent.left
                    anchors.bottom: templateNameField.top
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.bottomMargin: Math.round(4 * window.uiScale)
                    text: "NEW FILE NAME"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                }

                Rectangle {
                    id: templateNameField
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: templateFooter.top
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    anchors.bottomMargin: Math.round(8 * window.uiScale)
                    height: Math.round(34 * window.uiScale)
                    color: window.alpha(theme.foreground, 0.045)
                    border.width: templateNameInput.activeFocus ? 1 : 0
                    border.color: theme.accent
                    radius: theme.cornerRadius

                    TextInput {
                        id: templateNameInput
                        anchors.fill: parent
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        anchors.rightMargin: Math.round(9 * window.uiScale)
                        enabled: !templateModel.copying && templateList.currentIndex >= 0
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.darkBackground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        Keys.onEscapePressed: function(event) {
                            templateModel.close()
                            event.accepted = true
                        }
                        Keys.onReturnPressed: function(event) {
                            root.createSelectedTemplate()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: function(event) {
                            root.createSelectedTemplate()
                            event.accepted = true
                        }
                    }
                }

                Text {
                    id: templateFooter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Math.round(42 * window.uiScale)
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    text: templateModel.copying ? "CREATING…"
                          : "j/k choose  ·  Enter name  ·  Enter create  ·  Esc close"
                    color: templateModel.copying ? theme.accent : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            id: bulkRenameOverlay
            anchors.fill: parent
            visible: bulkRenameModel.active
            z: 149
            color: window.alpha(theme.background, 0.88)

            MouseArea { anchors.fill: parent }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(60 * window.uiScale),
                                Math.round(760 * window.uiScale))
                height: Math.min(parent.height - Math.round(70 * window.uiScale),
                                 Math.round(620 * window.uiScale))
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                Text {
                    id: bulkTitle
                    anchors.left: parent.left
                    anchors.right: bulkModeButton.left
                    anchors.top: parent.top
                    height: Math.round(48 * window.uiScale)
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    text: "BULK RENAME  ·  " + bulkRenameModel.count + " ITEMS"
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }

                Rectangle {
                    id: bulkModeButton
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: Math.round(12 * window.uiScale)
                    anchors.topMargin: Math.round(10 * window.uiScale)
                    width: Math.round(196 * window.uiScale)
                    height: Math.round(30 * window.uiScale)
                    color: bulkModePointer.containsMouse
                           ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                           : window.alpha(theme.foreground, 0.05)
                    border.width: 1
                    border.color: bulkRenameModel.numbering ? theme.accent
                                                            : window.alpha(theme.foreground, 0.18)
                    radius: theme.cornerRadius

                    Text {
                        anchors.centerIn: parent
                        text: "Alt-n  " + (bulkRenameModel.numbering ? "NUMBERED" : "FIND / REPLACE")
                        color: bulkRenameModel.numbering ? theme.accent : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 2)
                        font.bold: true
                    }
                    MouseArea {
                        id: bulkModePointer
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bulkRenameModel.numbering = !bulkRenameModel.numbering
                    }
                }

                Text {
                    id: bulkFindLabel
                    anchors.left: parent.left
                    anchors.top: bulkTitle.bottom
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    text: "FIND"
                    color: bulkRenameModel.numbering ? window.alpha(theme.muted, 0.45) : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                }

                Rectangle {
                    id: bulkFindField
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: bulkFindLabel.bottom
                    anchors.leftMargin: Math.round(16 * window.uiScale)
                    anchors.rightMargin: Math.round(16 * window.uiScale)
                    anchors.topMargin: Math.round(4 * window.uiScale)
                    height: Math.round(34 * window.uiScale)
                    color: window.alpha(theme.foreground, 0.045)
                    border.width: bulkFindInput.activeFocus ? 1 : 0
                    border.color: theme.accent
                    radius: theme.cornerRadius
                    opacity: bulkRenameModel.numbering ? 0.5 : 1

                    TextInput {
                        id: bulkFindInput
                        anchors.fill: parent
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        anchors.rightMargin: Math.round(9 * window.uiScale)
                        enabled: !bulkRenameModel.numbering && !bulkRenameModel.applying
                        text: bulkRenameModel.findText
                        onTextEdited: bulkRenameModel.findText = text
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.darkBackground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        Keys.onPressed: function(event) {
                            var control = (event.modifiers & Qt.ControlModifier) !== 0
                            var alt = (event.modifiers & Qt.AltModifier) !== 0
                            if (event.key === Qt.Key_Escape) {
                                bulkRenameModel.close()
                                event.accepted = true
                            } else if (event.key === Qt.Key_Tab) {
                                bulkReplacementInput.forceActiveFocus()
                                event.accepted = true
                            } else if (alt && event.key === Qt.Key_N) {
                                bulkRenameModel.numbering = !bulkRenameModel.numbering
                                bulkReplacementInput.forceActiveFocus()
                                event.accepted = true
                            } else if (control && (event.key === Qt.Key_Return
                                                   || event.key === Qt.Key_Enter)) {
                                root.applyBulkRename()
                                event.accepted = true
                            }
                        }
                    }
                }

                Text {
                    id: bulkReplacementLabel
                    anchors.left: bulkFindLabel.left
                    anchors.top: bulkFindField.bottom
                    anchors.topMargin: Math.round(10 * window.uiScale)
                    text: bulkRenameModel.numbering ? "BASE NAME" : "REPLACE WITH"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                }

                Rectangle {
                    id: bulkReplacementField
                    anchors.left: bulkFindField.left
                    anchors.right: bulkFindField.right
                    anchors.top: bulkReplacementLabel.bottom
                    anchors.topMargin: Math.round(4 * window.uiScale)
                    height: bulkFindField.height
                    color: window.alpha(theme.foreground, 0.045)
                    border.width: bulkReplacementInput.activeFocus ? 1 : 0
                    border.color: theme.accent
                    radius: theme.cornerRadius

                    TextInput {
                        id: bulkReplacementInput
                        anchors.fill: parent
                        anchors.leftMargin: Math.round(9 * window.uiScale)
                        anchors.rightMargin: Math.round(9 * window.uiScale)
                        enabled: !bulkRenameModel.applying
                        text: bulkRenameModel.replacementText
                        onTextEdited: bulkRenameModel.replacementText = text
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.darkBackground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        Keys.onPressed: function(event) {
                            var control = (event.modifiers & Qt.ControlModifier) !== 0
                            var alt = (event.modifiers & Qt.AltModifier) !== 0
                            if (event.key === Qt.Key_Escape) {
                                bulkRenameModel.close()
                                event.accepted = true
                            } else if (event.key === Qt.Key_Tab) {
                                if (bulkRenameModel.numbering)
                                    bulkReplacementInput.selectAll()
                                else
                                    bulkFindInput.forceActiveFocus()
                                event.accepted = true
                            } else if (alt && event.key === Qt.Key_N) {
                                bulkRenameModel.numbering = !bulkRenameModel.numbering
                                event.accepted = true
                            } else if (control && (event.key === Qt.Key_Return
                                                   || event.key === Qt.Key_Enter)) {
                                root.applyBulkRename()
                                event.accepted = true
                            }
                        }
                    }
                }

                ListView {
                    id: bulkPreview
                    anchors.left: bulkFindField.left
                    anchors.right: bulkFindField.right
                    anchors.top: bulkReplacementField.bottom
                    anchors.bottom: bulkFooter.top
                    anchors.topMargin: Math.round(14 * window.uiScale)
                    model: bulkRenameModel
                    clip: true
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property int index
                        required property string originalName
                        required property string proposedName
                        required property bool renameValid
                        required property string renameError
                        width: bulkPreview.width
                        height: Math.round(34 * window.uiScale)
                        color: index % 2 === 0 ? window.alpha(theme.foreground, 0.025)
                                              : "transparent"

                        Text {
                            anchors.left: parent.left
                            anchors.right: renameArrow.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Math.round(8 * window.uiScale)
                            anchors.rightMargin: Math.round(8 * window.uiScale)
                            text: parent.originalName
                            color: theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            elide: Text.ElideMiddle
                        }
                        Text {
                            id: renameArrow
                            anchors.centerIn: parent
                            text: "→"
                            color: theme.muted
                            font.family: theme.fontFamily
                        }
                        Text {
                            anchors.left: renameArrow.right
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Math.round(8 * window.uiScale)
                            anchors.rightMargin: Math.round(8 * window.uiScale)
                            text: parent.renameValid ? parent.proposedName
                                                     : parent.proposedName + "  ·  " + parent.renameError
                            color: parent.renameValid ? theme.foreground : theme.errorColor
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            elide: Text.ElideMiddle
                        }
                    }
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                }

                Rectangle {
                    id: bulkFooter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Math.round(50 * window.uiScale)
                    color: "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Math.round(16 * window.uiScale)
                        text: bulkRenameModel.applying ? "RENAMING…"
                              : (bulkRenameModel.errorMessage.length > 0
                                 ? bulkRenameModel.errorMessage
                                 : "Tab fields  ·  Alt-n mode  ·  Ctrl-Enter apply  ·  Esc close")
                        color: bulkRenameModel.errorMessage.length > 0
                               ? theme.errorColor : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 2)
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: Math.round(14 * window.uiScale)
                        width: Math.round(110 * window.uiScale)
                        height: Math.round(30 * window.uiScale)
                        color: bulkApplyPointer.containsMouse && bulkRenameModel.canApply
                               ? window.alpha(theme.accent, 0.24)
                               : window.alpha(theme.foreground, 0.05)
                        border.width: 1
                        border.color: bulkRenameModel.canApply ? theme.accent
                                                               : window.alpha(theme.foreground, 0.14)
                        radius: theme.cornerRadius

                        Text {
                            anchors.centerIn: parent
                            text: "APPLY"
                            color: bulkRenameModel.canApply ? theme.accent : theme.muted
                            opacity: bulkRenameModel.canApply ? 1 : 0.5
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 1)
                            font.bold: true
                        }
                        MouseArea {
                            id: bulkApplyPointer
                            anchors.fill: parent
                            enabled: bulkRenameModel.canApply
                            hoverEnabled: true
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.applyBulkRename()
                        }
                    }
                }
            }
        }

        Rectangle {
            id: conflictOverlay
            anchors.fill: parent
            visible: fileModel.transferConflictActive
            z: 150
            color: window.alpha(theme.background, 0.82)

            MouseArea {
                anchors.fill: parent
                onClicked: files.forceActiveFocus()
            }

            Rectangle {
                id: conflictCard
                anchors.centerIn: parent
                width: Math.min(parent.width - Math.round(40 * window.uiScale),
                                Math.round(660 * window.uiScale))
                height: Math.round((root.conflictRenameMode ? 250 : 270) * window.uiScale)
                color: theme.darkBackground
                border.width: 1
                border.color: window.alpha(theme.foreground, 0.24)
                radius: theme.cornerRadius

                Text {
                    id: conflictTitle
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: Math.round(18 * window.uiScale)
                    anchors.rightMargin: Math.round(18 * window.uiScale)
                    anchors.topMargin: Math.round(16 * window.uiScale)
                    text: (fileModel.transferConflictSourceIsDirectory
                           && fileModel.transferConflictTargetIsDirectory)
                          ? "A folder named “" + root.conflictItemName() + "” already exists"
                          : "An item named “" + root.conflictItemName() + "” already exists"
                    color: theme.foreground
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize
                    font.bold: true
                    elide: Text.ElideMiddle
                }

                Text {
                    id: conflictDescription
                    anchors.left: conflictTitle.left
                    anchors.right: conflictTitle.right
                    anchors.top: conflictTitle.bottom
                    anchors.topMargin: Math.round(7 * window.uiScale)
                    text: (fileModel.transferConflictSourceIsDirectory
                           && fileModel.transferConflictTargetIsDirectory)
                          ? "Replace removes the existing folder and its contents."
                          : "Choose what Shibui should do with the existing destination."
                    color: fileModel.transferConflictSourceIsDirectory
                           && fileModel.transferConflictTargetIsDirectory
                           ? theme.errorColor : theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                    elide: Text.ElideRight
                }

                Text {
                    id: conflictSource
                    anchors.left: conflictTitle.left
                    anchors.right: conflictTitle.right
                    anchors.top: conflictDescription.bottom
                    anchors.topMargin: Math.round(13 * window.uiScale)
                    text: "FROM  " + fileModel.transferConflictSource
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                    elide: Text.ElideMiddle
                }

                Text {
                    anchors.left: conflictTitle.left
                    anchors.right: conflictTitle.right
                    anchors.top: conflictSource.bottom
                    anchors.topMargin: Math.round(5 * window.uiScale)
                    text: "TO    " + fileModel.transferConflictTarget
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: Math.max(9, theme.fontSize - 2)
                    elide: Text.ElideMiddle
                }

                Column {
                    id: conflictChoices
                    visible: !root.conflictRenameMode
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(18 * window.uiScale)
                    anchors.rightMargin: Math.round(18 * window.uiScale)
                    anchors.bottomMargin: Math.round(16 * window.uiScale)
                    spacing: Math.round(12 * window.uiScale)

                    Row {
                        id: conflictActions
                        width: parent.width
                        height: Math.round(34 * window.uiScale)
                        spacing: Math.round(6 * window.uiScale)

                        Repeater {
                            model: [
                                { key: "r", label: "REPLACE", action: "replace" },
                                { key: "s", label: "SKIP", action: "skip" },
                                { key: "n", label: "RENAME", action: "rename" },
                                { key: "Esc", label: "CANCEL", action: "cancel" }
                            ]

                            Rectangle {
                                required property var modelData

                                width: (conflictActions.width - conflictActions.spacing * 3) / 4
                                height: conflictActions.height
                                color: conflictActionPointer.containsMouse
                                       ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                                       : window.alpha(theme.foreground, 0.045)
                                border.width: 1
                                border.color: window.alpha(theme.foreground,
                                                           conflictActionPointer.containsMouse
                                                           ? theme.hoverBorderAlpha : 0.14)
                                radius: theme.cornerRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData.key + "  " + parent.modelData.label
                                    color: parent.modelData.action === "replace"
                                           ? theme.errorColor : theme.foreground
                                    font.family: theme.fontFamily
                                    font.pixelSize: Math.max(9, theme.fontSize - 1)
                                    font.bold: true
                                }

                                MouseArea {
                                    id: conflictActionPointer
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (parent.modelData.action === "rename")
                                            root.enterConflictRename()
                                        else
                                            root.resolveConflict(parent.modelData.action)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: Math.round(30 * window.uiScale)
                        color: applyPointer.containsMouse
                               ? window.alpha(theme.foreground, theme.hoverFillAlpha)
                               : "transparent"
                        radius: theme.cornerRadius

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Math.round(8 * window.uiScale)
                            text: "a  APPLY REPLACE OR SKIP TO REMAINING"
                            color: theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 2)
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: Math.round(8 * window.uiScale)
                            text: root.conflictApplyRemaining ? "ON" : "OFF"
                            color: root.conflictApplyRemaining ? theme.accent : theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: Math.max(9, theme.fontSize - 2)
                            font.bold: true
                        }

                        MouseArea {
                            id: applyPointer
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.conflictApplyRemaining = !root.conflictApplyRemaining
                        }
                    }
                }

                Rectangle {
                    id: conflictRenameSurface
                    visible: root.conflictRenameMode
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Math.round(18 * window.uiScale)
                    anchors.rightMargin: Math.round(18 * window.uiScale)
                    anchors.bottomMargin: Math.round(18 * window.uiScale)
                    height: Math.round(68 * window.uiScale)
                    color: window.alpha(theme.foreground, theme.hoverFillAlpha)
                    border.width: 1
                    border.color: fileModel.transferConflictError.length > 0
                                  ? theme.errorColor
                                  : window.alpha(theme.foreground, theme.hoverBorderAlpha)
                    radius: theme.cornerRadius

                    Text {
                        id: conflictRenamePrefix
                        anchors.left: parent.left
                        anchors.verticalCenter: conflictRenameInput.verticalCenter
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        text: "rename"
                        color: theme.accent
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        font.bold: true
                    }

                    TextInput {
                        id: conflictRenameInput
                        anchors.left: conflictRenamePrefix.right
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: Math.round(38 * window.uiScale)
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        color: theme.foreground
                        selectionColor: theme.accent
                        selectedTextColor: theme.darkBackground
                        font.family: theme.fontFamily
                        font.pixelSize: theme.fontSize
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true

                        Keys.onEscapePressed: function(event) {
                            root.leaveConflictRename()
                            event.accepted = true
                        }
                        Keys.onReturnPressed: function(event) {
                            root.commitConflictRename()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: function(event) {
                            root.commitConflictRename()
                            event.accepted = true
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Math.round(10 * window.uiScale)
                        anchors.rightMargin: Math.round(10 * window.uiScale)
                        anchors.bottomMargin: Math.round(7 * window.uiScale)
                        text: fileModel.transferConflictError.length > 0
                              ? fileModel.transferConflictError
                              : "Enter to use this name  ·  Esc to go back"
                        color: fileModel.transferConflictError.length > 0
                               ? theme.errorColor : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: Math.max(9, theme.fontSize - 2)
                        elide: Text.ElideRight
                    }
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: root.helpVisible
            z: 100
            themeObject: theme
            modeName: root.modeName
            trashView: fileModel.trashView
            onCloseRequested: {
                root.helpVisible = false
                files.forceActiveFocus()
            }
        }

        Connections {
            target: fileModel

            function onTransferChanged() {
                if (fileModel.transferConflictActive && !root.conflictWasActive) {
                    root.conflictWasActive = true
                    root.conflictRenameMode = false
                    root.conflictApplyRemaining = false
                    files.forceActiveFocus()
                } else if (!fileModel.transferConflictActive && root.conflictWasActive) {
                    root.conflictWasActive = false
                    root.conflictRenameMode = false
                }
            }

            function onCurrentPathChanged() {
                if (root.historyNavigation) {
                    if (root.pendingHistoryEntry) {
                        root.pendingCursorPath = root.pendingHistoryEntry.cursor || ""
                        root.cursorPath = root.pendingHistoryEntry.cursor || ""
                        root.pendingScrollY = root.pendingHistoryEntry.scrollY
                    }
                    root.historyNavigation = false
                    root.pendingHistoryEntry = null
                } else if (root.historyCurrentPath.length > 0
                           && root.historyCurrentPath !== fileModel.currentPath) {
                    var history = root.backHistory.slice()
                    history.push({path: root.historyCurrentPath,
                                  cursor: root.cursorPath,
                                  scrollY: root.currentViewContentY()})
                    root.backHistory = history
                    root.forwardHistory = []
                }
                root.historyCurrentPath = fileModel.currentPath
                root.visualMode = false
                root.pointerSelectionAnchorIndex = -1
                root.clearSelection()
                if (!root.pendingCursorPath)
                    root.pendingCursorPath = ""
                root.cursorRestoreAttempts = 0
                cursorRestoreTimer.stop()
                if (root.pendingCursorPath.length === 0)
                    root.cursorPath = root.pendingSelectionPath
                root.pendingSelectionPath = ""
                files.currentIndex = -1
                files.contentY = 0
                grid.currentIndex = -1
                grid.contentY = 0
                root.updateCurrentTabLocation()
            }

            function onContentsChanged() {
                root.pruneSelection()
                Qt.callLater(root.restoreCursor)
            }

            function onTransferFinished(success, cancelled, message, completedSources,
                                        destinationPaths) {
                var wasRestore = root.restoreInProgress
                var gatheredFolder = root.folderGatherTargetPath
                root.removeCompletedCutPaths(completedSources)
                if (success) {
                    root.showNotice(message, false)
                    if (!wasRestore
                        && fileModel.currentPath === root.activeTransferDestination
                        && destinationPaths.length > 0) {
                        root.pendingCursorPath = destinationPaths[0]
                        root.cursorPath = destinationPaths[0]
                        Qt.callLater(root.restoreCursor)
                    }
                } else if (cancelled) {
                    root.showNotice(message, false)
                }
                if (wasRestore) {
                    root.restoreInProgress = false
                    root.clearSelection()
                    Qt.callLater(root.restoreCursor)
                }
                if (gatheredFolder.length > 0 && completedSources.length > 0) {
                    root.pendingCursorPath = gatheredFolder
                    root.cursorPath = gatheredFolder
                    Qt.callLater(root.restoreCursor)
                }
                root.folderGatherTargetPath = ""
                root.activeTransferDestination = ""
            }

            function onTrashFinished(success, cancelled, message, trashedSources,
                                     trashPaths, failedPaths) {
                root.emptyTrashInProgress = false
                root.removeClipboardPaths(trashedSources)
                if (failedPaths.length > 0)
                    root.pendingCursorPath = failedPaths[0]
                if (success || cancelled)
                    root.showNotice(message, failedPaths.length > 0)
                root.clearSelection()
                Qt.callLater(root.restoreCursor)
            }

            function onUndoFinished(success, message, paths) {
                root.showNotice(message, !success)
                if (success && paths.length > 0) {
                    var slash = paths[0].lastIndexOf("/")
                    var parentPath = slash > 0 ? paths[0].substring(0, slash) : "/"
                    if (fileModel.currentPath === parentPath) {
                        root.pendingCursorPath = paths[0]
                        root.cursorPath = paths[0]
                    }
                }
                root.clearSelection()
                Qt.callLater(root.restoreCursor)
            }

            function onFileClipboardChanged() {
                root.syncClipboardFromDesktop()
            }
        }

        Connections {
            target: searchModel

            function onCountChanged() {
                if (!root.finderMode)
                    return
                if (searchModel.count === 0)
                    finderResults.currentIndex = -1
                else if (finderResults.currentIndex < 0
                         || finderResults.currentIndex >= searchModel.count)
                    finderResults.currentIndex = 0
            }
        }

        Connections {
            target: recentModel

            function onCountChanged() {
                if (!root.recentMode)
                    return
                finderResults.currentIndex = recentModel.count > 0 ? 0 : -1
            }
        }

        Connections {
            target: openWithModel

            function onCountChanged() {
                if (openWithModel.active && openWithModel.count > 0
                    && openWithApplications.currentIndex < 0)
                    openWithApplications.currentIndex = 0
            }

            function onDefaultChanged(success, message) {
                root.showNotice(message, !success)
            }
        }

        Connections {
            target: placesModel

            function onDeviceActionFinished(success, message, path) {
                root.showNotice(message, !success)
                if (success && path.length > 0) {
                    root.placesMode = false
                    fileModel.navigateTo(path)
                    files.forceActiveFocus()
                }
            }
        }

        Connections {
            target: networkModel

            function onChanged() {
                if (root.networkMode && networkModel.promptActive) {
                    networkInput.text = ""
                    networkInput.forceActiveFocus()
                }
            }

            function onConnected(path, uri, message) {
                root.networkMode = false
                networkInput.text = ""
                root.showNotice(message, false)
                placesModel.refresh()
                fileModel.navigateTo(path)
                files.forceActiveFocus()
            }

            function onFinished(success, message) {
                if (!success) {
                    root.showNotice(message, true)
                    if (root.networkMode) {
                        networkInput.text = networkModel.uri
                        networkInput.forceActiveFocus()
                        networkInput.selectAll()
                    }
                } else {
                    placesModel.refresh()
                }
            }
        }

        Connections {
            target: archiveModel

            function onFinished(success, message, outputPath) {
                root.showNotice(message, !success)
                fileModel.refresh()
                if (success) {
                    root.pendingCursorPath = outputPath
                    Qt.callLater(root.restoreCursor)
                }
                files.forceActiveFocus()
            }
        }

        Connections {
            target: bulkRenameModel

            function onChanged() {
                if (!bulkRenameModel.active)
                    files.forceActiveFocus()
            }

            function onFinished(success, message, targetPaths) {
                root.showNotice(message, !success)
                fileModel.refresh()
                if (success) {
                    var selection = {}
                    for (var index = 0; index < targetPaths.length; ++index)
                        selection[targetPaths[index]] = true
                    root.applySelection(selection)
                    root.pendingCursorPath = targetPaths.length > 0 ? targetPaths[0] : ""
                    Qt.callLater(root.restoreCursor)
                    files.forceActiveFocus()
                }
            }
        }

        Connections {
            target: templateModel

            function onChanged() {
                if (!templateModel.active)
                    files.forceActiveFocus()
            }

            function onCountChanged() {
                if (!templateModel.active || templateModel.count === 0)
                    return
                templateList.currentIndex = 0
                templateNameInput.text = templateModel.suggestedNameAt(0)
            }

            function onFinished(success, message, outputPath) {
                root.showNotice(message, !success)
                fileModel.refresh()
                if (success) {
                    root.clearSelection()
                    root.pendingCursorPath = outputPath
                    root.cursorPath = outputPath
                    Qt.callLater(root.restoreCursor)
                    files.forceActiveFocus()
                }
            }
        }

        Component.onCompleted: {
            tabs = [makeTab(fileModel.currentPath, fileModel.trashView,
                            initialSelectionPath, 0, [], [])]
            currentTabIndex = 0
            syncClipboardFromDesktop()
            files.forceActiveFocus()
            Qt.callLater(restoreCursor)
        }
    }
}
