# Supported document formats

SumatraPDF reader supports the following document types:

- PDF (.pdf, .ai)
- eBook formats:
  - unencrypted EPUB (.epub)
  - MOBI / Kindle (.mobi, and un-encrypted .azw, .azw3, .azw4, .prc)
  - FictionBook (.fb2, .fb2z, .fbz, .zfb2, .fb2.zip)
  - Palm DOC (.pdb)
  - plain text (.txt, .log, .nfo, .tcr, …)
- Markdown (.md, .markdown), rendered as GitHub Flavored Markdown
- comic book files: .cbz, .cbr, .cbt, .cb7 (and .ora) — see [Comics and manga](Comics-and-manga.md)
- archive files (.zip, .rar, .7z, .tar) containing images
- DjVu (.djvu, .djv)
- Microsoft Compiled HTML Help (.chm)
- XPS (.xps, .oxps, .xod, .dwfx)
- SVG (.svg)
- images:
  - PNG (.png)
  - JPEG (.jpg, .jpeg, .jfif)
  - GIF (.gif), including animations
  - TIFF (.tif, .tiff), including multi-page
  - BMP (.bmp, .dib)
  - TGA (.tga)
  - WebP (.webp)
  - JPEG XR (.jxr, .hdp, .wdp)
  - JPEG 2000 (.jp2, .j2k, .jpx, .jpf, .jpm, .j2c)
  - AVIF (.avif)
  - JPEG XL (.jxl)
  - HEIF / HEIC (.heic, .heif) — built-in decoder; see below

If [Ghostscript](https://ghostscript.com/) is installed, we support PostScript (.ps, .eps) and PJL (Printer Job Language) files.

## HEIF / HEIC support

SumatraPDF can open [HEIF images](https://nokiatech.github.io/heif/) (.heic, .heif) and AVIF (.avif) with a built-in decoder:

- **HEIC** (typical phone stills, HEVC-coded): decoded in-process; no Windows codec required for most files
- **AVIF** (AV1-coded): decoded in-process via [dav1d](https://code.videolan.org/videolan/dav1d)
- Grids, overlays, and common transforms (crop / rotate / mirror) are handled for still images

If built-in decode fails, on Windows we still try the system [WIC](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-windows-imaging-codec) path (for example if a third-party HEIF codec is installed). That fallback is optional for typical HEIC/AVIF files.

You can make SumatraPDF the [default program for handling those file types](Set-as-default-pdf-viewer.md).
