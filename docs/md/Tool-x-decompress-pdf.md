# Decompress a PDF

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To decompress a PDF in SumatraPDF:

- open a PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Decompress PDF`

Or:

- open a PDF document
- right-click to open the context menu
- select `Document` > `Decompress PDF`

## From the command line

To decompress a PDF using SumatraPDF from the command line:

`sumatrapdf-tool clean -d foo.pdf foo-decompressed.pdf`

For an explanation of all flags, see [sumatrapdf-tool clean](Tool-clean.md).

You might want to decompress a PDF to inspect its structure more easily.

If a PDF file is compressed, the decompressed version should be bigger.

If a PDF is already decompressed, this will have little effect.

You can also [compress a PDF](Tool-x-compress-pdf.md).
