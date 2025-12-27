.. Copyright (C) 2001-2025 Artifex Software, Inc.
.. All Rights Reserved.



.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


Third Party Libraries
==================================================

These are the third party libraries used by :title:`MuPDF`.

.. list-table::
   :header-rows: 1

   * - **Library**
     - **Version**
     - **Function**
     - **License**
   * - **Required**
     -
     -
     -
   * - freetype_
     - 2.13.3
     - Font scaling and rendering
     - BSD-style
   * - harfbuzz_
     - 6.0.0 with patches
     - Text shaping
     - MIT-style
   * - libjpeg_
     - 9.0f with patches
     - JPEG decoding
     - BSD-style
   * - `Incompatible fork of lcms2`_
     - 2.16 with patches
     - Color management
     - MIT-style
   * - openjpeg_
     - 2.5.4
     - JPEG 2000 decoding
     - BSD-style
   * - zlib_
     - 1.3.1
     - Deflate compression
     - zlib License
   * - `gumbo-parser`_
     - 0.10.1 with patches
     - HTML5 parser
     - Apache 2.0
   * - `brotli`_
     - 1.1.0 with upstream and local patches
     - Brotli compression
     - MIT-style
   * - **Optional**
     -
     -
     -
   * - FreeGLUT_
     - 3.0.0 with patches
     - OpenGL API for UI
     - MIT-style
   * - curl_
     - 7.66.0 with patches
     - HTTP data transfer
     - MIT-style
   * - `JPEG-XR reference`_
     - 1.32 with patches
     - JPEG-XR decoding
     - special
   * - Tesseract_
     - 5.5.0 with patches
     - OCR
     - Apache 2.0
   * - Leptonica_
     - 1.85.0 with patches
     - Tesseract dependency
     - BSD-style
   * - Zint_
     - 2.13.0.9
     - Zxing-cpp dependency
     - BSD-style
   * - Zxing-cpp_
     - 2.3.0 with patches
     - Barcode decoding/encoding
     - Apache 2.0



.. note::

   jbig2dec_ and MuJS_ are included in "thirdparty" but are copyright :title:`Artifex Software Inc`.






.. External links

.. _freetype: http://www.freetype.org/
.. _harfbuzz: http://www.harfbuzz.org/
.. _libjpeg: http://www.ijg.org/
.. _Incompatible fork of lcms2: http://cgit.ghostscript.com/cgi-bin/cgit.cgi/thirdparty-lcms2.git/
.. _openjpeg: http://www.openjpeg.org/
.. _zlib: http://www.zlib.net/
.. _gumbo-parser: https://github.com/google/gumbo-parser
.. _brotli: https://brotli.org/
.. _FreeGLUT: http://freeglut.sourceforge.net/
.. _curl: http://curl.haxx.se/
.. _JPEG-XR reference: https://www.itu.int/rec/T-REC-T.835/
.. _Tesseract: https://tesseract-ocr.github.io/
.. _Leptonica: https://github.com/DanBloomberg/leptonica
.. _jbig2dec: https://jbig2dec.com/?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
.. _MuJS: https://mujs.com/?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
.. _Zint: https://www.zint.org.uk/
.. _Zxing-cpp: https://github.com/zxing-cpp/zxing-cpp
