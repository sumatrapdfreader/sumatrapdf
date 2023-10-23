.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool poster`
==========================================


The `poster` command reads the input PDF file and for each page chops it up into `x` by `y` pieces. Each piece becomes its own page in the output :title:`PDF` file. This makes it possible for each page to be printed upscaled and can then be merged into a large poster.



.. code-block:: bash

   mutool poster [options] input.pdf [output.pdf]


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:


   `-p` password
      Use the specified password if the file is encrypted.

   `-x` x decimation factor
      Pieces to horizontally divide each page into.

   `-y` y decimation factor
      Pieces to vertically divide each page into.

   `-r`
      Split horizontally from right to left (default splits from left to right).


----

`input.pdf`
   The input :title:`PDF` document.


----

`[output.pdf]`
   The output filename. Defaults to "out.pdf" if not supplied.




.. include:: footer.rst



.. External links
