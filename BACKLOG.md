# Shibui backlog

## Goal

Shibui is replacement-ready when it can be the default file manager on an
Omarchy workstation for ordinary file work without regularly sending the
user back to Nautilus. That means browsing, finding, inspecting, opening,
organizing, recovering, and moving files to and from removable storage with
both keyboard and pointer. It must also connect to the common network shares an
Omarchy user would expect from a desktop file manager.

This is not a Nautilus parity project. Shibui should cover standard file-manager
work, feel native to Omarchy, make the keyboard the fastest path through every
workflow, and remain extremely responsive. Extensibility and specialized tools
are not part of the mission.

## Priority

- **P0** is required for the replacement candidate.
- **P1** is valuable after Shibui becomes the default file manager.
- **Later** is intentionally outside the initial replacement target.

Work milestone by milestone. Each milestone should leave a usable application;
do not begin several partially working feature areas at once.

## Product gates

These apply to every backlog item, not to a final polish phase.

### Omarchy-native

- Use the active Omarchy palette, monospace font, icon theme, rounding, and
  spacing, including live theme changes.
- Follow Omarchy's compact proportions and restrained state treatment without
  importing its private QML modules or imitating GTK application chrome.
- Behave as a normal Wayland application with a stable app ID, desktop entry,
  file associations, and the configured Omarchy terminal where relevant.

### Keyboard-driven

- Every routine action must have a direct Normal-mode key or a short, visible
  key sequence. Pointer access is required, but it is not the primary design
  path.
- Focus, keyboard cursor, selection, current mode, and pending key sequences
  must always be unambiguous. Normal-mode actions must never leak into prompts.
- Opening `?` must show a compact, context-sensitive key reference. New features
  are incomplete until their keyboard path and help entry exist.
- Navigation should preserve cursor and scroll context. A completed operation
  should put focus somewhere useful instead of resetting the view.

### Performance

- The UI thread must never perform directory enumeration, recursive search or
  sizing, file transfer, network I/O, or thumbnail decoding.
- Render the first useful rows immediately and fill in nonessential metadata,
  icons, and thumbnails lazily. Cancel stale work when the user navigates away.
- Virtualize list and grid delegates. Holding `j` or `k` and scrolling must stay
  at the display refresh rate in a directory with 10,000 entries, including
  while thumbnails or a transfer are active.
- Target a warm launch to an interactive ordinary local directory within 200 ms
  on the reference Omarchy development machine. Record a simple repeatable
  measurement; do not build a telemetry or benchmarking framework.
- Slow disks, unavailable shares, and large directories may delay their own
  results, but they must never delay input or navigation elsewhere in the UI.

## 0.1 — Read-only browser

This is the first vertical slice from `HANDOFF.md`.

- [x] **P0 — Application shell.** Open one Wayland window with the stable
      `shibui` app ID; accept an optional local path on the command line and use
      the home directory otherwise.
- [x] **P0 — Live Omarchy appearance.** Load and watch the active colors, font,
      icon theme, rounding, and spacing without depending on Omarchy's private
      QML modules.
- [x] **P0 — Directory list.** Show a virtualized list with name, size, type,
      and modification time; keep directories first and support sorting by each
      displayed field.
- [x] **P0 — Navigation.** Implement mouse activation, arrows, breadcrumbs,
      `j`, `k`, `h`, `l`, `Enter`, `gg`, `G`, half-page movement, and Home.
- [x] **P0 — Keyboard reference.** Show the current mode and implement the `?`
      key reference from the beginning so the interaction model stays
      discoverable as it grows.
- [x] **P0 — Current-directory filter.** Implement `/`, live matching, visible
      mode, and predictable `Esc` behavior.
- [x] **P0 — Hidden files.** Toggle them with `.` without losing a sensible
      selection.
- [x] **P0 — File activation.** Open regular files with their default desktop
      application and handle directories, symlinks, and broken symlinks
      explicitly.
- [x] **P0 — Live filesystem changes.** Reconcile external creates, deletes,
      renames, and directory disappearance without stale rows or invalid
      selection.
- [x] **P0 — Read-only failure states.** Show useful loading, empty-directory,
      missing-path, and permission-denied states without blocking the UI.

**Exit:** browsing real local directories for an hour is comfortable and a
theme change is applied without restarting.

## 0.2 — Safe file organizer

- [x] **P0 — Selection model.** Add multiple selection and Visual range
      selection with one clearly distinguished keyboard cursor.
- [x] **P0 — Create and rename.** Create folders and rename one item with inline
      validation, collision handling, and errors that leave the user's input
      intact.
- [x] **P0 — Copy, cut, and paste.** Operate on the current selection, show the
      pending `yy` or `dd` sequence, and preserve the selection until the
      operation has actually begun.
- [x] **P0 — Standard selection commands.** Support Select All, clear selection,
      contiguous range selection, and additive pointer selection without
      weakening the Normal/Visual mode model.
- [x] **P0 — Transfer UI.** Run the one active copy or move off the UI thread;
      show source, destination, progress, current item, failure, and Cancel.
- [x] **P0 — Conflict prompts.** Support Replace, Skip, Rename, and Cancel, plus
      an apply-to-remaining choice when more than one item is involved.
- [x] **P0 — Trash, never direct delete.** Move selected items to the
      freedesktop trash with confirmation and clear partial-failure reporting.
- [x] **P0 — Trash location.** Browse trashed items and restore them to their
      original location. Add Empty Trash only as a separate, strongly confirmed
      action; do not add a permanent-delete shortcut.
- [x] **P0 — Undo recent mutations.** Keep a small in-memory undo history for
      successful rename, move, create-folder, and trash actions. If external
      changes make an undo unsafe, explain why instead of guessing.
- [x] **P0 — Desktop clipboard interoperability.** Exchange copied and cut file
      URLs with other freedesktop file managers and applications, rather than
      maintaining only a private clipboard.
- [x] **P0 — Drag and drop.** Support selection and dragging between Shibui,
      other applications, the sidebar, and directories in the current view,
      with copy-versus-move intent made visible.
- [x] **P0 — Context actions.** Provide a concise pointer-accessible context
      menu for the same core operations available from the keyboard.

**Exit:** every mutation has a visible result or error, accidental trash and
rename actions can be recovered, and copying to or from another file manager
works.

## 0.3 — Fast navigation and finding

- [x] **P0 — History.** Add Back and Forward navigation while preserving the
      selected item and scroll position when practical.
- [x] **P0 — Editable location.** Let the breadcrumb become a path entry for
      pasting or typing an absolute path, including `~` expansion.
- [x] **P0 — Places sidebar.** Show Home, standard XDG user directories, Trash,
      and mounted volumes; make the sidebar hideable so the file list can stay
      compact.
- [x] **P0 — Bookmarks.** Add, rename, reorder, and remove folder bookmarks and
      persist only this small piece of user state.
- [x] **P0 — Fuzzy recursive file finder.** From the current directory, use
      a cancellable background path scan—the same no-index approach as
      `rg --files`—to stream files and folders from the entire subtree. Rank
      them by fuzzy matches against the basename and relative path so an exact
      target name is unnecessary. Keep typing and result navigation responsive,
      respect hidden and ignored-file settings, distinguish finding from
      current-directory filtering, do not hide filesystem entries merely because
      project ignore files list them, and show each result's relative location.
- [x] **P0 — Useful search refinements.** Filter results by file type and
      modified date. Defer content search until a real need justifies using an
      existing system index.
- [x] **P0 — Tabs.** Add conventional tabs only after history and location state
      are solid. Opening a folder in a new tab must work by keyboard, middle
      click, and context menu.

**Exit:** common and recently visited locations are reachable in one or two
actions, pasted paths work, and files can be found below the current directory
without leaving Shibui.

## 0.4 — Inspect, choose, and open

- [x] **P0 — Grid view.** Add a virtualized icon grid and keep selection,
      sorting, filtering, and keyboard behavior consistent with list view.
- [x] **P0 — Thumbnails.** Generate cheap local image thumbnails asynchronously,
      cancel work for off-screen items, and use themed MIME icons for everything
      else. Do not build a general thumbnail service.
- [x] **P0 — Space preview.** Preview common images, text, and PDFs without
      changing the selection. Unsupported formats should fail quietly back to
      the MIME icon and properties.
- [x] **P0 — Open With.** Choose an installed compatible application and set the
      default MIME association through freedesktop mechanisms.
- [x] **P0 — Properties.** Show name, exact path, MIME type, size, timestamps,
      owner, permissions, symlink target, and filesystem free space. Calculate
      recursive folder size only on explicit request and allow cancellation.
- [x] **P1 — Permission editing.** Change ordinary owner/group/other mode bits
      for files owned by the user. Do not add privilege escalation or root mode.
- [x] **P1 — Copy path and open terminal here.** Expose these as small context
      actions using the configured Omarchy terminal.

**Exit:** visual directories are pleasant to browse, alternate applications are
available, and the user can inspect a suspicious or unfamiliar item before
acting on it.

## 0.5 — Devices, network shares, archives, and replacement

- [x] **P0 — Removable volumes.** Reflect device arrival and removal in the
      Places sidebar and support mount, unmount, and eject through the desktop's
      existing storage service. Show busy and permission failures clearly.
- [x] **P0 — Safe removal during operations.** Cancel or fail a transfer cleanly
      if its source or destination disappears; never leave the UI claiming the
      operation completed.
- [x] **P0 — Connect to network locations.** Open SMB and SFTP locations by URI
      through the desktop's existing GIO/GVfs facilities. Support WebDAV and NFS
      when the same backend exposes them without protocol-specific code in
      Shibui.
- [x] **P0 — Network authentication.** Use the existing desktop authentication
      and credential-storage flow. Keep password prompts fully keyboard
      navigable; Shibui must never store credentials itself.
- [x] **P0 — Remote file operations.** Browse, open, create folders, rename,
      copy, move, and resolve conflicts on connected shares using the same
      interaction model as local files. Use remote trash only when the backend
      explicitly supports it; never silently turn Trash into permanent delete.
- [x] **P0 — Network locations in Places.** Show active connections, allow them
      to be bookmarked and disconnected, and remember recent server addresses
      without retaining credentials.
- [x] **P0 — Network failure states.** Keep the UI responsive through latency,
      disconnection, authentication failure, and reconnect attempts. Do not
      recursively prefetch or thumbnail remote trees.
- [x] **P1 — Share discovery.** Display servers advertised by the desktop's
      existing network discovery service when available; do not implement a
      discovery service in Shibui.
- [x] **P0 — Archive basics.** Extract common archives and create ZIP or tar
      archives from the current selection using an established installed
      library or desktop facility. Archive browsing can remain out of scope.
- [x] **P0 — Desktop entry.** Install a proper application launcher, icons, and
      directory MIME association metadata, but do not replace Nautilus
      automatically.
- [x] **P0 — External invocation.** Reuse or open an appropriate Shibui window
      when another application asks the desktop to reveal a local directory or
      supported network URI.
- [ ] **P0 — Replacement verification.** Exercise navigation, search, each file
      mutation, trash restore, archive operations, tabs, thumbnails, and
      removable devices under Hyprland using both keyboard and pointer. Repeat
      the core workflows against an SMB or SFTP share with simulated latency
      and a mid-operation disconnect.
      - [x] Automated local workflows, keyboard contract, archive round-trip,
            10,000-item responsiveness, install staging, and disconnect failure.
      - [x] Native launch and window identity checked in the running Hyprland session.
      - [x] Live key-authenticated SFTP browse, create, rename, copy, move,
            conflict, undo, and mid-transfer disconnect rollback checked in the
            running Hyprland session.
      - [ ] Physical removable-device mount/eject and a latency-injected network
            round-trip still require suitable test hardware or infrastructure.

**Exit:** after an intentional manual switch of the directory MIME association,
ordinary local, removable-device, and network-share workflows no longer require
Nautilus.

## After the replacement candidate

- [x] **P1 — Recent files.** Read the desktop's existing recent-file data and
      respect its privacy setting; do not create a second history database.
- [x] **P1 — Bulk rename.** Support a focused find/replace and numbered rename
      flow with a full preview before applying changes.
- [x] **P1 — Templates.** Create documents from the user's standard Templates
      directory without inventing a template system.
- [x] **P1 — New folder with selection.** Create a folder and move the current
      selection into it as one undoable action.
- [ ] **P1 — Tree list.** Allow expandable folders in list view if users show a
      concrete need; do not make it a separate filesystem model.
- [ ] **P1 — Starred files.** Add only if bookmarks and Recent do not cover the
      observed workflow.
- [ ] **Later — Content search.** Integrate an existing desktop index only if
      recursive filename search proves insufficient. Shibui should not own an
      indexing daemon or database.

## Intentionally not planned

- Permanent-delete shortcut or root/admin mode.
- Plugin, extension, or script system of any kind.
- Split panes.
- Cloud-provider integrations.
- Built-in terminal, editor, archive browser, or disk-usage analyzer.
- Independent theme picker or a broad settings screen.
- Background daemon, persistent operation queue, or Shibui-owned search index.

## Reference baseline

The replacement target is informed by the current GNOME Files feature set, but
is deliberately narrower:

- <https://apps.gnome.org/en/Nautilus/>
- <https://help.gnome.org/gnome-help/files-browse.html>
- <https://help.gnome.org/gnome-help/files-delete.html>
- <https://help.gnome.org/gnome-help/files-open.html>
- <https://help.gnome.org/gnome-help/nautilus-file-properties-basic.html>
- <https://help.gnome.org/gnome-help/nautilus-bookmarks-edit.html>
