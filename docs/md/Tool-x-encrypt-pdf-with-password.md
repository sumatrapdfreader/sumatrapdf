# Encrypt a PDF with password

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To encrypt a PDF in SumatraPDF:

- open an unencrypted PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Encrypt PDF`

Or:

- open an unencrypted PDF document
- right-click to open the context menu
- select `Document` > `Encrypt PDF`

## From the command line

To encrypt a PDF with a password using SumatraPDF from the command line:

`sumatrapdf-tool clean -E aes-256 -U pwd foo.pdf foo-encrypted.pdf`

Flags:

- `-E` : encryption algorithm
- `-U <pwd>` : password required to open PDF file

You can see [all flags](Tool-clean.md).

You can also [decrypt a PDF](Tool-x-decrypt-pdf.md).
