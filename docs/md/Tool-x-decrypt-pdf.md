# Decrypt a PDF

> Use `sumatrapdf-tool.exe` or [SumatraPDF.exe](Tools.md) with the same arguments.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In the application

To decrypt a PDF in SumatraPDF:

- open an encrypted PDF document
- press `Ctrl + K` to open the [command palette](Command-Palette.md)
- select `Decrypt PDF`

Or:

- open an encrypted PDF document
- right-click to open the context menu
- select `Document` > `Decrypt PDF`

## From the command line

To decrypt a PDF using SumatraPDF from the command line:

`sumatrapdf-tool clean -D -p pwd foo-encrypted.pdf foo-decrypted.pdf`

Flags:

- `-D` : decrypt
- `-p <pwd>` : provide password

You can see [all flags](Tool-clean.md).

You can also [encrypt a PDF](Tool-x-encrypt-pdf-with-password.md).
