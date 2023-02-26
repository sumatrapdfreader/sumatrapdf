.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_page:


.. _mutool_run_js_api_pdf_page:




`PDFPage`
---------------

.. method:: getAnnotations()

    Return array of all annotations on the page.

    :return: `[]`.


.. method:: createAnnotation(type)

    Create a new blank annotation of a given :ref:`type<mutool_run_js_api_annotation_types>`.

    :arg type: `String` representing :ref:`annotation type<mutool_run_js_api_annotation_types>`.
    :return: `PDFAnnotation`.


.. _mutool_run_js_api_annotation_types:


**Annotation types**

.. note::

    Annotation types are also referred to as "subtypes".


.. list-table::
   :header-rows: 1

   * - **Name**
   * - Text
   * - Link
   * - FreeText
   * - Square
   * - Circle
   * - Polygon
   * - PolyLine
   * - Highlight
   * - Underline
   * - Squiggly
   * - StrikeOut
   * - Redact
   * - Stamp
   * - Caret
   * - Ink
   * - Popup
   * - FileAttachment
   * - Sound
   * - Movie
   * - RichMedia
   * - Widget
   * - Screen
   * - PrinterMark
   * - TrapNet
   * - Watermark
   * - 3D
   * - Projection


.. method:: deleteAnnotation(annot)

    Delete the annotation from the page.

    :arg annot: `PDFAnnotation`.


.. method:: getWidgets()

    Return array of all :ref:`widgets<mutool_object_pdf_widget>` on the page.

    :return: `[]`.


.. method:: update()

    Loop through all annotations of the page and update them. Returns true if re-rendering is needed because at least one annotation was changed (due to either events or :title:`JavaScript` actions or annotation editing).

.. method:: applyRedactions(blackBoxes, imageMethod)

    Applies redactions to the page.

    :arg blackBoxes: `Boolean` Whether to use black boxes at each redaction or not.
    :arg imageMethod: `Integer`. `0` for no redactions, `1` to redact entire images, `2` for redacting just the covered pixels.

    .. note::

        Redactions are secure as they remove the affected content completely.

.. method:: process(processor)

    Run through the page contents stream and call methods on the supplied :ref:`PDF processor<mutool_run_js_api_pdf_processor>`.

    :arg processor: User defined function.

.. method:: toPixmap(transform, colorspace, alpha, renderExtra, usage)

    Render the page into a `Pixmap` using the given `colorspace` and `alpha` while applying the `transform`. Rendering of annotations/widgets can be disabled. A page can be rendered for e.g. "View" or "Print" `usage`.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.
    :arg renderExtra: `Boolean`.
    :usage: `String`.



.. method:: getTransform()

    Return the transform from :title:`Fitz` page space (upper left page origin, y descending, 72 dpi) to :title:`PDF` user space (arbitrary page origin, y ascending, UserUnit dpi).

    :return: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
