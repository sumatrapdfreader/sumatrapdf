# Convert to PDF

**Convert to PDF** turns a comic book, folder of images, or single image into a multi-page PDF, from the **File** menu or the right-click menu. Each image becomes one page.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Open a `.cbz` and use File → Convert to PDF… to get one PDF with every page.** At a glance:

- **Convert to PDF…** (`CmdConvertToPDF`): all pages → one multi-page PDF.
- **Convert page to PDF** (`CmdConvertImageToPdf`): the current page only, via the image editor.
- **Command line:** `sumatrapdf-tool convert` without the GUI dialog.

This restores the pre–3.5 multi-page image → PDF workflow (issues [#4118](https://github.com/sumatrapdfreader/sumatrapdf/issues/4118), [#5532](https://github.com/sumatrapdfreader/sumatrapdf/issues/5532)).

## Convert a comic or image folder

1. Open a comic archive, image folder, or image (see [what can be converted](#what-can-be-converted)).
2. Start **Convert to PDF…** in any of these ways:
   - **File → Convert to PDF…**
   - Right-click the document → **Document → Convert to PDF…**
   - Right-click an image → **Convert to PDF...**
   - `Ctrl + K`, `Convert To PDF` in [Command Palette](Command-Palette.md) (`CmdConvertToPDF`, see [Commands](Commands.md))
3. A dialog shows the source path and a suggested destination:
   - Default path is the same location and base name with a `.pdf` extension.
   - If that file already exists, the name is made unique (e.g. `book.1.pdf`).
   - Edit the path in the text field, or use **…** to browse.
4. Click **Convert to PDF**. On success the new PDF is written and opened in SumatraPDF. On failure you get a short error dialog.

Press **Esc** or **Cancel** to close the dialog without converting.

## Convert only the current page

| Command                                          | Menu / context                                    | What it does                                                             |
| ------------------------------------------------ | ------------------------------------------------- | ------------------------------------------------------------------------ |
| **Convert to PDF…** (`CmdConvertToPDF`)          | File menu; Document context menu; command palette | All pages of the comic / folder / image → one multi-page PDF (this page) |
| **Convert page to PDF** (`CmdConvertImageToPdf`) | Image context menu                                | Current page only, via the image editor / save dialog                    |

For a single open image file, **Convert to PDF…** and converting that one page produce a one-page PDF; prefer **Convert to PDF…** when you want the simple path-picker dialog without opening the image editor.

## Convert from the command line

To convert images or other documents to PDF without the GUI dialog, use `sumatrapdf-tool.exe convert` or [SumatraPDF.exe convert](Tools.md):

```
sumatrapdf-tool convert -o output.pdf input.png
sumatrapdf-tool convert -o book.pdf comic.cbz
```

More options: [sumatrapdf-tool convert](Tool-convert.md), [Convert PNG to PDF](Tool-x-convert-png-to-pdf.md).

Note: the GUI command and `sumatrapdf-tool convert` are separate code paths; the dialog’s JPEG/PNG embed and optimized-PNG fallback are specific to **Convert to PDF…** in the app.

## Tips

- Use JPEG or PNG sources for the smallest PDF; they are embedded as-is.
- Use **Convert to PDF…** rather than **Convert page to PDF** for a single image, to skip the image editor.
- Use [Compress a PDF](Tool-x-compress-pdf.md) for documents that are already PDF.

## What can be converted

The command appears only when the open document is an **image collection**:

| Source              | Examples                                                                                                |
| ------------------- | ------------------------------------------------------------------------------------------------------- |
| Comic book archives | `.cbz`, `.cbr`, `.cbt`, `.cb7`, `.ora`                                                                  |
| Archives of images  | `.zip`, `.rar`, `.7z`, `.tar` that contain images                                                       |
| Image folder        | a directory of images opened as a multi-page document                                                   |
| Single image        | PNG, JPEG, WebP, JPEG XL, HEIC, AVIF, TIFF, GIF, and [other image types](Supported-document-formats.md) |

It is **hidden** for PDF, EPUB, DjVu, CHM, and other non-image documents. See [Comics and manga](Comics-and-manga.md) for how to open comics and image folders.

## How images are stored in the PDF

SumatraPDF prefers to **embed the original image bytes** when PDF can use them directly:

| Format                                          | Behavior                                                                        |
| ----------------------------------------------- | ------------------------------------------------------------------------------- |
| **JPEG**, **PNG**                               | Embedded as-is (usually the most compact)                                       |
| **Animated GIF, multi-page TIFF, ICO**          | Each frame is rendered as its own page (the file is not per-page bytes)         |
| **WebP, JPEG XL, HEIC, AVIF, TGA**, and similar | Decoded, encoded as PNG, then losslessly recompressed (zopfli) before embedding |
| If encode/embed fails                           | Page is **rendered** into the PDF as a last resort                              |
| If every path fails for a page                  | That page is **skipped** (conversion continues with the rest)                   |

The PDF’s producer metadata is set to the current SumatraPDF version (e.g. `SumatraPDF 3.7`).

There is no quality / DPI slider in the dialog: JPEG and PNG keep their original data; other formats go through a lossless PNG path when possible, so the main size trade-off is whether the source was already JPEG/PNG versus a format that must be re-encoded.

## See also

- [Comics and manga](Comics-and-manga.md) — opening and reading comics
- [Supported document formats](Supported-document-formats.md)
- [Commands](Commands.md)
- [Command palette](Command-Palette.md)
- [Compress a PDF](Tool-x-compress-pdf.md) (for existing PDFs)
- [All cmd-line tools](Tools.md)
