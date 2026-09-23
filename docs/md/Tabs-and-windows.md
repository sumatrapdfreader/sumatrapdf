# Tabs and windows

SumatraPDF opens documents in **tabs** inside one window, or in **separate windows**. Which you get depends on two independent settings, `UseTabs` and `ReuseInstance`, plus optional command-line flags.

**By default files open as tabs in the running SumatraPDF window.**

- **Tabs:** `UseTabs = true` opens new documents as tabs
- **Single instance:** `ReuseInstance = true` sends files from Explorer or the command line to the running process
- **Switch tabs:** `Ctrl + Tab` (Smart Tab Switch), `Ctrl + PageDown` / `Ctrl + PageUp`, `Alt + 1` … `Alt + 9`
- **New window:** `Ctrl + Shift + N`, or `-new-window` / `-new-window-tabs` on the command line
- **Session restore:** `RestoreSession = true` reopens last session's tabs

## Open documents in tabs

Set both in [advanced settings](Advanced-options-settings.md) (**Settings → Advanced Settings...**):

- `UseTabs = true` — new documents open as tabs in an existing window
- `ReuseInstance = true` — files opened from Explorer or the command line go to the running process

Both default to `true`. `UseTabs` needs a restart; `ReuseInstance` takes effect on the next file open.

## Fix: tabs enabled but new window every time

If `UseTabs = true` but `ReuseInstance = false`, each file is opened by a **new process**, so you always get a separate window — tabs only apply _within_ one process.

**Fix:** set `ReuseInstance = true` and restart SumatraPDF.

This is the most frequent tabs question on the [forum](https://github.com/sumatrapdfreader/sumatrapdf/discussions/5621).

## Open a document in a new window

- Shortcut: `Ctrl + Shift + N` opens the current document in a new window
- Command-line: `-new-window` opens each file in a new window even when `UseTabs = true` (**ver 3.2+**)
- Command-line: `-new-window-tabs` opens **one** new window with all files as tabs (**ver 3.7+**)

See the flags table under Reference.

## Switch tabs

See [Keyboard shortcuts](Keyboard-shortcuts.md):

- `Ctrl + Tab` / `Ctrl + Shift + Tab` — next / previous tab (**ver 3.6+**: opens the **Smart Tab Switch** list while you hold Ctrl; release to switch). Same idea as the browser tab switcher
- `Ctrl + PageDown` / `Ctrl + PageUp` — next / previous tab in **tab-strip order**, immediately, without the switcher popup (`CmdNextTab` / `CmdPrevTab`)
- `Alt + 1` … `Alt + 8`, `Alt + 9` (last tab) — jump to tab by number
- [Command Palette](Command-Palette.md): `Ctrl + K`, then type `@` to switch tabs by name

## Restore pre-3.6 Ctrl+Tab (no switcher popup)

In **3.5 and earlier**, `Ctrl + Tab` / `Ctrl + Shift + Tab` switched tabs immediately in tab-strip order (no overlay list). In **3.6+** those keys run **Smart Tab Switch** (`CmdNextTabSmart` / `CmdPrevTabSmart`), which shows a tab list while Ctrl is held.

To get the old behavior (useful when flicking quickly between two documents):

- Setting (**ver 3.7+**, simplest): set `CtrlTabSimple = true`. `Ctrl + Tab` / `Ctrl + Shift + Tab` then switch immediately in tab-strip order; set it back to `false` to get the switcher back.
- Shortcut: use `Ctrl + PageDown` / `Ctrl + PageUp` instead.
- Rebind: [rebind the keys](Customize-keyboard-shortcuts.md) in advanced settings so `Ctrl + Tab` runs plain next/prev tab:

```
Shortcuts [
    [
        Cmd = CmdNextTab
        Key = Ctrl + Tab
    ]
    [
        Cmd = CmdPrevTab
        Key = Ctrl + Shift + Tab
    ]
]
```

Note: `TabsMru` only changes the **order** of tabs inside the Smart Tab Switch list (most-recently-used vs strip order). It does **not** hide the switcher; use `CtrlTabSimple` or the rebind above.

## Close a tab

- Shortcut: `Ctrl + W` or `q`

## Show or hide the Home tab

When `UseTabs = true`, an empty window may show a **Home** tab.

- Setting: `NoHomeTab = true` skips it
- Close it when other tabs are open; reopen with `Ctrl + K`, then `Go To Home Page` in [Command Palette](Command-Palette.md)

## Reopen tabs on startup

With `RestoreSession = true`, SumatraPDF reopens tabs (and window positions) from the last session on startup. See [How we store settings](How-we-store-settings.md).

Note: `-for-testing` deliberately skips session restore and does not save settings.

## Tips

- If every file opens a new window despite `UseTabs = true`, set `ReuseInstance = true`.
- Use `Ctrl + PageDown` / `Ctrl + PageUp` to switch tabs without the switcher popup.
- Use `Ctrl + K`, `@` to find a tab by name when many are open.
- Use `-new-window-tabs` to open a batch of files together in their own window.

## Reference

### Settings

| Setting         | Default | What it does                                                                                      |
| --------------- | ------- | ------------------------------------------------------------------------------------------------- |
| `UseTabs`       | `true`  | New documents open as **tabs** in an existing window instead of always spawning a new window      |
| `ReuseInstance` | `true`  | Opening a file from Explorer or the command line **reuses** an already running SumatraPDF process |

| Setting         | Restart required?                       |
| --------------- | --------------------------------------- |
| `ReuseInstance` | No — takes effect on the next file open |
| `UseTabs`       | **Yes** — close and restart SumatraPDF  |

You can also change `ReuseInstance` and `UseTabs` at runtime (**ver 3.7+**) via `Ctrl + K`, `Advanced Settings...` in [Command Palette](Command-Palette.md) (`CmdAdvancedSettings`). Changing `UseTabs` affects **new** windows only; switching between tabbed and non-tabbed layout for an existing window may still need a restart.

### Command-line flags

| Flag               | Effect                                                                                                                                           |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `-new-window`      | Open each file in a **new window** even when `UseTabs = true` (**ver 3.2+**). Several files: one window per file                                 |
| `-new-window-tabs` | Open **one** new window and load all files as tabs in that window (**ver 3.7+**)                                                                 |
| `-reuse-instance`  | Send the file to an already running instance (mainly for [DDE](DDE-Commands.md) and scripts). For normal use, prefer the `ReuseInstance` setting |

`-reuse-instance` is **not** needed for everyday double-click opening when `ReuseInstance = true`.

### Multiple instances

If several SumatraPDF processes are already running (for example because `ReuseInstance = false`), behavior of `-reuse-instance` is undefined — the file may go to any instance.

To force a single instance, keep `ReuseInstance = true` and close extra windows.

## See also

- [FAQ](FAQ.md) — quick troubleshooting
- [Command-line arguments](Command-line-arguments.md) — `-new-window`, `-new-window-tabs`, `-reuse-instance`
- [How we store settings](How-we-store-settings.md) — `RestoreSession`, `SessionData`
