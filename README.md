# Shibui

Shibui is a quiet, keyboard-first graphical file manager designed to feel
native to Omarchy and Hyprland.

> **Status:** Shibui is a working replacement candidate. Automated local,
> interaction, archive, network-failure, and large-directory checks pass. The
> remaining release gate is hands-on verification with physical removable
> media and a latency-injected network share.

The current build covers ordinary local, removable-device, and GVfs network
work: keyboard-driven navigation and selection, safe file operations and undo,
Trash, tabs, fuzzy finding, previews, properties, archives, Recent, templates,
and bulk rename. It follows the active Omarchy theme and icon set, accepts local
paths and supported network URIs, and reacts to external filesystem changes
without a restart.

## Requirements

Shibui targets Linux, Omarchy, and Hyprland. Building requires a C++17 compiler,
`make`, `qmake6`, and the Qt 6 Core, Concurrent, GUI, QML, Quick, and Widgets
modules. Omarchy normally provides these already.

Runtime integrations use existing desktop tools instead of private services:

- `fd` powers recursive filename search.
- GIO/GVfs powers network locations and application associations.
- `udisksctl` and `lsblk` power removable-device actions.
- `bsdtar` enables archive creation and extraction.
- `pdftoppm` enables PDF previews.
- `xdg-terminal-exec` opens the configured terminal.

Missing optional tools disable only their corresponding feature and produce a
clear error in the interface.

## Build and run

```sh
mkdir -p build
cd build
qmake6 ../shibui.pro
make -j2
./shibui [path]
```

The optional location may be a directory, local file, `file://` URI, or a
supported network URI. Supplying a file opens its parent and selects it. A
second invocation reuses the running window and reveals the requested location.

To install the binary, launcher, and scalable icon without changing your
current default file manager:

```sh
sudo make install
```

The desktop entry advertises directory and SMB/SFTP/WebDAV/NFS handling, but
installation deliberately does not change any MIME defaults.

To measure when the UI and initial directory are ready for input on the current
machine:

```sh
./shibui --profile-startup [path]
```

On the reference Omarchy machine, three native `/tmp` runs on 2026-08-26
reported 254 ms cold, then 134 ms and 134 ms warm; the warm-launch target is
under 200 ms.

## Keyboard

| Key | Action |
| --- | --- |
| `j` / `k`, arrows | Select next / previous item |
| `h` / Left / Backspace | Go to the parent directory |
| `l` / Right / Enter | Open the selected item |
| `[` / `]`, `Alt-Left` / `Alt-Right` | Go back / forward in location history |
| `t` / `Ctrl-t` | Open the selected folder / current location in a new tab |
| `gt` / `gT`, `Ctrl-Tab` | Move to the next / previous tab |
| `Ctrl-w` | Close the current tab |
| `b` / `B` | Enter Places mode / hide or show the Places sidebar |
| `Enter` on Recent | Browse the desktop's existing recent-file list |
| `m` | Bookmark the current folder |
| `a` / `r` / `d` in Places | Add, rename, or remove a bookmark |
| `J` / `K` in Places | Move a bookmark down or up |
| `Enter` / `u` / `e` in Places | Mount, unmount, or eject a device |
| `Ctrl-k` / `c` in Places | Connect to SMB, SFTP, WebDAV, or NFS |
| `x` in Places | Disconnect the selected active network share |
| `gg` / `G` | Select first / last item |
| `Ctrl-d` / `Ctrl-u` | Move half a page down / up |
| `Space` | Preview the item under the cursor; press again or `Esc` to close |
| `o` | Choose another compatible application; `d` makes it the default |
| `z` / `Alt-Enter` | Show file properties; `s` calculates folder size |
| `x` / `Ctrl-Space` | Toggle the item under the cursor in the selection |
| `v` | Begin or end Visual range selection |
| `Ctrl-a` | Select all visible items |
| `n` / `Ctrl-Shift-n` | Create a folder |
| `N` | Create a file from the standard Templates folder |
| `gn` | Create a folder and move the selection into it as one undoable action |
| `r` / `F2` | Rename one item; restore the selection while viewing Trash |
| `R` | Bulk rename selected items with find/replace or numbering preview |
| `yy` / `Ctrl-c` | Copy the selection |
| `yp` / `!` | Copy path(s) as text / open the configured terminal here |
| `ac` / `ax` | Create a ZIP or tar archive / extract the selected archive here |
| `dd` / `Ctrl-x` | Cut the selection |
| `D`, then `y` | Confirm moving the selection to Trash |
| `T` | Open Trash |
| `E` | While viewing Trash, open the typed Empty Trash confirmation |
| `p` / `Ctrl-v` | Paste into the current directory |
| `u` | Undo the most recent rename, move, folder creation, or Trash action |
| `r` / `s` / `n` | Replace, skip, or rename when a paste conflicts |
| `/` | Filter the current directory |
| `f` / `Ctrl-p` | Fuzzy find files and folders below the current directory |
| `Alt-t` / `Alt-d` in Find | Cycle file-type / modified-date filters |
| `Ctrl-Enter` / `Alt-Enter` in Find | Reveal the result / show its properties |
| `i`, `Ctrl-1` / `Ctrl-2` | Toggle view, or choose list / icon grid |
| `:` / `Ctrl-l` | Edit or paste a location (`~` is supported) |
| `.` | Toggle hidden files |
| `s` / `S` | Cycle the sort field / reverse its order |
| `~` | Open the home directory |
| `Ctrl-r` | Refresh |
| `?` | Show the in-app key reference |
| `Esc` | Cancel a staged cut, leave the current mode, or clear selection/filter state |

For user-owned files, Properties shows nine numbered `rwx` controls. Click one
or press its number to toggle the corresponding owner, group, or other mode bit.

Mouse selection, Ctrl-click additive selection, Shift-click range selection,
activation, breadcrumb navigation, column sorting, wheel scrolling, and
scrollbar dragging also work. Right-click opens the same core actions available
from the keyboard. File URL drag-and-drop works between Shibui, folder rows,
Places, and other desktop applications; hold Ctrl to propose Copy instead of
Move.

The recursive finder streams paths as `fd` discovers them and ranks fuzzy
matches against both the item name and its relative path. It follows the hidden
files toggle but intentionally does not treat project ignore files as filesystem
visibility rules. While typing, use arrows or `Ctrl-j` / `Ctrl-k` to choose a
result, then press Enter.
Matched characters are accented; `Ctrl-Enter` reveals the result in its parent
directory and `Alt-Enter` opens Properties without leaving the finder.
Network shares intentionally skip recursive finding and image thumbnails so a
slow server never causes an invisible tree scan. Active and recent connections
appear in Places; bookmark one with `m`, reconnect with Enter, or disconnect it
with `x`.

When multiple pasted items conflict, press `a` before Replace or Skip to apply
that choice to the remaining conflicts. Replace temporarily preserves the old
destination so it can be restored if the transfer fails.

Press `u` to undo up to 20 recent rename, move, folder-creation, and Trash
actions. Undo refuses to guess when an original path has been occupied or a
created folder has gained contents, and explains what changed instead.

Shibui has no direct permanent-delete command. `D` always uses the freedesktop
Trash, and `Esc` closes the confirmation without changing anything. Press `T`
to browse trashed items, `r` to restore the current selection to its original
locations, and `h` to return to the directory you came from. Empty Trash is the
explicit permanent-deletion flow: press `E`, type `EMPTY`, and press Enter.

## Tests

```sh
mkdir -p build/tests
cd build/tests
qmake6 ../../tests/tests.pro
make -j2
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./shibui-tests
```

See [HANDOFF.md](HANDOFF.md) for the product direction and [BACKLOG.md](BACKLOG.md)
for implementation history, remaining verification, and intentionally deferred
features.

## License

Shibui is available under the [MIT License](LICENSE).
