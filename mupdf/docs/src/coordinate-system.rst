.. Copyright (C) 2001-2024 Artifex Software, Inc.
.. All Rights Reserved.


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


.. _How_To_Guide_Coordinate_System:

Coordinate Systems
=================================

Origin Point, Point Size and Y-Axis
----------------------------------------------

In **PDF**, the origin (for example: `(0, 0)`) of a page is located at its **bottom-left point**. In **MuPDF**, the origin of a page is located at its **top-left point**.



.. image:: images/img-coordinate-space.webp


.. note::

    The exact x,y coordinate space for origin may not be `0,0` and is defined by "CropBox" - thus "bottom left" is derived from what is defined there.

.. important::

    The rotation of the page affects the PDF coordinate space orientation.


Point Size
~~~~~~~~~~~~~


Coordinates are float numbers and measured in **points**, where:

- **The default is one point equals 1/72 inches**.
- **Is defined by the "UserUnit" property**.


Positioning objects
~~~~~~~~~~~~~~~~~~~~~~~~~~

When working with positioning objects, we should appreciate this coordinate system.

For example:

.. code-block:: javascript

    let rectangle = [0,0,200,200]

Results in:

.. image:: images/200x200-rect.webp



.. _How_To_Guide_Coordinate_System_PDF:

Coordinate space and `PDFObject`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In exception to using the **MuPDF** coordinate space, if we are dealing *directly* with `PDFObject` streams then we are using the **PDF** coordinate space.

Therefore if we doing something low-level like adding a content stream to a `PDFObject`, for example:

.. code-block:: javascript

    var extra_contents = document.addStream("q 200 0 0 200 10 10 cm /Image1 Do Q", null)

Then in this case we are working in the **PDF** coordinate space with the origin Y-axis at the bottom left.

.. note::

    To explain the syntax behind `"q 200 0 0 200 10 10 cm /Image1 Do Q"` this is **Adobe PDF** syntax for referencing, sizing and positioning an image and can be explained as follows:

    .. code-block:: postscript

        q                               % Save graphics state
            200 0 0 200 10 10 cm        % Translate
            /Image1 Do                  % Paintimage
        Q                               % Restore graphics state

    For more see `page 337 of the Adobe PDF Reference Guide <https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf>`_.
