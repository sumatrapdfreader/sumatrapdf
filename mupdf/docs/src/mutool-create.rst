.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool create`
==========================================


The `create` command creates a new :title:`PDF` file with the contents created from one or more input files containing graphics commands.


.. code-block:: bash

   mutool create [-o output.pdf] [-O options] page.txt [page2.txt ...]


.. include:: optional-command-line-note.rst



`[-o output.pdf]`
   The output filename. Defaults to "out.pdf" if not supplied.


----

`[-O options]`
   The `-O` argument is a comma separated list of options for writing the :title:`PDF` file:

      .. include:: mutool-comma-separated-list-of-options.rst

----


`page.txt`
   :ref:`Page content stream<mutool_create_page_content_streams>` with annotations for creating resources.

----

`[page2.txt ...]`
   Define multiple :ref:`page content streams<mutool_create_page_content_streams>` as required.


----


.. _mutool_create_page_content_streams:

Page content streams
-------------------------

A page is created for each input file, with the contents of the file copied into the content stream. Special comments in the input files are parsed to define the page dimensions and font and image resources.


**Example #1**

Define an image:

.. code-block::

   %%MediaBox 0 0 500 800
   %%Rotate 90
   %%Image Im0 path/to/image.png






**Example #2**

Font resources can be created by either giving the name of a standard :title:`PDF` font, or by giving the path to a font file. If a third argument is present and either "Greek" or "Cyrillic" the font will be encoded using ISO 8859-7 or KOI8-U, respectively:

.. code-block::

   %%Font Tm Times-Roman
   %%Font TmG Times-Roman Greek
   %%Font TmC Times-Roman Cyrillic
   %%Font Fn0 path/to/font/file.ttf
   %%Font Fn1 path/to/font/file.ttf Cyrillic



**Example #3**

CJK fonts can be created by passing a language tag for one of the 4 CID orderings: zh-Hant, zh-Hans, ja, or ko (Traditional Chinese, Simplified Chinese, Japanese, Korean). The CJK font will use the UTF-16 encoding. A font file will not be embedded, so a :title:`PDF` viewer will use a substitute font:


.. code-block::

   %%CJKFont Batang ko
   %%CJKFont Mincho ja
   %%CJKFont Ming zh-Hant
   %%CJKFont Song zh-Hans




**Example #4**

An example input file, which adds an image, a triangle and some text:

.. code-block::

   %%MediaBox 0 0 595 842
   %%Font TmRm Times-Roman
   %%Font Helv-C Helvetica Cyrillic
   %%Font Helv-G Helvetica Greek
   %%CJKFont Song zh-Hant
   %%CJKFont Mincho ja
   %%CJKFont Batang ko
   %%Image I0 logo/mupdf-simplified-logo.png

   % Draw an image.
   q
   480 0 0 480 50 250 cm
   /I0 Do
   Q

   % Draw a triangle.
   q
   1 0 0 rg
   50 50 m
   100 200 l
   200 50 l
   f
   Q

   % Show some text.
   q
   0 0 1 rg
   BT /TmRm 24 Tf 50 760 Td (Hello, world!) Tj ET
   BT /Helv-C 24 Tf 50 730 Td <fac4d2c1d7d3d4d7d5cad4c521> Tj ET
   BT /Helv-G 24 Tf 50 700 Td <eae1ebe7ecddf1e1> Tj ET
   BT /Song 24 Tf 50 670 Td <4F60 597D> Tj ET
   BT /Mincho 24 Tf 50 640 Td <3053 3093 306b 3061 306f> Tj ET
   BT /Batang 24 Tf 50 610 Td <c548 b155 d558 c138 c694> Tj ET
   Q





.. include:: footer.rst



.. External links
