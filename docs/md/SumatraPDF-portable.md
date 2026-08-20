# Portable vs installer

Portable version is a self-contained executable that does not require installation. You can copy it to any folder / new computer, put on an USB drive and it'll just work.

Installer version also includes:

- `PdfPreview.dll` : provides OS level (e.g. in Explorer) preview / thumbnails for supported files formats
- `PdfFilter.dll` : provides OS level search within supported files (e.g. PDF, ePub files etc.)
- `libsumatrapdf.dll` : for technical reasons, to make `PdfPreview.dll` and `PdfFilter.dll` work, the application must be split into `libsumatrapdf.dll` (core functionality) and the app `SumatraPDF.exe`
- `sumatrapdf-tool.exe` : cmd-line tool to [perform operations](Tools.md) on PDF files. You can also run the same commands as `SumatraPDF.exe <tool>` without extracting this file.

You can extract those files with `-x` cmd-line option. Use `-d <dir>` to extract to given directory.

Before **3.7**, the dll was called `libmupdf.dll`.

Before **3.7**, portable was a separate executable that contained the engine code.

In **3.7** I simplified things. Portable version is exactly the same executable as the installer.

You can install it with `-install` cmd-line option or run it as portable executable.

When exe has `-install` in the name, we assume it's intented to be run as installer.

To make this dual-purpose portable/installer executable work, due to Windows technical limitations, we have to extract `libsumatrapdf.dll` to disk. The location is an implementation detail: `%LOCALAPPDATA%\SumatraPDF-data\<build-id>\libsumatrapdf.dll`. If you prefer, you can place it next to `SumatraPDF.exe` executable.

Data storage (`SumatraPDF-settings.txt` etc.):

- in portable mode application data is stored next to `SumatraPDF.exe`.
- when installed, in `%LOCALAPPDATA%\SumatraPDF`.
- you can over-ride the location with `-appdata <dir>` [cmd-line option](Command-line-arguments.md)

See also:

- [Installation](Installation.md)
- [Installer cmd-line arguments](Installer-cmd-line-arguments.md)
- [How we store settings](How-we-store-settings.md)
- [Corrupted installation](Corrupted-installation.md)
- [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md)
