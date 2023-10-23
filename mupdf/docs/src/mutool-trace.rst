.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool trace`
==========================================


The `trace` command prints a trace of device calls needed to render a page.


.. code-block:: bash

   mutool trace [options] file [pages]


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:


   `-p` password
      Use the specified password if the file is encrypted.
   `-W` width
      Page width in points for :title:`EPUB` layout.
   `-H` height
      Page height in points for :title:`EPUB` layout.
   `-S` size
      Font size in points for :title:`EPUB` layout.
   `-U` filename
      User CSS stylesheet for :title:`EPUB` layout.
   `-X`
      Disable document styles for :title:`EPUB` layout.
   `-d`
      Use display list.


----

`file`
   Input file name. The input can be any of the :ref:`document formats supported by MuPDF<supported_document_formats>`.

----

`[pages]`
   Comma separated list of page ranges. The first page is "1", and the last page is "N". The default is "1-N".

----


The trace takes the form of an XML document, with the root element being the document, its children each page, and one page child element for each device call on that page.

**An example trace:**

.. code-block:: xml

   <document filename="hello.pdf">
   <page number="1" mediabox="0 0 595 842">
   <fill_path winding="nonzero" colorspace="DeviceRGB" color="1 0 0" transform="1 0 0 -1 0 842">
   <moveto x="50" y="50"/>
   <lineto x="100" y="200"/>
   <lineto x="200" y="50"/>
   </fill_path>
   <fill_text colorspace="DeviceRGB" color="0" transform="1 0 0 -1 0 842">
   <span font="Times-Roman" wmode="0" trm="100 0 0 100">
   <g unicode="H" glyph="H" x="50" y="500" />
   <g unicode="e" glyph="e" x="122.2" y="500" />
   <g unicode="l" glyph="l" x="166.6" y="500" />
   <g unicode="l" glyph="l" x="194.4" y="500" />
   <g unicode="o" glyph="o" x="222.2" y="500" />
   <g unicode="!" glyph="exclam" x="272.2" y="500" />
   </span>
   </fill_text>
   </page>
   </document>


.. include:: footer.rst



.. External links
