# Delete pages from PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In application

To delete pages from a PDF in SumatraPDF:
- open PDF document
- `Ctrl + k` for [command palette](Command-Palette.md)
- `Delete Pages From PDF`

Or:
- open PDF document
- right-click for context menu
- `Document` > `Delete Pages From PDF`

## From command-line

To delete a page from a PDF using SumatraPDF:

`SumatraPDF clean input.pdf output.pdf 1-3,5-N`

This deletes page 4 from `input.pdf` and creates `output.pdf`

You can delete any number of pages by providing the range of pages to retain.

`N` is special character meaning: "last page".

For all options see [SumatraPDF clean](Tool-clean.md).
