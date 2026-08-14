# Customize eBook UI

EPUB, MOBI, FB2, and similar formats use SumatraPDF's **eBook UI** (HTML-based layout). Since **version 3.4** the engine is MuPDF-based: text selection, in-document search, and bookmarks work much like PDF.

## What you can customize

Open **Settings → Advanced Options...** and edit the `EBookUI` section:

```
EBookUI [
    FontName =
    FontSize = 0
    LineSpacing = 0
    LayoutDx = 0
    LayoutDy = 0
    IgnoreDocumentCSS = false
    CustomCSS =
    WindowBgCol =
]
```

| Setting                 | Meaning                                                                                                                                                    |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `FontName`              | Default font family, e.g. `Segoe UI` or `Microsoft YaHei` (empty = engine default, usually a serif). Overrides the document's `font-family` (**ver 3.7+**) |
| `FontSize`              | Base font size (default `8.0`; `0` = built-in default)                                                                                                     |
| `LineSpacing`           | Line-height multiplier, e.g. `1.5` for expanded spacing (`0` = document or engine default; **ver 3.7+**)                                                   |
| `LayoutDx` / `LayoutDy` | Virtual page width / height for reflow (defaults `420` / `595`)                                                                                            |
| `IgnoreDocumentCSS`     | Ignore stylesheet from the EPUB (`true` = your `CustomCSS` wins)                                                                                           |
| `CustomCSS`             | Extra CSS rules — often paired with `IgnoreDocumentCSS = true`                                                                                             |
| `WindowBgCol`           | Canvas background around the reflowed text (**ver 3.7+**)                                                                                                  |

Full field reference: [Advanced options / settings](Advanced-options-settings.md).

## Themes and document page colors

UI themes (`Theme = ...`) only change window chrome. Ebooks that go through MuPDF’s fixed-page color path use the same **`DocumentColorsFollowTheme`** setting as PDF:

| Value        | Effect                                                           |
| ------------ | ---------------------------------------------------------------- |
| **`off`**    | Original page colors.                                            |
| **`smart`**  | Dark (or custom) text and background; **images stay intact**.    |
| **`legacy`** | Recolor text, background, **and** images (pre-3.7 invert-style). |

- **`Shift + I`** toggles **`off` ↔ `smart`**.
- For **`legacy`**, use Settings → Theme… / Make Document Colors Follow Theme, or set `DocumentColorsFollowTheme = legacy` in advanced settings.

For dark reading of reflowed EPUBs without recoloring the whole page, you can also use `EBookUI.CustomCSS` + `IgnoreDocumentCSS = true` and `WindowBgCol`. See [Customize theme colors](Customize-theme-colors.md).

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

CHM uses a separate `ChmUI` section. Set `ChmUI.UseFixedPageUI = true` to render CHM with the PDF-style fixed-page engine instead of the interactive browser view. In that mode, set `ChmUI.FontName` to override the document font; when empty it falls back to `EBookUI.FontName` and then the engine default.

## See also

- [Supported document formats](Supported-document-formats.md)
- [FAQ](FAQ.md) — dark mode / invert questions
- [Advanced options / settings](Advanced-options-settings.md)
