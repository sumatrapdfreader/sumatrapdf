# Comics and manga

SumatraPDF can open comic-book archives and folders of images as multi-page documents. This page covers reading modes, manga (right-to-left) layout, zoom tips for double-page spreads, and related advanced settings.

## Supported formats

| Kind                | Extensions / how to open                                                                                             |
| ------------------- | -------------------------------------------------------------------------------------------------------------------- |
| Comic book archives | `.cbz`, `.cbr`, `.cbt`, `.cb7` (and `.ora`)                                                                          |
| Archives of images  | `.zip`, `.rar`, `.7z`, `.tar` that contain images                                                                    |
| Image folder        | open a directory of images (each file is a page)                                                                     |
| Single image        | PNG, JPEG, WebP, AVIF, HEIC, GIF (including animation), TIFF, and [other image types](Supported-document-formats.md) |

Encrypted `.cbz` / `.cbr` files are supported (password prompt when needed). Archives opened from a **network drive** may be copied into a local cache (`cbx-cache` under the [settings data directory](How-we-store-settings.md)) so page turns stay fast. Clear that cache with **Delete Cached Files** in the [command palette](Command-Palette.md) (`CmdDeleteCachedFiles`).

Document properties (`Ctrl + D`) for comic archives list image files and, where present, EXIF and ComicInfo / ComicBookInfo metadata.

## Table of contents

The Bookmarks sidebar lists pages in the archive.

- If the archive has a `ComicInfo.xml` with bookmarks, those names are used.
- Otherwise, if images live in **chapter folders**, the folders appear as nested outline entries (click a folder to jump to its first page). A directory shared by every file is omitted, so a comic whose files are all in one folder stays a flat list of file names.

## View layout

Use the same page layout commands as for PDF:

| Action                               | Shortcut / command               |
| ------------------------------------ | -------------------------------- |
| Single page                          | `Ctrl + 6` (`CmdSinglePageView`) |
| Facing (two pages)                   | `Ctrl + 7` (`CmdFacingView`)     |
| Book view (facing, first page alone) | `Ctrl + 8` (`CmdBookView`)       |
| Continuous scroll                    | `c` (`CmdToggleContinuousView`)  |

Facing or book view is useful for double-page spreads. Continuous mode is natural for long webtoon-style strips. See [Scrolling and zooming](Scrolling-and-zooming.md).

To turn the page by clicking the left or right edge of the window (like many comic readers), set `ClickEdgeToTurnPage = true` in [advanced settings](Advanced-options-settings.md). In manga mode the sides are reversed so a click on the left still advances.

## Manga mode (right-to-left)

**Manga mode** displays pages right-to-left in facing and book views (typical for Japanese manga and
right-to-left documents).

- **View** menu or command palette: **Toggle Manga Mode** (`CmdToggleMangaMode`)
- Available for PDF, XPS, DjVu, ebooks, comic books, images and other fixed-page documents
- When manga mode is on, **Left** advances and **Right** goes back (and horizontal swipe matches that), so navigation follows right-to-left reading

### Default for new comic files

In [advanced settings](Advanced-options-settings.md):

```
ComicBookUI [
    CbxMangaMode = true
]
```

That sets the **default** for comic books you have not opened before. Per-file state is stored as `DisplayR2L` under `FileStates` when [remembering state per document](How-we-store-settings.md).

Command-line `-manga-mode true|false` still works but is deprecated in favor of `ComicBookUI.CbxMangaMode`.

## Zoom and double-page spreads

Comics often mix **narrow single pages** with **wide double-page spreads**. If you zoom so singles look good, spreads may be wider than the window; if you fit the spread, singles look too small.

### Limit page size to the window (ver 3.7+)

Advanced settings under `ComicBookUI` (comics) and `ImageUI` (images / image folders):

```
ComicBookUI [
    LimitToWindowWidth = true
    LimitToWindowHeight = false
]
```

| Setting                      | Effect when using an **absolute** zoom (e.g. 150%)                                         |
| ---------------------------- | ------------------------------------------------------------------------------------------ |
| `LimitToWindowWidth = true`  | No page is drawn wider than the window; each page is capped at **Fit Width** for that page |
| `LimitToWindowHeight = true` | No page is drawn taller than the window; each page is capped at **Fit Height**             |

Enable **both** to cap at Fit Page. Caps apply **per page**, so:

1. Set zoom with `+` / `-`, `Ctrl + 1` (100%), or a custom level so **single** pages are the size you want.
2. Turn on `LimitToWindowWidth` (recommended for manga with double spreads).
3. Wide spreads shrink to the window width; smaller pages stay at your chosen zoom.

Virtual zoom modes (**Fit Width**, **Fit Page**, **Shrink to Fit**, etc.) already size relative to the window; the limit settings only affect **percentage** zooms.

### Other useful zoom modes

| Mode                       | When it helps                                                                                                                                 |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| **Fit Width** (`Ctrl + 2`) | Every page fills the window width (spreads and singles both edge-to-edge; singles can become very large)                                      |
| **Fit Page** (`Ctrl + 0`)  | Whole page(s) visible                                                                                                                         |
| **Shrink to Fit**          | Never enlarge past 100%; only shrink pages that are larger than the window ([like IrfanView “fit only big images”](Scrolling-and-zooming.md)) |
| **Fit by Orientation**     | Fit width in landscape viewports, fit page in portrait                                                                                        |
| **Fit Height**             | Page height fills the window (handy for landscape pages)                                                                                      |

Default zoom for **single image files** is controlled by `ImageUI.DefaultZoom` (default `shrink to fit`). Comic archives use `ComicBookUI.DefaultZoom` on first open (empty keeps the global `DefaultZoom`); a remembered zoom for that file still wins. Set `ComicBookUI.DefaultZoom = fit width` to open new comics at Fit Width while PDFs stay at Fit Page.

## Background color

Comics and images use a **black** canvas by default (unlike PDF’s white). Override it:

```
ComicBookUI [
    WindowBgCol = #1a1a1a
]

ImageUI [
    WindowBgCol = #1a1a1a
]
```

Values accept normal colors or `checkered` for a transparency checkerboard. You can also use **Change Background Color** (`CmdChangeBackgroundColor`) from the command palette. UI theme colors are separate — see [Customize theme colors](Customize-theme-colors.md).

## Margins and page spacing

```
ComicBookUI [
    WindowMargin = 0 0 0 0
    PageSpacing = 4 4
]
```

- `WindowMargin` — top, right, bottom, left gap between the window and the document (default all zeros for comics)
- `PageSpacing` — horizontal and vertical gap between pages in facing / book view

Sizes are in pixels at 100% display scaling and are DPI-scaled.

## Page info tip

Press `i` (`CmdTogglePageInfo`) for the page-info tip. For comics and image folders it shows the current image **file name** and size (both pages when two are visible in facing view). For a single open image it can show pixel resolution, file size, and DPI when not the default 96.

## Convert to PDF

**File → Convert to PDF…** / **Document → Convert to PDF…** (`CmdConvertToPDF`) turns a comic, image folder, or single image into a multi-page PDF. Full details: [Convert to PDF](Convert-to-PDF.md).

**Convert page to PDF** on the image context menu (`CmdConvertImageToPdf`) saves only the **current page** via the image editor.

## Copy image

Right-click a comic or image page and choose **Copy Image** (`CmdCopyImage`) to copy the page image to the clipboard.

## Image folders

Opening a **folder of images** treats each image as a page (ordered by file name). Bookmarks sidebar lists the files. The same `ImageUI` settings apply (`DefaultZoom`, `LimitToWindowWidth` / `Height`, `WindowBgCol`).

## What is not available for comics / images

These PDF-oriented features do not apply to comic archives, image folders, or plain images:

- In-document text search and text selection (there is no extractable text)
- **Read Aloud** (menu, context menu, toolbar) — no text to speak
- Keyboard link following (`Shift + F`)
- [AI Chat with document](AI-Chat-with-document.md) (PDF only)

## Quick settings reference

### `ComicBookUI`

```
ComicBookUI [
    WindowMargin = 0 0 0 0
    PageSpacing = 4 4
    CbxMangaMode = false
    WindowBgCol =
    LimitToWindowWidth = false
    LimitToWindowHeight = false
    DefaultDisplayMode =
    DefaultZoom =
]
```

### `ImageUI`

```
ImageUI [
    WindowBgCol =
    DefaultZoom = shrink to fit
    LimitToWindowWidth = false
    LimitToWindowHeight = false
]
```

Full field comments: [Advanced options / settings](Advanced-options-settings.md).

### Related commands

| Command                                                     | Purpose                                                            |
| ----------------------------------------------------------- | ------------------------------------------------------------------ |
| `CmdToggleMangaMode`                                        | Toggle right-to-left facing/book layout                            |
| `CmdConvertToPDF`                                           | Convert comic / image folder / image to a multi-page PDF           |
| `CmdConvertImageToPdf`                                      | Convert the current page via the image editor                      |
| `CmdCopyImage`                                              | Copy current page image                                            |
| `CmdTogglePageInfo`                                         | Show / hide page info tip (`i`)                                    |
| `CmdDeleteCachedFiles`                                      | Clear local `cbx-cache` copies from network opens                  |
| `CmdChangeBackgroundColor`                                  | Pick canvas background                                             |
| `CmdZoomFitWidth` / `CmdZoomFitPage` / `CmdZoomShrinkToFit` | Zoom modes (see [Scrolling and zooming](Scrolling-and-zooming.md)) |

## See also

- [Convert to PDF](Convert-to-PDF.md)
- [Supported document formats](Supported-document-formats.md)
- [Scrolling and zooming](Scrolling-and-zooming.md)
- [Advanced options / settings](Advanced-options-settings.md)
- [Commands](Commands.md)
- [How we store settings](How-we-store-settings.md)
