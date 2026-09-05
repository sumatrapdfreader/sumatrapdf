# Convert to PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Convert to PDF** turns a comic book, folder of images, or single image into a multi-page PDF. Each image becomes one page. This restores the pre–3.5 multi-page image → PDF workflow (issues [#4118](https://github.com/sumatrapdfreader/sumatrapdf/issues/4118), [#5532](https://github.com/sumatrapdfreader/sumatrapdf/issues/5532)).

Command id: `CmdConvertToPDF` (see [Commands](Commands.md)).

## When it is available

The command appears only when the open document is an **image collection**:

| Source              | Examples                                                                                                |
| ------------------- | ------------------------------------------------------------------------------------------------------- |
| Comic book archives | `.cbz`, `.cbr`, `.cbt`, `.cb7`, `.ora`                                                                  |
| Archives of images  | `.zip`, `.rar`, `.7z`, `.tar` that contain images                                                       |
| Image folder        | a directory of images opened as a multi-page document                                                   |
| Single image        | PNG, JPEG, WebP, JPEG XL, HEIC, AVIF, TIFF, GIF, and [other image types](Supported-document-formats.md) |

It is **hidden** for PDF, EPUB, DjVu, CHM, and other non-image documents. See [Comics and manga](Comics-and-manga.md) for how to open comics and image folders.

## In the application

1. Open a comic archive, image folder, or image.
2. Start **Convert to PDF…** in any of these ways:
   - **File → Convert to PDF…**
   - Right-click the document → **Document → Convert to PDF…**
   - [Command palette](Command-Palette.md) (`Ctrl + k`) → type “Convert to PDF”
3. A dialog shows the source path and a suggested destination:
   - Default path is the same location and base name with a `.pdf` extension.
   - If that file already exists, the name is made unique (e.g. `book.1.pdf`).
   - Edit the path in the text field, or use **…** to browse.
4. Click **Convert to PDF**. On success the new PDF is written and opened in SumatraPDF. On failure you get a short error dialog.

Press **Esc** or **Cancel** to close the dialog without converting.

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

## Convert whole document vs one page

| Command                                          | Menu / context                                    | What it does                                                             |
| ------------------------------------------------ | ------------------------------------------------- | ------------------------------------------------------------------------ |
| **Convert to PDF…** (`CmdConvertToPDF`)          | File menu; Document context menu; command palette | All pages of the comic / folder / image → one multi-page PDF (this page) |
| **Convert page to PDF** (`CmdConvertImageToPdf`) | Image context menu                                | Current page only, via the image editor / save dialog                    |

For a single open image file, **Convert to PDF…** and converting that one page produce a one-page PDF; prefer **Convert to PDF…** when you want the simple path-picker dialog without opening the image editor.

## From the command line

To convert images or other documents to PDF without the GUI dialog, use `sumatrapdf-tool.exe convert` or [SumatraPDF.exe convert](Tools.md):

```
sumatrapdf-tool convert -o output.pdf input.png
sumatrapdf-tool convert -o book.pdf comic.cbz
```

More options: [sumatrapdf-tool convert](Tool-convert.md), [Convert PNG to PDF](Tool-x-convert-png-to-pdf.md).

The GUI command and `sumatrapdf-tool convert` are separate code paths; the dialog’s JPEG/PNG embed and optimized-PNG fallback are specific to **Convert to PDF…** in the app.

## Related

- [Comics and manga](Comics-and-manga.md)
- [Supported document formats](Supported-document-formats.md)
- [Commands](Commands.md)
- [Command palette](Command-Palette.md)
- [Compress a PDF](Tool-x-compress-pdf.md) (for existing PDFs)
- [All cmd-line tools](Tools.md)
