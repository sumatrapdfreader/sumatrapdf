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
- comic book files: .cbz, .cbr, .cbt, .cb7 (and .ora)
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
  - HEIF / HEIC (.heic, .heif) — may need a Windows codec, see below

If [Ghostscript](https://ghostscript.com/) is installed, we support PostScript (.ps, .eps) and PJL (Printer Job Language) files.

## HEIF / HEIC support

**Ver 3.4+**: SumatraPDF can open [HEIF images](https://nokiatech.github.io/heif/). AVIF decodes out of the box, but HEIC (HEVC-coded) may require a Windows codec for the underlying video codec.

You can use one of those codecs:

- [https://www.copytrans.net/copytransheic/](https://www.copytrans.net/copytransheic/) : free for personal use
- [https://www.microsoft.com/en-us/p/heif-image-extensions/9pmmsr1cgpwg?activetab=pivot:overviewtab](https://www.microsoft.com/en-us/p/heif-image-extensions/9pmmsr1cgpwg?activetab=pivot:overviewtab) : HEIF image codec from Microsoft, Windows 10 or later

You can make SumatraPDF the [default program for handling those file types](Set-as-default-pdf-viewer.md).
