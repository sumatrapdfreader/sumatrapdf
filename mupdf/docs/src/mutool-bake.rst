.. Copyright (C) 2025 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf


:title:`mutool bake`
==========================================


The `bake` command changes the colorspaces used in
a :title:`PDF` file.


.. code-block:: bash

   mutool bake [options] input.pdf [output.pdf]


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

   `-A`
      Do not bake annotations into page contents.

   `-F`
      Do not bake form fields into page contents.

   `[-O options]`
      The `-O` argument is a comma separated list of options for writing the :title:`PDF` file. If none are given `garbage` is used by default.

      .. include:: mutool-comma-separated-list-of-options.rst

----

`input.pdf`
   Name of input :title:`PDF` file.

----

`output.pdf`
   Optional name of output :title:`PDF` file. If none is given `out.pdf` is used by default.





.. External links
