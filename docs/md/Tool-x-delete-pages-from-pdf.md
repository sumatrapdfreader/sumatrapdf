# Delete pages from PDF

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To delete pages from a PDF in SumatraPDF:

- open a PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Delete Pages From PDF`

Or:

- open a PDF document
- right-click to open the context menu
- select `Document` > `Delete Pages From PDF`

## From the command line

To delete a page from a PDF using SumatraPDF:

`sumatrapdf-tool clean input.pdf output.pdf 1-3,5-N`

This deletes page 4 from `input.pdf` and creates `output.pdf`.

You can delete any number of pages by providing the range of pages to retain.

`N` is a special character meaning "last page".

For all options, see [sumatrapdf-tool clean](Tool-clean.md).
