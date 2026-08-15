# Customize eBook UI

EPUB, MOBI, FB2, and similar formats use SumatraPDF's **eBook UI** (HTML-based layout). Since **version 3.4** the engine is MuPDF-based: text selection, in-document search, and bookmarks work much like PDF.

## The eBook Settings dialog (**ver 3.7+**)

With an ebook open, run **Change eBook Settings** (`CmdChangeEbookSettings`) from
the [command palette](Command-Palette.md) (`Ctrl + K`). It has the font, size,
margin and line spacing, and shows the CSS those values produce in a read-only
box, so you can see exactly what is applied. **Margin** is a single field, but it
takes the same one, two or four values as the setting. Tick **Custom CSS** and the box becomes
editable: what you type there is then used instead of the generated rules (it
starts out holding them, so nothing is lost by taking over).

At the bottom, **This file** stores the settings for the open document only and
**For all ebooks** stores them globally, the same way **Change Background Color**
works. In per-document mode only the values that differ from the global ones are
stored, so everything else keeps following the global setting. The document is
reloaded when you press OK.

**Reset to defaults** undoes the customization at whichever of the two is
selected: for one document that means inheriting everything from the global
section again, for all ebooks it means the values SumatraPDF ships with. `Tab`
moves between the controls; `Esc` does *not* close the dialog, so a long piece
of CSS can't be lost by a stray keypress — use **Cancel**.

The command is only offered for reflowable documents — for a PDF or an image
there is no font or CSS to change.

## What you can customize

The same settings, and a few more, are in **Settings → Advanced Options...**,
in the `EBookUI` section:

```
EBookUI [
    FontName =
    FontSize = 0
    Margin =
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
| `FontName`              | Default font family, e.g. `Segoe UI` or `Microsoft YaHei` (empty = engine default, usually a serif). Overrides the document's `font-family`, including inline `style="font-family: ..."`. A font that can't be loaded is reported with a notification (**ver 3.7+**) |
| `FontSize`              | Base font size (default `8.0`; `0` = built-in default)                                                                                                     |
| `Margin`                | White space around the text, in points, written like a CSS margin: one number for all four sides, two for top/bottom and left/right, or four in top-right-bottom-left order. Empty keeps the default (3 em above and below, 2 em to the sides, so it follows the font size); `0` leaves no margin at all (**ver 3.7+**) |
| `LineSpacing`           | Line-height multiplier, e.g. `1.5` for expanded spacing (`0` = document or engine default; **ver 3.7+**)                                                   |
| `LayoutDx` / `LayoutDy` | Virtual page width / height for reflow (defaults `420` / `595`)                                                                                            |
| `IgnoreDocumentCSS`     | Ignore stylesheet from the EPUB (`true` = your `CustomCSS` wins)                                                                                           |
| `CustomCSS`             | Extra CSS rules. A declaration marked `!important` beats the document's own CSS and its inline styles, so `IgnoreDocumentCSS` is rarely needed              |
| `WindowBgCol`           | Canvas background around the reflowed text (**ver 3.7+**)                                                                                                  |

Full field reference: [Advanced options / settings](Advanced-options-settings.md).

## Settings for a single document (**ver 3.7+**)

The same options can be set for one document only, as an `EBookUI` block inside
that file's entry in `FileStates`:

```
FileStates [
    [
        FilePath = C:\books\war-and-peace.epub
        EBookUI [
            FontName = Microsoft YaHei
            LineSpacing = 1.4
        ]
    ]
]
```

A field you leave out (empty, or `0` for the numbers) uses the value from the
global `EBookUI` section, so the block above changes only the font and the line
spacing for that one book. `Margin` is empty when unset, since `0` means no
margin at all. `IgnoreDocumentCSS` is `true` / `false` here, and
empty means "use the global setting" — so a single document can keep the
publisher's CSS even when the global setting ignores it.

The block is written to the settings file only for documents that have one, and
it survives `RememberOpenedFiles = false` (it's a setting you typed, not reading
history). The document has to be reopened for a change to take effect.

`WindowBgCol` and `DefaultDisplayMode` have no per-document version here: the
`BgCol` and `DisplayMode` fields of the file entry already do that job.

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
