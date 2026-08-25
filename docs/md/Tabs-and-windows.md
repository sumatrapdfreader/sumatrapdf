# Tabs and windows

SumatraPDF can open documents in **tabs** inside one window, or in **separate windows**. Which you get depends on two independent settings plus optional command-line flags.

## The two settings

| Setting         | Default | What it does                                                                                      |
| --------------- | ------- | ------------------------------------------------------------------------------------------------- |
| `UseTabs`       | `true`  | New documents open as **tabs** in an existing window instead of always spawning a new window      |
| `ReuseInstance` | `true`  | Opening a file from Explorer or the command line **reuses** an already running SumatraPDF process |

Both live in [advanced settings](Advanced-options-settings.md) (`Settings → Advanced Options...`).

### Common confusion: tabs enabled but new window every time

If `UseTabs = true` but `ReuseInstance = false`, each file is opened by a **new process**, so you always get a separate window — tabs only apply _within_ one process.

**Fix:** set `ReuseInstance = true` and restart SumatraPDF.

This is the most frequent tabs question on the [forum](https://github.com/sumatrapdfreader/sumatrapdf/discussions/5621).

## When settings take effect

| Setting         | Restart required?                       |
| --------------- | --------------------------------------- |
| `ReuseInstance` | No — takes effect on the next file open |
| `UseTabs`       | **Yes** — close and restart SumatraPDF  |

You can also change `ReuseInstance` and `UseTabs` at runtime (**ver 3.7+**) via `Ctrl + K` → `Advanced Settings...` (`CmdAdvancedSettings`). Changing `UseTabs` affects **new** windows only; switching between tabbed and non-tabbed layout for an existing window may still need a restart.

## Command-line flags

| Flag               | Effect                                                                                                                                           |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `-new-window`      | Open each file in a **new window** even when `UseTabs = true` (**ver 3.2+**). Several files: one window per file                                 |
| `-new-window-tabs` | Open **one** new window and load all files as tabs in that window (**ver 3.7+**)                                                                 |
| `-reuse-instance`  | Send the file to an already running instance (mainly for [DDE](DDE-Commands.md) and scripts). For normal use, prefer the `ReuseInstance` setting |

`-reuse-instance` is **not** needed for everyday double-click opening when `ReuseInstance = true`.

## Multiple instances

If several SumatraPDF processes are already running (for example because `ReuseInstance = false`), behavior of `-reuse-instance` is undefined — the file may go to any instance.

To force a single instance, keep `ReuseInstance = true` and close extra windows.

## Session restore

With `RestoreSession = true`, SumatraPDF reopens tabs (and window positions) from the last session on startup. See [How we store settings](How-we-store-settings.md).

`-for-testing` deliberately skips session restore and does not save settings.

## Keyboard shortcuts for tabs

See [Keyboard shortcuts](Keyboard-shortcuts.md):

- `Ctrl + Tab` / `Ctrl + Shift + Tab` — next / previous tab (**ver 3.6+**: opens the **Smart Tab Switch** list while you hold Ctrl; release to switch). Same idea as the browser tab switcher
- `Ctrl + PageDown` / `Ctrl + PageUp` — next / previous tab in **tab-strip order**, immediately, without the switcher popup (`CmdNextTab` / `CmdPrevTab`)
- `Ctrl + W` or `q` — close current tab
- `Ctrl + Shift + N` — open current document in a new window
- `Alt + 1` … `Alt + 8`, `Alt + 9` (last tab) — jump to tab by number

Use the [command palette](Command-Palette.md): type `@` to switch tabs by name.

### Restore pre-3.6 Ctrl+Tab (no switcher popup)

In **3.5 and earlier**, `Ctrl + Tab` / `Ctrl + Shift + Tab` switched tabs immediately in tab-strip order (no overlay list). In **3.6+** those keys run **Smart Tab Switch** (`CmdNextTabSmart` / `CmdPrevTabSmart`), which shows a tab list while Ctrl is held.

If you prefer the old behavior (useful when flicking quickly between two documents), the simplest way (**ver 3.7+**) is to set the `CtrlTabSimple` advanced setting to `true`. From then on `Ctrl + Tab` / `Ctrl + Shift + Tab` switch tabs immediately in tab-strip order and the switcher no longer shows up (set it back to `false` to get the switcher back).

Alternatively:

1. Use **`Ctrl + PageDown` / `Ctrl + PageUp`** instead, or
2. [Rebind the keys](Customize-keyboard-shortcuts.md) in advanced settings so `Ctrl + Tab` runs plain next/prev tab again:

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

Note: the advanced setting `TabsMru` only changes the **order** of tabs inside the Smart Tab Switch list (most-recently-used vs strip order). It does **not** hide the switcher; use `CtrlTabSimple` or the shortcut rebind above for that.

## Home tab

When `UseTabs = true`, an empty window may show a **Home** tab. Set `NoHomeTab = true` in advanced settings to skip it. You can close the Home tab when other tabs are open; reopen it with `Ctrl + K` → `Show Start Page` if needed.

## See also

- [FAQ](FAQ.md) — quick troubleshooting
- [Command-line arguments](Command-line-arguments.md) — `-new-window`, `-new-window-tabs`, `-reuse-instance`
- [How we store settings](How-we-store-settings.md) — `RestoreSession`, `SessionData`
