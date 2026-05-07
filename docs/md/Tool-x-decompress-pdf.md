# Decompress a PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In application

To decompress a PDF in SumatraPDF:
- open PDF document
- `Ctrl + k` for [command palette](Command-Palette.md)
- `Deompress PDF`

Or:
- open PDF document
- right-click for context menu
- `Document` > `Decompress PDF`

## From command-line

To decompress a PDF using SumatraPDF from command-line:

`SumatraPDF clean -d foo.pdf foo-decompressed.pdf`

For explanation of all flags see [SumatraPDF clean](Tool-clean.md).

You might want to de-compress to inspect the structure of PDF file more easily.

If a PDF file is compressed, the decompressed version should be bigger.

If a PDF is already decompressed, this will have little effect.

You can also [compress](Tool-x-compress-pdf.md) PDF.
