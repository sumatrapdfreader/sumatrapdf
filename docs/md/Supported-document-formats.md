# Supported document formats

SumatraPDF reader supports the following document types:

- PDF (.pdf)
- eBook formats:
    - unencrypted EPUB (.epub)
    - MOBI (.mobi and un-encrypted .azw)
    - FictionBook (.fb2, .fb2z, .zfb2)
    - .pdb (Palm DOC format)
    - .tcr
- comic book files: .cbz, .cbr, .cbt, .cb7
- archive files (.7z, .rar, .tar, .zip) with images
- DjVu (.djv, .djvu)
- Microsoft Compiled HTML Html (.chm)
- XPS (.xps, .oxps, .xod);
- images (.jpg, .png,.gif, .webp, .tiff, tga, .j2k, .bmp, .dib)
- HEIF (if codec installed, see below)

If [Ghostscript](https://ghostscript.com/) is installed, we support PostScript (.ps, .eps) and PJL (Printer Job Language) files.

## HEIF support

**Ver 3.4+**: SumatraPDF can open [HEIF images](https://nokiatech.github.io/heif/) but only if Windows has a codec for the format.

You can use one of those codecs:

- [https://www.copytrans.net/copytransheic/](https://www.copytrans.net/copytransheic/) : free for personal use
- [https://www.microsoft.com/en-us/p/heif-image-extensions/9pmmsr1cgpwg?activetab=pivot:overviewtab](https://www.microsoft.com/en-us/p/heif-image-extensions/9pmmsr1cgpwg?activetab=pivot:overviewtab) : HEIF image codec from Microsoft, Windows 10 or later

You can make SumatraPDF to be [default program for handling those file types](Set-as-default-PDF-viewer.md).