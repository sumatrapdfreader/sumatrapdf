# Extract pages from PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In application

To extract pages from a PDF in SumatraPDF:
- open PDF document
- `Ctrl + k` for [command palette](Command-Palette.md)
- `Extract Pages From PDF`

Or:
- open PDF document
- right-click for context menu
- `Document` > `Extract Pages From PDF`

## From command-line

To extract pages from a PDF using SumatraPDF on a command line:

`SumatraPDF clean input.pdf output.pdf 1,2-5,8-N`

This extracts pages 1, 2, 3, 4, 5 and 8 to last from `input.pdf` and saves as `output.pdf`

`N` is special character meaning "last page".

## Delete page or pages from PDF

To delete a page you need to extract all pages except the one you want to delete. See [delete pages from PDF](Tool-x-delete-pages-from-pdf.md).
