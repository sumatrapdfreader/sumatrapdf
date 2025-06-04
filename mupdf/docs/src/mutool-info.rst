.. Copyright (C) 2001-2025 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool info`
==========================================


The `info` command shows info about the objects on pages in an input file. For each page the page object number is shown, and then detailed information about the desired objects.

.. code-block:: bash

   mutool info [options] file [pages]


.. include:: optional-command-line-note.rst



`[options]`
   Options are as follows:

      `-p` password
         Use the specified password if the file is encrypted.

      `-F`
         List fonts.

      `-I`
         List images.

      `-M`
         List dimensions.

      `-P`
         List patterns.

      `-S`
         List shadings.

      `-X`
         List form and postscript xobjects.

      `-Z`
         List ZUGFeRD info.


----


`file`
   The input file.

----

`[pages]`
   Comma separated list of page numbers and ranges. If no pages are supplied then all document pages will be considered for the output file.





.. External links
