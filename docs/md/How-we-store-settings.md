# How we store settings

## Where we store settings

Persisted data is stored in `SumatraPDF-settings.txt` file.

In portable version the file is stored in the same directory as SumatraPDF executable. In non-portable version, it's in `%LOCALAPPDATA%\SumatraPDF` directory.

Starting with version 1.6 we also persist thumbnails for "Frequently read" list. They are stored in subdirectory `sumatrapdfcache` as `.png` files.

Override the directory with `-appdata <path>` on the command line — see [Installation](Installation.md).

See [https://www.sumatrapdfreader.org/settings/settings](https://www.sumatrapdfreader.org/settings/settings) for information about all the settings.

## How settings apply

Not every setting works the same way. Understanding the three layers avoids "I changed it but nothing happened" confusion.

### 1. Global defaults (first open)

Settings like `DefaultDisplayMode` and `DefaultZoom` in [advanced settings](Advanced-options-settings.md) apply when you open a document **for the first time** — when there is no saved state for that file yet.

The **Settings → Options** dialog ("remember these settings for each document") writes global defaults into the same settings file. They do **not** retroactively change files you have already opened.

### 2. Per-document state (reopen)

When `RememberStatePerDocument = true` (default), SumatraPDF saves display settings per file in the `FileStates` section: zoom, scroll position, view mode, sidebar width, tab color, etc.

The next time you open that file, **saved state wins** over global defaults. This is why changing "default two-page view" does not affect a PDF you have opened before — see [FAQ](FAQ.md).

To reset one file: delete its `[[FileStates]]` block from the settings file, or delete the entire settings file to reset everything.

### 3. Restart-required settings

Some settings apply only after restarting SumatraPDF:

- `UseTabs` — tabbed vs separate windows for **new** windows
- Others noted in [advanced settings](Advanced-options-settings.md) (e.g. some UI layout options)

`ReuseInstance` and most color/theme changes take effect immediately or on the next file open.

### Session restore

| Setting | Effect |
| --- | --- |
| `RestoreSession = true` | On startup, reopen tabs and window layout from `SessionData` |
| `RememberOpenedFiles = true` | Track history for Home / `#` command palette |
| `LazyLoading = true` | When restoring session, load tab content only when the tab is selected |

`-for-testing` skips restore and does not save settings.

### What is not in the settings file

- **Unsaved annotations** — kept in memory until you save or discard; see [Editing annotations](Editing-annotations.md)
- **Installer / restrict policy** — `sumatrapdfrestrict.ini` beside the executable; see [Configure for restricted use](Configure-for-restricted-use.md)

## Reset to defaults

Delete `SumatraPDF-settings.txt`. SumatraPDF recreates it on next launch with factory defaults.

## See also

- [FAQ](FAQ.md)
- [Tabs and windows](Tabs-and-windows.md) — `UseTabs`, `ReuseInstance`, `RestoreSession`
- [Advanced options / settings](Advanced-options-settings.md)