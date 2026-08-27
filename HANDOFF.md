# Shibui handoff

## What the name means

*Shibui* describes a quiet, understated kind of beauty: simple at first glance,
careful in its details, and free of decoration that exists only to attract
attention. That is the intended character of the application.

## Vision

Shibui is a fast graphical file manager for Omarchy. It should feel like part of
the desktop rather than a GTK or KDE application placed on top of it.

The interaction model is keyboard-first and Vim-inspired, but not a Vim emulator.
The mouse and conventional arrow keys remain useful. Common navigation should be
possible without leaving normal mode or reaching for a modifier key.

The product should be calm, direct, and trustworthy:

- Open instantly into a useful directory.
- Make the selected item and current mode unmistakable.
- Follow the active Omarchy theme, font, icon theme, rounding, and spacing.
- Keep routine navigation to one or two keystrokes.
- Treat file mutations as serious operations with clear feedback.
- Prefer one polished workflow over a collection of half-finished features.

## Product principles

1. **Keyboard first, not keyboard only.** Vim-style navigation is the primary
   path; mouse, touchpad, arrows, and standard activation still work.
2. **A small modal vocabulary.** Start with Normal, Filter, Visual selection,
   and Prompt modes. Do not build a general Vim command language.
3. **Omarchy-native presentation.** Use Omarchy's live design tokens rather than
   shipping a parallel theme picker.
4. **Safe file operations.** Deletion means trash. Shibui has no permanent-delete
   command or root mode in the first release.
5. **Local files first.** A dependable local file manager is more valuable than
   premature support for remote providers, plugins, or virtual filesystems.
6. **A separate application process.** Shibui is not an Omarchy shell plugin.
   Slow disks, large directories, thumbnail work, and application bugs must not
   threaten the bar, notifications, or lock screen.

## Original v0.1 proposal

- One window and one file pane.
- Breadcrumb path and a small Places sidebar.
- List view with name, size, type, and modification time.
- Open files with their default application.
- Create directory, rename, copy, cut, paste, and move to trash.
- Multiple selection and visual range selection.
- Sort by name, size, type, or modified time; directories first.
- Toggle hidden files.
- Incremental filtering of the current directory.
- Image thumbnails where they are cheap to produce; themed MIME icons otherwise.
- Automatic adoption of the active Omarchy colors, font, icon theme, and geometry.
- Clear progress and conflict prompts for file operations.

### Explicitly not v0.1

- Tabs or split panes.
- Network shares and cloud providers.
- Removable-device mounting UI.
- Plugin or scripting systems.
- A database, indexer, daemon, or background service.
- Archive browsing, bulk rename, Git integration, or a built-in terminal.
- A settings screen or independent theme system.
- Nautilus feature parity.

## Initial keyboard model

This is a starting contract, not an invitation to reproduce every Vim command.

| Key | Normal-mode action |
| --- | --- |
| `j` / `k` | Select next / previous item |
| `h` | Go to parent directory |
| `l` or `Enter` | Open directory or launch file |
| `gg` / `G` | Select first / last item |
| `Ctrl-d` / `Ctrl-u` | Move half a page down / up |
| `/` | Enter Filter mode |
| `.` | Toggle hidden files |
| `Space` | Toggle the selected item in the selection set |
| `v` | Begin or end Visual range selection |
| `yy` | Copy the selection |
| `dd` | Cut the selection |
| `p` | Paste into the current directory |
| `r` | Rename the selected item |
| `n` | Create a directory |
| `D` | Move the selection to trash after confirmation |
| `Esc` | Return to Normal mode or clear transient state |

Normal-mode keys must never fire while a text prompt has focus. The current mode
should be visible but subtle. File operation sequences such as `yy` and `dd`
should show their pending state instead of relying on invisible timeout behavior.

## Technical direction

Use **Qt 6 with QML** for the application and a small C++ backend.

Why this direction:

- Qt 6 and Qt Quick are already present on Omarchy because the shell uses
  Quickshell/QML.
- Custom QML controls can match Omarchy without inheriting KDE widget styling.
- `QFileSystemModel`, `QMimeDatabase`, `QFile`, `QDir`, `QDesktopServices`, and
  `QFileSystemWatcher` cover most of the first release without another framework.
- A thin C++ boundary gives file operations typed paths and errors without adding
  a Rust/C++ bridge or a separate service.

Suggested source layout once implementation begins:

```text
CMakeLists.txt
src/
  main.cpp
  filesystemmodel.{h,cpp}
  fileoperations.{h,cpp}
qml/
  Main.qml
  Theme.qml
  components/
tests/
```

Keep the backend concrete. There is one local filesystem implementation, so it
does not need repository interfaces, provider registries, dependency injection,
or a service layer. When copy and move operations arrive, run the active operation
off the UI thread and expose its progress directly; do not introduce a job system
or persistent queue.

## Omarchy integration

Treat the installed Omarchy shell as the visual reference, not as an application
runtime dependency. In particular, do not load Shibui into the long-running shell
process and do not edit anything under `/usr/share/omarchy/`.

Read the active values from:

```text
~/.local/state/omarchy/current/theme/colors.toml
~/.local/state/omarchy/current/theme/shell.toml
~/.local/state/omarchy/current/theme/icons.theme
~/.local/state/omarchy/current/theme.name
```

Also:

- Resolve the desktop monospace family with
  `fc-match -f '%{family[0]}' monospace`.
- Read `decoration:rounding` and `general:gaps_out` through `hyprctl -j getoption`
  when matching interior control geometry.
- Watch the active theme state and reload tokens without restarting Shibui.
- Use the selected freedesktop icon theme through `QIcon`.
- Use a normal Wayland toplevel and a stable `shibui` app ID.
- Consult `/usr/share/omarchy/shell/Commons/` and
  `/usr/share/omarchy/shell/Ui/` for behavior and proportions. Do not depend on
  those private QML modules directly or copy the entire component library.

The important visual traits are the restrained surface palette, thin state
borders, active-theme accent, compact spacing, monospace typography, and strong
keyboard cursor. Avoid a GNOME-style header bar and avoid reproducing a generic
KDE file manager in different colors.

Once Shibui is dependable, it can replace Omarchy's existing Nautilus launcher
and directory MIME association. Do not change either during early development.

## First vertical slice

Build a read-only browser before file mutations:

1. Create the Qt/QML application and one regular Wayland window.
2. Load the current Omarchy palette, font, icon theme, rounding, and spacing.
3. Show the home directory, or a path supplied on the command line.
4. Render a virtualized list with directories first.
5. Implement `j`, `k`, `h`, `l`, `Enter`, `gg`, `G`, `/`, `.`, and `Esc`.
6. Open a regular file through its default application.
7. React correctly when files are added, removed, or renamed externally.

The slice is complete when it is pleasant enough to browse real directories for
an hour without falling back to the mouse, has no file mutation code, and follows
an Omarchy theme change without being restarted.

## After the first slice

Add capabilities in this order, keeping the application usable after each step:

1. Multiple and Visual selection.
2. Rename and create directory.
3. Move to trash.
4. Copy, cut, and paste with progress and conflict handling.
5. Places sidebar and history.
6. Cheap image thumbnails and Space preview.

Use temporary-directory tests for path navigation and every mutating file
operation. Verify the complete interaction manually under Hyprland. Broad CI,
packaging, and distribution work can wait until the local application is useful.

## Repository state

Shibui is now a working replacement candidate rather than the initial design-only
repository described above. [BACKLOG.md](BACKLOG.md) is the authoritative record:
the implementation milestones through devices, network shares, archives, and
desktop integration are complete, as are Recent, bulk rename, Templates, and
new-folder-with-selection. The automated suite currently passes 30 tests, and a
live SFTP round-trip plus mid-transfer disconnect rollback has been exercised in
the running Hyprland session.

The remaining replacement-verification gate is deliberately external: test a
physical removable device and a latency-injected network share. Conditional
Tree list, Starred, and content-search ideas remain unstarted because no concrete
need has justified them.
