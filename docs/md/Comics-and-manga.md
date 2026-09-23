# Comics and manga

SumatraPDF opens comic-book archives and folders of images as multi-page documents. Use it to read comics page by page, as two-page spreads, or right-to-left for manga.

**Open a `.cbz` / `.cbr` file and press `Ctrl + 7` for two-page spreads.** At a glance:

- **Page layout:** single page `Ctrl + 6`, facing `Ctrl + 7`, book view `Ctrl + 8`, continuous `c`.
- **Manga mode:** right-to-left pages in facing and book views. **View → Manga Mode**
- **Zoom caps:** keep wide spreads inside the window while single pages stay at your zoom (ver 3.7+).
- **Landscape pages as spreads:** a wide page takes the whole two-page row (ver 3.7+).
- **Page info:** image file name and size. `i`
- **Convert to PDF:** turn a comic or image folder into a multi-page PDF.

## Open a comic or image folder

| Kind                | Extensions / how to open                                                                                             |
| ------------------- | -------------------------------------------------------------------------------------------------------------------- |
| Comic book archives | `.cbz`, `.cbr`, `.cbt`, `.cb7` (and `.ora`)                                                                          |
| Archives of images  | `.zip`, `.rar`, `.7z`, `.tar` that contain images                                                                    |
| Image folder        | open a directory of images (each file is a page)                                                                     |
| Single image        | PNG, JPEG, WebP, AVIF, HEIC, GIF (including animation), TIFF, and [other image types](Supported-document-formats.md) |

- Encrypted `.cbz` / `.cbr` files are supported (password prompt when needed).
- An **image folder** treats each image as a page, ordered by file name. The Bookmarks sidebar lists the files. The same `ImageUI` settings apply (`DefaultZoom`, `LimitToWindowWidth` / `Height`, `LandscapeAsSpread`, `WindowBgCol`).
- Archives opened from a **network drive** that are 32 MB or smaller are loaded into memory; larger ones may be copied into a local cache (`cbx-cache` under the [settings data directory](How-we-store-settings.md)) so page turns stay fast. Clear that cache with `Ctrl + K`, `Delete Cached Files` in [Command Palette](Command-Palette.md) (`CmdDeleteCachedFiles`).

## Navigate chapters

The Bookmarks sidebar lists pages in the archive.

- If the archive has a `ComicInfo.xml` with bookmarks, those names are used.
- Otherwise, if images live in **chapter folders**, the folders appear as nested outline entries (click a folder to jump to its first page). A directory shared by every file is omitted, so a comic whose files are all in one folder stays a flat list of file names.

## Choose a page layout

Use the same page layout commands as for PDF (**View** menu):

| Action                               | Shortcut / command               |
| ------------------------------------ | -------------------------------- |
| Single page                          | `Ctrl + 6` (`CmdSinglePageView`) |
| Facing (two pages)                   | `Ctrl + 7` (`CmdFacingView`)     |
| Book view (facing, first page alone) | `Ctrl + 8` (`CmdBookView`)       |
| Continuous scroll                    | `c` (`CmdToggleContinuousView`)  |

Facing or book view is useful for double-page spreads (see [Show landscape pages as spreads](#show-landscape-pages-as-spreads-ver-37)). Continuous mode is natural for long webtoon-style strips. See [Scrolling and zooming](Scrolling-and-zooming.md).

To turn the page by clicking the left or right edge of the window (like many comic readers), set `ClickEdgeToTurnPage = true` in [advanced settings](Advanced-options-settings.md). In manga mode the sides are reversed so a click on the left still advances.

## Read manga right-to-left

**Manga mode** displays pages right-to-left in facing and book views (typical for Japanese manga and right-to-left documents).

- **View → Manga Mode**
- `Ctrl + K`, `Toggle Manga Mode` in [Command Palette](Command-Palette.md) (`CmdToggleMangaMode`)
- Available for PDF, XPS, DjVu, ebooks, comic books, images and other fixed-page documents
- When manga mode is on, **Left** advances and **Right** goes back (and horizontal swipe matches that), so navigation follows right-to-left reading

### Make manga mode the default for new comics

In [advanced settings](Advanced-options-settings.md):

```
ComicBookUI [
    CbxMangaMode = true
]
```

That sets the **default** for comic books you have not opened before. Per-file state is stored as `DisplayR2L` under `FileStates` when [remembering state per document](How-we-store-settings.md).

Command-line `-manga-mode true|false` still works but is deprecated in favor of `ComicBookUI.CbxMangaMode`.

## Fit double-page spreads and single pages

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

### Show landscape pages as spreads (ver 3.7+)

In **facing** and **book view**, a page wider than it is tall is treated as a double-page spread: it takes the full row and is not paired with the next page. Comics that store a centerfold as one image show it that way. Book view still keeps the cover (page 1) alone.

```
ComicBookUI [
    LandscapeAsSpread = true
]

ImageUI [
    LandscapeAsSpread = true
]
```

Default is **true**. Set it to `false` (under `ComicBookUI`, or `ImageUI` for image folders) to keep the old pairing: every page occupies one slot of the two-page row, even if it is landscape (issue #872).

### Pick a zoom mode

| Mode                       | When it helps                                                                                                                                 |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| **Fit Width** (`Ctrl + 2`) | Every page fills the window width (spreads and singles both edge-to-edge; singles can become very large)                                      |
| **Fit Page** (`Ctrl + 0`)  | Whole page(s) visible                                                                                                                         |
| **Shrink to Fit**          | Never enlarge past 100%; only shrink pages that are larger than the window ([like IrfanView “fit only big images”](Scrolling-and-zooming.md)) |
| **Fit by Orientation**     | Fit width in landscape viewports, fit page in portrait                                                                                        |
| **Fit Height**             | Page height fills the window (handy for landscape pages)                                                                                      |

### Set the default zoom

- Single image files: `ImageUI.DefaultZoom` (default `shrink to fit`).
- Comic archives: `ComicBookUI.DefaultZoom` on first open (empty uses Fit Page, not the global `DefaultZoom`). A remembered zoom for that file still wins.

Set `ComicBookUI.DefaultZoom = fit width` to open new comics at Fit Width while PDFs stay at Fit Page.

## Change the background color

Comics and images use a **black** canvas by default (unlike PDF’s white). Override it:

```
ComicBookUI [
    WindowBgCol = #1a1a1a
]

ImageUI [
    WindowBgCol = #1a1a1a
]
```

- Values accept normal colors or `checkered` for a transparency checkerboard.
- Or `Ctrl + K`, `Change Background Color` in [Command Palette](Command-Palette.md) (`CmdChangeBackgroundColor`).

UI theme colors are separate — see [Customize theme colors](Customize-theme-colors.md).

## Adjust margins and page spacing

```
ComicBookUI [
    WindowMargin = 0 0 0 0
    PageSpacing = 4 4
]
```

- `WindowMargin` — top, right, bottom, left gap between the window and the document (default all zeros for comics)
- `PageSpacing` — horizontal and vertical gap between pages (between columns in facing / book view, between rows in continuous view)

Sizes are in pixels at 100% display scaling and are DPI-scaled.

## Show page info

Press `i` (`CmdTogglePageInfo`) for the page-info tip.

- Comics and image folders: the current image **file name** and size (both pages when two are visible in facing view).
- A single open image: pixel resolution, file size, and DPI when not the default 96.

Document properties (`Ctrl + D`) for comic archives list image files and, where present, EXIF and ComicInfo / ComicBookInfo metadata.

## Convert to PDF

Turn a comic, image folder, or single image into a multi-page PDF (`CmdConvertToPDF`):

- **File → Convert to PDF…**
- Right-click → **Document → Convert to PDF…**

**Convert page to PDF** on the image context menu (`CmdConvertImageToPdf`) saves only the **current page** via the image editor.

Full details: [Convert to PDF](Convert-to-PDF.md).

## Copy a page image

Right-click a comic or image page and choose **Copy To Clipboard** (`CmdCopyImage`, `Copy Image` in [Command Palette](Command-Palette.md)) to copy the page image to the clipboard.

## Tips

- Turn on `LimitToWindowWidth` so spreads fit the window while single pages keep your zoom.
- Use book view (`Ctrl + 8`) to keep the cover alone and pair the following pages.
- Set `ClickEdgeToTurnPage = true` to turn pages by clicking the window edges.
- Use continuous view (`c`) for webtoon-style strips.
- Set `ComicBookUI.DefaultZoom = fit width` to open comics differently from PDFs.

## What is not available for comics / images

These PDF-oriented features do not apply to comic archives, image folders, or plain images:

- In-document text search and text selection (there is no extractable text)
- **Read Aloud** (menu, context menu, toolbar) — no text to speak
- Keyboard link following (`Shift + F`)
- [AI Chat with document](AI-Chat-with-document.md) (PDF only)

## Settings reference

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
    LandscapeAsSpread = true
]
```

### `ImageUI`

```
ImageUI [
    WindowBgCol =
    DefaultZoom = shrink to fit
    LimitToWindowWidth = false
    LimitToWindowHeight = false
    LandscapeAsSpread = true
]
```

Full field comments: [Advanced settings](Advanced-options-settings.md).

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

- [Convert to PDF](Convert-to-PDF.md) — full conversion details
- [Supported document formats](Supported-document-formats.md) — every image and archive type
- [Scrolling and zooming](Scrolling-and-zooming.md) — zoom modes and layouts
- [Advanced settings](Advanced-options-settings.md) — all `ComicBookUI` / `ImageUI` fields
- [Commands](Commands.md) — command ids for rebinding
- [How we store settings](How-we-store-settings.md) — per-file state and data directory
