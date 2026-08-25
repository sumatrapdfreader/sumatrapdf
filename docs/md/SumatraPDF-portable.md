# Portable vs installer

The portable version is a self-contained executable that does not require installation. You can copy it to any folder or new computer, put it on a USB drive, and it will just work.

The installer version also includes:

- `PdfPreview.dll` : provides OS-level previews and thumbnails (e.g. in Explorer) for supported file formats
- `PdfFilter.dll` : provides OS-level search within supported files (e.g. PDF and EPUB files)
- `libsumatrapdf.dll` : for technical reasons, the application must be split into `libsumatrapdf.dll` (core functionality) and `SumatraPDF.exe` to make `PdfPreview.dll` and `PdfFilter.dll` work
- `sumatrapdf-tool.exe` : command-line tool for [performing operations](Tools.md) on PDF files. You can also run the same commands as `SumatraPDF.exe <tool>` without extracting this file

You can extract these files with the `-x` command-line option. Use `-d <dir>` to extract them to a given directory.

Before **3.7**, the DLL was called `libmupdf.dll`.

Before **3.7**, the portable version was a separate executable that contained the engine code.

In **3.7**, we simplified the distribution: the portable version is exactly the same executable as the installer.

You can install it with the `-install` command-line option or run it as a portable executable.

When the executable has `-install` in its name, we assume it's intended to be run as an installer.

Because of technical limitations in Windows, this dual-purpose portable/installer executable must extract `libsumatrapdf.dll` to disk. The location is an implementation detail: `%LOCALAPPDATA%\SumatraPDF-data\<build-id>\libsumatrapdf.dll`. If you prefer, you can place it next to the `SumatraPDF.exe` executable.

Data storage (`SumatraPDF-settings.txt` etc.):

- in portable mode, application data is stored next to `SumatraPDF.exe`
- when installed, it is stored in `%LOCALAPPDATA%\SumatraPDF`
- you can override the location with the [`-appdata <dir>` command-line option](Command-line-arguments.md)

See also:

- [Installation](Installation.md)
- [Installer cmd-line arguments](Installer-cmd-line-arguments.md)
- [How we store settings](How-we-store-settings.md)
- [Corrupted installation](Corrupted-installation.md)
- [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md)
