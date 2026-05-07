# Decrypt a PDF

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## In application

To decrypt a PDF in SumatraPDF:
- open encrypted PDF document
- `Ctrl + k` for [command palette](Command-Palette.md)
- `Decrypt PDF`

Or:
- open encrypted PDF document
- right-click for context menu
- `Document` > `Decrypt PDF`

## From command-line

To decrypt a PDF using SumatraPDF from command-line:

`SumatraPDF clean -D -p pwd foo-encrypted.pdf foo-decrypted.pdf`

Flags:

- `-D` : decrypt
- `-p <pwd>` : provide password

You can see [all flags](Tool-clean.md).

You can also [encrypt](Tool-x-encrypt-pdf-with-password.md)
