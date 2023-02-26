.. Copyright (C) 2001-2022 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool pages`
==========================================


The `pages` command prints out `MediaBox`, `CropBox`, etc. as well as `Rotation` and `UserUnit` for each page given (or all if not specified).


.. code-block:: bash

   mutool pages [options] input.pdf [pages]


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

      `-p` password
         Use the specified password if the file is encrypted.


----

`input.pdf`
   Input file name. Must be a :title:`PDF` file.

----


`[pages]`
   Comma separated list of page numbers and ranges. If no pages are supplied then all document pages will be considered for the output file.







.. include:: footer.rst



.. External links
