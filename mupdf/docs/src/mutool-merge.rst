.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool merge`
==========================================


The `merge` command is used to pick out pages from two or more files and merge them into a new output file.



.. code-block:: bash

   mutool merge [-o output.pdf] [-O options] input.pdf [pages] [input2.pdf] [pages2] ...



.. include:: optional-command-line-note.rst



`[-o output.pdf]`
   The output filename. Defaults to "out.pdf" if not supplied.

----

`[-O options]`
   Comma separated list of output options.

   .. include:: mutool-comma-separated-list-of-options.rst


----

`input.pdf`
   The first document.


----

`[pages]`
   Comma separated list of page ranges for the first document (`input.pdf`). The first page is "1", and the last page is "N". The default is "1-N".


----

`[input2.pdf]`
   The second document.


----

`[pages2]`
   Comma separated list of page ranges for the second document (`input2.pdf`). The first page is "1", and the last page is "N". The default is "1-N".


----

`...`
   Indicates that we add as many additional `[input]` & `[pages]` pairs as required to merge multiple documents.

.. include:: footer.rst



.. External links
