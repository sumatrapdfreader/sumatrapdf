.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


Third Party Libraries Used by :title:`MuPDF`
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
     - 2.13.0
     - Font scaling and rendering
     - BSD-style
   * - harfbuzz_
     - 6.0.0
     - Text shaping
     - MIT-style
   * - libjpeg_
     - 9.0e with patches
     - JPEG decoding
     - BSD-style
   * - `Incompatible fork of lcms2`_
     - 2.14 with patches
     - Color management
     - MIT-style
   * - openjpeg_
     - 2.5.0
     - JPEG 2000 decoding
     - BSD-style
   * - zlib_
     - 1.2.13
     - Deflate compression
     - zlib License
   * - `gumbo-parser`_
     - 0.10.1
     - HTML5 parser
     - Apache 2.0
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
     - 5.0.0-alpha-20201231 with patches
     - OCR
     - Apache 2.0
   * - Leptonica_
     - 1.80.0 with patches
     - Tesseract dependency
     - BSD-style



.. note::

   jbig2dec_ and MuJS_ are included in "thirdparty" but are copyright :title:`Artifex Software Inc`.



.. include:: footer.rst



.. External links

.. _freetype: http://www.freetype.org/
.. _harfbuzz: http://www.harfbuzz.org/
.. _libjpeg: http://www.ijg.org/
.. _Incompatible fork of lcms2: http://git.ghostscript.com/?p=thirdparty-lcms2.git;a=summary
.. _openjpeg: http://www.openjpeg.org/
.. _zlib: http://www.zlib.net/
.. _gumbo-parser: https://github.com/google/gumbo-parser
.. _FreeGLUT: http://freeglut.sourceforge.net/
.. _curl: http://curl.haxx.se/
.. _JPEG-XR reference: https://www.itu.int/rec/T-REC-T.835/
.. _Tesseract: https://tesseract-ocr.github.io/
.. _Leptonica: https://github.com/DanBloomberg/leptonica
.. _jbig2dec: https://jbig2dec.com/?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
.. _MuJS: https://mujs.com/?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
