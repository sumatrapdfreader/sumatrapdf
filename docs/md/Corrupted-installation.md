# Corrupted installation

See also [Installation](Installation.md) for an overview of the portable and installer versions.

If you're reading this, you might have been told by SumatraPDF that you have a corrupted installation.

**How can that happen?**

SumatraPDF comes in two flavors: a portable version and an installer.

A portable version is a self-contained executable and cannot be corrupted.

An installer needs to be run to be properly installed. Part of it is extracting the engine library (`libsumatrapdf.dll` from **3.7**; through **3.6** it was named `libmupdf.dll`).

There are two possible problems:

- `libsumatrapdf.dll` is missing
- `libsumatrapdf.dll` is there but its version doesn't match the version of `SumatraPDF.exe`

**How to solve the problem?**

If you want to place SumatraPDF in any location, under any name, use the self-contained portable flavor.

If you want to use the installable version, install it. The installer will run if the `.exe` filename contains `-install` (as it will if you download an [official build](https://www.sumatrapdfreader.org/download-free-pdf-viewer)).

If you rename the `.exe`, you can force it to run as the installer with the `-install` option.

Alternatively, you can extract `libsumatrapdf.dll` and all other files with the `-x` option.
