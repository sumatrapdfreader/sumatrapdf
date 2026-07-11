# Customize eBook UI

EPUB, MOBI, FB2, and similar formats use SumatraPDF's **eBook UI** (HTML-based layout). Since **version 3.4** the engine is MuPDF-based: text selection, in-document search, and bookmarks work much like PDF.

## What you can customize

Open **Settings → Advanced Options...** and edit the `EBookUI` section:

```
EBookUI [
    FontSize = 0
    LayoutDx = 0
    LayoutDy = 0
    IgnoreDocumentCSS = false
    CustomCSS =
    WindowBgCol =
]
```

| Setting | Meaning |
| --- | --- |
| `FontSize` | Base font size (default `8.0`; `0` = built-in default) |
| `LayoutDx` / `LayoutDy` | Virtual page width / height for reflow (defaults `420` / `595`) |
| `IgnoreDocumentCSS` | Ignore stylesheet from the EPUB (`true` = your `CustomCSS` wins) |
| `CustomCSS` | Extra CSS rules — often paired with `IgnoreDocumentCSS = true` |
| `WindowBgCol` | Canvas background around the reflowed text (**ver 3.7+**) |

Full field reference: [Advanced options / settings](Advanced-options-settings.md).

## Themes and invert colors

UI themes (`Theme = ...` in advanced settings) change window chrome. To change how the **document** looks:

- **`Shift + I`** (`CmdInvertColors`) toggles the `DocumentColorsFollowTheme` advanced setting between `off` and `smart` for MuPDF-rendered documents (PDF-style and ebook pages through the shared color path). `smart` recolors text and background but preserves images; `legacy` recolors images too (pre-3.7 behavior).

For dark reading without crushing photos, experiment with `CustomCSS` (dark background, light text) and `IgnoreDocumentCSS = true` instead of global invert. See [Customize theme colors](Customize-theme-colors.md).

## Continuous vs paged view

Use the same view commands as PDF: `c` toggles continuous mode; `Ctrl + 6` / `7` / `8` change column layout. See [Scrolling and zooming](Scrolling-and-zooming.md).

## Search and navigation

- In-document find: `Ctrl + F`, `F3` — see [Finding text](Finding-text.md)
- Table of contents: `F12` or command palette `*` prefix
- Internal links: click to follow; `Alt + Left` to go back

## Known limitations

Report issues in [discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions) or the [issue tracker](https://github.com/sumatrapdfreader/sumatrapdf/issues).

- **Vertical writing** (some Asian EPUBs): may not detect top-to-bottom / right-to-left layout correctly.
- **CSS features**: not every EPUB3 layout feature is supported; complex fixed-layout EPUBs may look wrong.
- **`ReloadModifiedDocuments`**: auto-reload when the file changes on disk does not apply to eBook UI documents.
- **Thumbnails in Explorer** for `.epub`: limited compared to PDF — see installer `-with-preview` in [Installation](Installation.md).

## CHM files

CHM uses a separate `ChmUI` section. Set `ChmUI.UseFixedPageUI = true` to render CHM with the PDF-style fixed-page engine instead of the HTML ebook engine.

## See also

- [Supported document formats](Supported-document-formats.md)
- [FAQ](FAQ.md) — dark mode / invert questions
- [Advanced options / settings](Advanced-options-settings.md)