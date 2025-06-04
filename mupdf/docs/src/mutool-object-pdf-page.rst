.. _mutool_object_pdf_page:


.. _mutool_run_js_api_pdf_page:




`PDFPage`
---------------

Extends :ref:`Page<mutool_run_js_api_page>`.


|instance_methods|

.. method:: getAnnotations()

    Return array of all annotations on the page.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var annots = pdfPage.getAnnotations();


.. _mutool_run_js_api_pdf_page_createAnnotation:

.. method:: createAnnotation(type)

    Create a new blank annotation of a given :ref:`type<mutool_run_js_api_annotation_types>`.

    :arg type: `String` representing :ref:`annotation type<mutool_run_js_api_annotation_types>`.
    :return: `PDFAnnotation`.

    |example_tag|

    .. code-block:: javascript

        var annot = pdfPage.createAnnotation("Text");


.. _mutool_run_js_api_annotation_types:


**Annotation types**

.. note::

    Annotation types are also referred to as "subtypes".


.. list-table::
   :header-rows: 1

   * - **Name**
     - **Supported**
     - **Notes**
   * - Text
     - Yes
     -
   * - Link
     - Yes
     - Please use :ref:`Page.createLink()<mutool_run_js_api_page_create_link>`.
   * - FreeText
     - Yes
     -
   * - Square
     - Yes
     -
   * - Circle
     - Yes
     -
   * - Line
     - Yes
     -
   * - Polygon
     - Yes
     -
   * - PolyLine
     - Yes
     -
   * - Highlight
     - Yes
     -
   * - Underline
     - Yes
     -
   * - Squiggly
     - Yes
     -
   * - StrikeOut
     - Yes
     -
   * - Redact
     - Yes
     -
   * - Stamp
     - Yes
     -
   * - Caret
     - Yes
     -
   * - Ink
     - Yes
     -
   * - Popup
     - No
     -
   * - FileAttachment
     - Yes
     -
   * - Sound
     - No
     -
   * - Movie
     - No
     -
   * - RichMedia
     - No
     -
   * - Widget
     - No
     -
   * - Screen
     - No
     -
   * - PrinterMark
     - No
     -
   * - TrapNet
     - No
     -
   * - Watermark
     - No
     -
   * - 3D
     - No
     -
   * - Projection
     - No
     -


.. method:: deleteAnnotation(annot)

    Delete the annotation from the page.

    :arg annot: `PDFAnnotation`.

    |example_tag|

    .. code-block:: javascript

        pdfPage.deleteAnnotation(annot);


.. _mutool_run_js_api_pdf_page_getWidgets:

.. method:: getWidgets()

    Return array of all :ref:`widgets<mutool_object_pdf_widget>` on the page.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var widgets = pdfPage.getWidgets();


.. method:: update()

    Loop through all annotations of the page and update them. Returns true if re-rendering is needed because at least one annotation was changed (due to either events or :title:`JavaScript` actions or annotation editing).

    |example_tag|

    .. code-block:: javascript

        pdfPage.update();

.. method:: applyRedactions(blackBoxes, imageMethod)

    Applies redactions to the page.

    :arg blackBoxes: `Boolean` Whether to use black boxes at each redaction or not.
    :arg imageMethod: `Integer`. `0` for no redactions, `1` to redact entire images, `2` for redacting just the covered pixels.

    .. note::

        Redactions are secure as they remove the affected content completely.

    |example_tag|

    .. code-block:: javascript

        pdfPage.applyRedactions(true, 1);

.. method:: process(processor)

    |mutool_tag|

    Run through the page contents stream and call methods on the supplied :ref:`PDF processor<mutool_run_js_api_pdf_processor>`.

    :arg processor: User defined function.


    |example_tag|

    .. code-block:: javascript

        pdfPage.process(processor);


.. method:: toPixmap(transform, colorspace, alpha, renderExtra, usage, box)

    Render the page into a `Pixmap` using the given `colorspace` and `alpha` while applying the `transform`. Rendering of annotations/widgets can be disabled. A page can be rendered for e.g. "View" or "Print" usage.

    :arg transform: `[a,b,c,d,e,f]` The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.
    :arg renderExtra: `Boolean` Whether annotations and widgets should be rendered.
    :arg usage: `String` "View" or "Print".
    :arg box: `String` Default is "CropBox".

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pixmap = pdfPage.toPixmap(mupdf.Matrix.identity,
                                      mupdf.ColorSpace.DeviceRGB,
                                      false,
                                      true,
                                      "View",
                                      "CropBox");

.. redundant

    .. method:: getTransform()

        Return the transform from :title:`Fitz` page space (upper left page origin, y descending, 72 dpi) to :title:`PDF` user space (arbitrary page origin, y ascending, UserUnit dpi).

        :return: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

        |example_tag|

        .. code-block:: javascript

            var transform = pdfPage.getTransform();

.. method:: createSignature(name)

    |mutool_tag|

    Create a new signature widget with the given name as field label.

    :arg name: `String` The desired field label.

    :return: `PDFWidget`.

    |example_tag|

    .. code-block:: javascript

        var signatureWidget = pdfPage.createSignature("test");

.. method:: countAssociatedFiles()

    |mutool_tag|

    Return the number of Associated Files on this page. Note that this is the number of files associated to this page, not necessarily the total number of files associated with elements throughout the entire document.

    :return: `Integer`


    |example_tag|

    .. code-block:: javascript

        var count = pdfPage.countAssociatedFiles();




.. method:: associatedFile(n)

    |mutool_tag|

    Return the FileSpec object that represents the nth Associated File on this page. 0 <= n < count, where count is the value given by countAssociatedFiles().

    :return fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.


    |example_tag|

    .. code-block:: javascript

        var obj = pdfPage.associatedFile(0);



.. method:: getObject()

    Return the `PDFObject` that corresponds to this PDF page.

    :return: `Object` :ref:`PDFObject<mutool_run_js_api_pdf_object>`.

    |example_tag|

    .. code-block:: javascript

        var pageObj = pdfPage.getObject();



.. method:: setPageBox(boxname, rect)

    Modify the page boxes using fitz space coordinates.

    Note that changing the CropBox will change the fitz coordinate space mapping,
    invalidating all bounding boxes previously acquired.

    :arg boxname: `String` representing :ref:`box type<mutool_run_js_api_box_types>`.
    :arg rect: `Array`. `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        pdfPage.setPageBox("MediaBox", [0, 0, 612, 792]);

.. _mutool_run_js_api_box_types:

**Box name values**

.. list-table::
   :header-rows: 1

   * - **Box names**
   * - "MediaBox"
   * - "CropBox"
   * - "BleedBox"
   * - "TrimBox"
   * - "ArtBox"
