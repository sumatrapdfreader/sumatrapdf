.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool trim`
==========================================

This command allows you to make a modified version of a :title:`PDF` with content that falls inside or outside of :ref:`defined boxes<mutool_trim_defined_boxes>` removed.

See :ref:`mutool pages<mutool_pages>` to get information on the box elements in a :title:`PDF`.


.. code-block:: bash

   mutool trim [options] input.pdf

`[options]`
   Options are as follows:

      `-b` box
         Which box to trim to (`mediabox` (default), `cropbox`, `bleedbox`, `trimbox`, `artbox`).

      `-m` margins
         Add margins to box (positive for inwards, negative for outwards).

         *<All>* or *<V>,<H>* or *<T>,<R>,<B>,<L>*

      `-e`
         Exclude contents of box, rather than include them.

      `-f`
         Fallback to `mediabox` if specified box not available.

      `-o` filename
         Output file.





.. include:: optional-command-line-note.rst





----

**Examples:**

   Trim a document by trimming the MediaBox_ elements with a margin of 200 points inwards:

   .. code-block::

      mutool trim -b mediabox -m 200 -o out.pdf in.pdf


   Trim a document by trimming the MediaBox_ elements with a margin of 20 points from the top & bottom (vertical) and 30 points from the left & right (horizontal):

   .. code-block::

      mutool trim -b mediabox -m 20,30 -o out.pdf in.pdf


   Trim a document by trimming the MediaBox_ elements to a box offset 10 points from the top & right, 50 points from the bottom and 20 points in from the left:

   .. code-block::

      mutool trim -b mediabox -m 10,10,50,20 -o out.pdf in.pdf


   Exclude the contents of the ArtBox_ element:

   .. code-block::

      mutool trim -b artbox -e -o out.pdf in.pdf



.. _mutool_trim_defined_boxes:


.. _MediaBox:

**MediaBox**


The MediaBox is for complete pages including items that will be physically trimmed from the final product like crop marks, registration marks, etc.


.. _CropBox:

**CropBox**


The CropBox defines the region that a :title:`PDF` is expected to display or print.


.. _BleedBox:

**BleedBox**


The BleedBox determines the region to which the page contents expect to be clipped.


.. _trimBox:

**TrimBox**


The TrimBox defines the intended dimensions of the finished page.


.. _ArtBox:

**ArtBox**


The ArtBox can be used to denote areas where it is considered "safe" to place graphical elements.



.. include:: footer.rst



.. External links
