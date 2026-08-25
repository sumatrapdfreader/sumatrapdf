# Compress a PDF

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To compress a PDF in SumatraPDF:

- open a PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Compress PDF`

Or:

- open a PDF document
- right-click to open the context menu
- select `Document` > `Compress PDF`

## From the command line

To compress a PDF using SumatraPDF from the command line:

`sumatrapdf-tool clean -gggg -e 100 -f -i -t -Z foo.pdf foo-compressed.pdf`

This uses the most aggressive compression flags.

For an explanation of all flags, see [sumatrapdf-tool clean](Tool-clean.md).

If a PDF file is uncompressed, the compressed version should be smaller.

If a PDF is already compressed, this will have little effect.

You can also [decompress a PDF](Tool-x-decompress-pdf.md).
