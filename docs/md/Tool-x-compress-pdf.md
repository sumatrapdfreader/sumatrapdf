# Compress a PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In application

To compress a PDF in SumatraPDF:
- open PDF document
- `Ctrl + k` for [command palette](Command-Palette.md)
- `Compress PDF`

Or:
- open PDF document
- right-click for context menu
- `Document` > `Compress PDF`

## From command-line

To compress a PDF using SumatraPDF from command-line:

`SumatraPDF clean -gggg -e 100 -f -i -t -Z foo.pdf foo-compressed.pdf`

This uses most aggressive compression flags.

For explanation of all flags see [SumatraPDF clean](Tool-clean.md).

If a PDF file is uncompressed, the compressed version should be smaller.

If a PDF is already compressed, this will have little effect.

You can also [decompress](Tool-x-decompress-pdf.md) PDF.
