# Convert PDF to Images

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Convert PDF to Images** renders each chosen page of the open PDF to an image file (issue [#5991](https://github.com/sumatrapdfreader/sumatrapdf/issues/5991)).

Command id: `CmdConvertPdfToImages` (see [Commands](Commands.md)).

## When it is available

The command appears only when the open document is a **PDF**. It is hidden for comics, images, EPUB, DjVu, CHM, and other non-PDF files. The reverse direction (images → PDF) is [Convert to PDF](Convert-to-PDF.md).

## In the application

1. Open a PDF.
2. Start **Convert PDF to Images…** in any of these ways:
   - **File → Convert PDF to Images…**
   - Right-click the document → **Document → Convert PDF to Images…**
   - [Command palette](Command-Palette.md) (`Ctrl + k`) → type “Convert PDF to Images”
3. A dialog shows the source path and a destination **template**:
   - Default is the same folder and base name as the PDF, with `-<N>.png` (for `book.pdf`, `book-<N>.png`).
   - `<N>` is replaced by the 1-based page number (`book-1.png`, `book-2.png`, …).
   - A **format** drop-down (PNG, JPEG, BMP) next to the path works like Save Image: picking a format updates the file extension.
   - Edit the path, or use **…** to browse. If the chosen name has no `<N>`, `-<N>` is inserted before the extension.
4. Choose which pages to convert:
   - **Current** — the page you are viewing
   - **All** — every page (the default)
   - **Custom** — a range in the edit box, same syntax as Extract Pages: `1,3-5,8-` (`N` is the last page)
5. Click **Convert**. On success, Explorer opens on the first written file. On failure you get a short error dialog.

Press **Esc** or **Cancel** to close the dialog without converting.

Pages are rendered at **150 DPI** (independent of your current zoom level). PNG files are losslessly recompressed in the background (the same Zopfli pass as Save Image).

## From the command line

`sumatrapdf-tool convert` can also write images. Use `%d` in the output name for the page number:

```
sumatrapdf-tool convert -o book-%d.png book.pdf
sumatrapdf-tool convert -o book-%d.png book.pdf 1-3,8
```

## See also

- [Convert to PDF](Convert-to-PDF.md) — comics / images → PDF
- [Extract text from PDF](Tool-x-extract-text-from-pdf.md)
- [Extract pages from PDF](Tool-x-extract-pages-from-pdf.md)
