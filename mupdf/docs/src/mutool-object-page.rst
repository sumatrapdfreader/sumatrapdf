.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_page:

.. _mutool_run_js_api_page:

`Page`
-------------

The base class for a :ref:`PDF Page<mutool_run_js_api_pdf_page>`.


|instance_methods|

.. method:: getBounds()

    Returns a :ref:`rectangle<mutool_run_js_api_rectangle>` containing the page dimensions.

    :return: `[ulx,uly,lrx,lry]`.

    |example_tag|

    .. code-block:: javascript

        var rect = page.getBounds();


.. _Page_run:


.. method:: run(device, matrix)

    |wasm_tag|

    Calls device functions for all the contents on the page, using the specified transform :ref:`matrix<mutool_run_js_api_matrix>`. The `device` can be one of the built-in devices or a :title:`JavaScript` object with methods for the device calls. The `matrix` maps from user space points to device space pixels.

    :arg device: The device object.
    :arg matrix: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        var rect = page.run(obj, mupdf.Matrix.identity);


.. method:: runPageContents(device, matrix)

    |wasm_tag|

    This is the same as the :ref:`run<Page_run>` method above but it only considers the page itself and omits annotations and widgets.

    :arg device: The device object.
    :arg matrix: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        var rect = page.runPageContents(obj, mupdf.Matrix.identity);


.. method:: runPageAnnots(device, matrix)

    |wasm_tag|

    This is the same as the :ref:`run<Page_run>` method above but it only considers the page annotations.

    :arg device: The device object.
    :arg matrix: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        var rect = page.runPageAnnots(obj, mupdf.Matrix.identity);


.. method:: runPageWidgets(device, matrix)

    |wasm_tag|

    This is the same as the :ref:`run<Page_run>` method above but it only considers the page widgets.

    :arg device: The device object.
    :arg matrix: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        var rect = page.runPageWidgets(obj, mupdf.Matrix.identity);


.. method:: toPixmap(matrix, colorspace, alpha, showExtras)

    Render the page into a :ref:`Pixmap<mutool_run_js_api_pixmap>`, using the specified transform :ref:`matrix<mutool_run_js_api_matrix>` and `colorspace`. If `alpha` is *true*, the page will be drawn on a transparent background, otherwise white. If `showExtras` is *true* then the operation will include any page annotations and/or widgets.

    :arg matrix: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.
    :arg showExtras: `Boolean`.

    :return: `Pixmap`.

    .. note::

        In :title:`MuPDF WASM` `alpha` & `showExtras` default to *true* unless otherwise specified.
        In :title:`mutool run` `alpha` defaults to *false* and `showExtras` defaults to *true* unless otherwise specified.

    |example_tag|

    .. code-block:: javascript

        var pixmap = page.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true, true);

.. method:: toDisplayList(showExtras)

    Record the contents on the page into a :ref:`DisplayList<mutool_object_display_list>`. If `showExtras` is *true* then the operation will include any page annotations and/or widgets.


    :arg showExtras: `Boolean`.

    :return: `DisplayList`.

    .. note::

        In both :title:`MuPDF WASM` and :title`mutool run` `showExtras` defaults to *true* unless otherwise specified.

    |example_tag|

    .. code-block:: javascript

        var displayList = page.toDisplayList(true);


.. _mutool_page_toStructuredText:


.. method:: toStructuredText(options)

    Extract the text on the page into a `StructuredText` object. The options argument is a comma separated list of flags: "preserve-ligatures", "preserve-whitespace", "preserve-spans", and "preserve-images".

    :arg options: `String`.
    :return: `StructuredText`.

    |example_tag|

    .. code-block:: javascript

        var sText = page.toStructuredText("preserve-whitespace");


.. method:: search(needle, max_hits)


    Search the page text for all instances of the `needle` value, and return an array of search hits. Each search hit is an array of :ref:`rectangles<mutool_run_js_api_quad>` corresponding to all characters in the search hit.

    :arg needle: `String`.
    :arg max_hits: `Integer` Defaults to 500 unless otherwise specified.
    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var results = page.search("my search phrase");


    .. note::

        The numbers are `[ulx, uly, urx, ury, llx, lly, lrx, lry]` for each rectangle against each result. These type of rectangles are know as "Quads" or "QuadPoints" in the :title:`PDF` specification.


.. method:: getLinks()

    Return an array of all the links on the page. Each link is an object with a 'bounds' property, and either a 'page' or 'uri' property, depending on whether it's an internal or external link. See: :ref:`Link<mutool_object_link>`.

    :return: `[...]`.


    .. code-block:: javascript

        var links = page.getLinks();
        var link = links[0];
        var linkDestination = doc.resolveLink(link)


.. _mutool_run_js_api_page_create_link:


.. method:: createLink(rect, destinationUri)

    Create a new link within the rectangle on the page, linking to the destination URI string.

    :arg rect: :ref:`Rectangle<mutool_run_js_api_rectangle>` for the link.
    :arg destinationUri: `String` containing URI.
    :return: :ref:`Link<mutool_object_link>`.

    |example_tag|

    .. code-block:: javascript

        var link = page.createLink([0,0,100,100], "https://example.com");



.. method:: deleteLink(link)

    Delete the link from the page.

    :arg link: :ref:`Link<mutool_object_link>`.

    |example_tag|

    .. code-block:: javascript

        page.deleteLink(link_obj);


.. method:: getLabel()

    Returns the page number as a string using the numbering scheme of the document.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var label = page.getLabel();



.. method:: isPDF()

    Returns *true* if the page is from a :title:`PDF` document.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isPDF = page.isPDF();


    .. note::

        As `PDFPage` extends `Page` this method will return **false**. It is only if we actually have an instance of a `PDFPage` when this method is overridden to return **true**.
