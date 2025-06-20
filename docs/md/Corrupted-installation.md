# Corrupted installation

If you're reading this, you might have been told by SumatraPDF that you have a corrupted installation.

**How can that happen?**

SumatraPDF comes in two flavors: a portable version and an installer.

A portable version is a self-contained executable and cannot be corrupted.

An installer needs to be run to be properly installed. Part of it is extracting `libmupdf.dll` library.

There are 2 possible problem:

- `libmupdf.dll` is missing
- `libmupdf.dll` is there but its version doesn't match the version of `SumatraPDF.exe`

**How to solve the problem?**

If you want to place SumatraPDF in any location, under any name, use the self-contained portable flavor.

If you insist on using the installable version, just install it. The installer will run if it has `-install` in the name of the .exe (which it will if you download [official build](https://www.sumatrapdfreader.org/download-free-pdf-viewer)).

If you rename the `.exe`, you can force running the installer with `-install` option.

Alternatively, you can extract `libmupdf.dll` and all other files with `-x` option.