.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool extract`
==========================================


The `extract` command can be used to extract images and font files from a :title:`PDF` file. The image and font files will be saved out to the same folder which the file originates from.




.. code-block:: bash

   mutool extract [options] input.pdf [object numbers]


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

      `-p` password
            Use the specified password if the file is encrypted.

      `-r`
            Convert images to RGB when extracting them.

      `-a`
            Embed SMasks as alpha channel.

      `-N`
            Do not use ICC color conversions.



----


`input.pdf`
   Input file name. Must be a :title:`PDF` file.

----


`[object numbers]`
   An array of object ids to extract from. If no object numbers are given on the command line, all images and fonts will be extracted from the file.

.. include:: footer.rst



.. External links
