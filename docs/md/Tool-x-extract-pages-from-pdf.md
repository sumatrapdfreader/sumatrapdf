# Extract pages from PDF

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To extract pages from a PDF in SumatraPDF:

- open a PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Extract Pages From PDF`

Or:

- open a PDF document
- right-click to open the context menu
- select `Document` > `Extract Pages From PDF`

Enter the page range to consider. Select **Only with annotations** to extract
only annotated pages within that range; use `1-N` to consider the whole PDF.

## From the command line

To extract pages from a PDF using SumatraPDF on a command line:

`sumatrapdf-tool clean input.pdf output.pdf 1,2-5,8-N`

This extracts pages 1, 2, 3, 4, 5, and 8 through the last page from `input.pdf`, and saves them as `output.pdf`.

`N` is a special character meaning "last page".

## Delete page or pages from PDF

To delete a page, extract all pages except the one you want to delete. See [delete pages from PDF](Tool-x-delete-pages-from-pdf.md).
