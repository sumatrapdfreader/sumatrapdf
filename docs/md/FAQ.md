# Frequently asked questions

Quick answers to common questions from the [GitHub discussions forum](https://github.com/sumatrapdfreader/sumatrapdf/discussions). Each item links to a page with more detail.

## Opening files and windows

**I enabled tabs but every PDF opens in a new window**

Check `ReuseInstance = true` in [advanced settings](Advanced-options-settings.md). With `ReuseInstance = false`, Windows starts a separate SumatraPDF process for each file, so tabs cannot be shared. See [Tabs and windows](Tabs-and-windows.md).

**How do I open a file in a new window instead of a tab?**

Use `-new-window` on the command line, or `Ctrl + Shift + N` in the app. See [Tabs and windows](Tabs-and-windows.md).

**SumatraPDF asks to install when I double-click a PDF**

This usually means the `.exe` you are running has `-install` in its name (the installer renames itself that way) or `libmupdf.dll` is missing next to the executable. Download the [official build](https://www.sumatrapdfreader.org/download-free-pdf-viewer) and run the installer, or use the portable (self-contained) version. See [Installation](Installation.md) and [Corrupted installation](Corrupted-installation.md).

**SumatraPDF is running as admin and cannot open files from a non-admin process**

Do not run SumatraPDF elevated. Run it as a normal user, or run the program that opens the file with the same privilege level.

## Settings and display

**I changed the default layout but reopening a file ignores it**

`DefaultDisplayMode` and `DefaultZoom` apply when you open a file **for the first time**. After that, [per-document state](How-we-store-settings.md) restores the layout from when you last closed the file. To reset a file, remove its entry from `FileStates` in advanced settings or delete the settings file.

**View settings from the menu don't take effect**

Some settings (for example `UseTabs`) require restarting SumatraPDF. Per-document settings override global defaults for files you have opened before. See [How we store settings](How-we-store-settings.md).

**I accidentally inverted document colors — how do I undo?**

Press `Shift + I` again (`CmdInvertColors`), or set `DocumentColorsFollowTheme = off` in advanced settings. See [Customize theme colors](Customize-theme-colors.md).

**How do I get a dark background without inverting images in EPUBs?**

There is no perfect one-click solution: `Shift + I` inverts the whole rendered page including images. For EPUBs you can tune `EBookUI` (custom CSS, `WindowBgCol`) or use themes — see [Customize eBook UI](Customize-eBook-UI.md).

## Links and navigation

**SumatraPDF turns plain-text URLs, email addresses, and DOIs into clickable links**

SumatraPDF auto-detects URLs, email addresses, and plain-text DOIs (e.g. `10.1109/WICSA.2015.29` → `https://doi.org/...`) in PDF text (a long-standing PDF viewer convention for URLs). Embedded hyperlinks in the file are separate. Set `DisableAutoLinks = true` in advanced settings to turn off auto-detection only. See [Hyperlinks](Hyperlinks.md).

**I followed an internal link (footnote) — how do I go back?**

`Alt + Left` or `Backspace` — same as browser back. See [Scrolling and zooming](Scrolling-and-zooming.md).

## Search

**Search shortcuts don't work / F3 changes volume on my laptop**

`F3` only works when keyboard focus is in the document, not in the find box. On many laptops you need `Fn + F3`. See [Finding text](Finding-text.md).

## Printing

**Command-line printing uses the wrong paper size or tray**

Use explicit `-print-settings` tokens (`paper=`, `bin=`, `paperkind=`). On Windows 11 the print dialog may hide Advanced options — CLI printing avoids that. See [Printing](Printing.md).

## Annotations

**How do I change default highlight color or size?**

Edit the `Annotations` section in advanced settings (`HighlightColor`, `FreeTextSize`, etc.). There is no global undo for annotations. See [Editing annotations](Editing-annotations.md).

## PDF tools (merge, split, delete pages)

**How do I delete the last page without knowing the page count?**

Use `sumatrapdf-tool clean` with `N` as the last-page marker, e.g. `1-N-1` keeps all pages except the last. See [Tools](Tools.md) and [Delete pages from PDF](Tool-x-delete-pages-from-pdf.md).

## Accessibility

**Can SumatraPDF read a document aloud?**

Yes — **pre-release 3.7+** adds Read Aloud via the command palette (`Ctrl + K`, type `read aloud`) using Windows text-to-speech. See [Read Aloud (TTS)](Read-Aloud-TTS.md).

## Default PDF viewer

**SumatraPDF doesn't appear in Windows Default apps**

Install from the [official installer](https://www.sumatrapdfreader.org/download-free-pdf-viewer) (not only the portable exe), then set defaults manually. See [Set as default PDF viewer](Set-as-default-pdf-viewer.md) and [Installation](Installation.md).

## Still stuck?

- Search [discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions) — your question may already be answered
- [Report a bug](How-to-submit-bug-reports.md) if something is broken
- [Request a feature](https://sumatrapdf.userjot.com) on the feature board