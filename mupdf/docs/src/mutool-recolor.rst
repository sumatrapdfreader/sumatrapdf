.. Copyright (C) 2025 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf


:title:`mutool recolor`
==========================================


The `recolor` command changes the colorspaces used in
a :title:`PDF` file.


.. code-block:: bash

   mutool recolor [options] file


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

   `-c` colorspace
      The desired colorspace used in the output document: `gray` (default), `rgb` or `cmyk`.

   `-o` output file
      Use the specified filename for the output :title:`PDF` file with altered colorspaces.

   `-r`
      Remove any output intents present in the input file.

----

`file`
   Input file name. The input must be a :title:`PDF` file.

----





.. External links
