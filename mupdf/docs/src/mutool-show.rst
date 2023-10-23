.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool show`
==========================================


The `show` command will print the specified objects and streams to `stdout`. Streams are decoded and non-printable characters are represented with a period by default.



.. code-block:: bash

   mutool show [options] input.pdf ( trailer | xref | pages | grep | outline | js | form | <path> ) *

.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:

      `-p` password
         Use the specified password if the file is encrypted.
      `-o` output
         The output file name instead of using `stdout`. Should be a plain text file format.
      `-e`
         Leave stream contents in their original form.
      `-b`
         Print only stream contents, as raw binary data.
      `-g`
         Print only object, one line per object, suitable for grep.

----


`input.pdf`
   Input file name. Must be a :title:`PDF` file.

----

`( trailer | xref | pages | grep | outline | js | form | <path> ) *`
   Specify what to show by using one of the following keywords, or specify a path to an object:

   `trailer`
      Print the trailer dictionary.

   `xref`
      Print the cross reference table.

   `pages`
      List the object numbers for every page.

   `grep`
      Print all the objects in the file in a compact one-line format suitable for piping to grep.

   `outline`
      Print the outline (also known as "table of contents" or "bookmarks").

   `js`
      Print document level :title:`JavaScript`.

   `form`
      Print form objects.

   `<path>`
      A path starts with either an object number, a property in the trailer dictionary, or the keyword "trailer" or "pages". Separate elements with a period '.' or slash '/'. Select a page object by using pages/N where N is the page number. The first page is number 1.

   `*`
      You can use `*` as an element to iterate over all array indices or dictionary properties in an object. Thus you can have multiple keywords with for your `mutool show` query.



----

**Examples:**

   Find the number of pages in a document:

   .. code-block:: bash

      mutool show $FILE trailer/Root/Pages/Count

   Print the raw content stream of the first page:

   .. code-block:: bash

      mutool show -b $FILE pages/1/Contents

   Print the raw content stream of the first page & second page & the PDF outline (demonstrates use of the `*` element):

   .. code-block:: bash

      mutool show -b $FILE pages/1/Contents pages/2/Contents outline

   Show all :title:`JPEG` compressed stream objects:

   .. code-block:: bash

      mutool show $FILE grep | grep '/Filter/DCTDecode'


.. include:: footer.rst



.. External links
