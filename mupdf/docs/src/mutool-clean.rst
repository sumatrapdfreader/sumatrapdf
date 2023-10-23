.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool clean`
==========================================


The `clean` command pretty prints and rewrites the syntax of a :title:`PDF` file. It can be used to repair broken files, expand compressed streams, filter out a range of pages, etc.

.. code-block:: bash

   mutool clean [options] input.pdf [output.pdf] [pages]

.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

      `-p` password
         Use the specified password if the file is encrypted.

      `-g`
         Garbage collect unused objects.

      `-gg`
         In addition to `-g` compact xref table.

      `-ggg`
         In addition to `-gg` merge duplicate objects.

      `-gggg`
         In addition to `-ggg` check streams for duplication.

      `-l`
         Linearize PDF.

      `-D`
         Save file without encryption.

      `-E` encryption
         Save file with new encryption (`rc4-40`, `rc4-128`, `aes-128`, or `aes-256`).

      `-O` owner_password
         Owner password (only if encrypting).

      `-U` user_password
         User password (only if encrypting).

      `-P` permission
         Permission flags (only if encrypting).

      `-a`
         ASCII hex encode binary streams.

      `-d`
         Decompress streams.

      `-z`
         Deflate uncompressed streams.

      `-f`
         Compress font streams.

      `-i`
         Compress image streams.

      `-c`
         Clean content streams.

      `-s`
         Sanitize content streams.

      `-A`
         Create appearance streams for annotations.

      `-AA`
         Recreate appearance streams for annotations.

      `-m`
         Preserve metadata.


----


`input.pdf`
   Input file name. Must be a :title:`PDF` file.

----

`[output.pdf]`
   The output file. Must be a :title:`PDF` file.

   .. note::

      If no output file is specified, it will write the cleaned :title:`PDF` to "out.pdf" in the current directory.

----

`[pages]`
   Comma separated list of page numbers and ranges. If no pages are supplied then all document pages will be considered for the output file.


.. include:: footer.rst



.. External links
