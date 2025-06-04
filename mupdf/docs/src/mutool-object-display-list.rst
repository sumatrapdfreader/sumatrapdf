.. _mutool_object_display_list:



.. _mutool_run_js_api_display_list:


`DisplayList`
------------------------

A display list records all the device calls for playback later. If you want to run a page through several devices, or run it multiple times for any other reason, recording the page to a display list and replaying the display list may be a performance gain since then you can avoid reinterpreting the page each time. Be aware though, that a display list will keep all the graphics required in memory, so will increase the amount of memory required.




.. method:: new DisplayList(mediabox)

    *Constructor method*.

    Create an empty display list. The mediabox rectangle should be the bounds of the page.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    :return: `DisplayList`.

    |example_tag|

    .. code-block:: javascript

        var displayList = new mupdf.DisplayList([0,0,100,100]);



|instance_methods|


.. method:: run(device, transform)

    Play back the recorded device calls onto the device.

    :arg device: `Device`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        displayList.run(device, mupdf.Matrix.identity);



.. method:: getBounds()

    Returns a rectangle containing the dimensions of the display list contents.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


    |example_tag|

    .. code-block:: javascript

        var bounds = displayList.getBounds();




.. method:: toPixmap(transform, colorspace, alpha)

    Render display list to a `Pixmap`.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`. If alpha is *true*, a transparent background, otherwise white.

    :return: `Pixmap`.


    |example_tag|

    .. code-block:: javascript

        var pixmap = displayList.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, false);


.. method:: toStructuredText(options)


    Extract the text on the page into a `StructuredText` object. The options argument is a comma separated list of flags: "preserve-ligatures", "preserve-whitespace", "preserve-spans", and "preserve-images".

    :arg options: `String`.
    :return: `StructuredText`.

    |example_tag|

    .. code-block:: javascript

        var sText = displayList.toStructuredText("preserve-whitespace");



.. method:: search(needle, max_hits)


    Search the display list text for all instances of the `needle` value,
    and return an array of search hits.
    Each search hit is an array of :ref:`rectangles<mutool_run_js_api_quad>`
    corresponding to all characters in the search hit.

    :arg needle: `String`.
    :arg max_hits: `Integer` Use to limit number of results, defaults to 500.
    :return: `[ [ Quad, Quad, ... ], [ Quad, Quad, ...], ... ]`.


    |example_tag|

    .. code-block:: javascript

        var results = displayList.search("my search phrase");



.. method:: decodeBarcode(subarea, rotate)

    |mutool_tag|

    Decodes a barcode detected in the pixmap, and returns an object with properties for barcode type and contents.

    :arg subarea: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>` Only detect barcode within subarea.
    :arg rotate: `Integer` Degrees of rotation to rotate pixmap before detecting barcode.

    :return: :ref:`BarcodeInfo<mutool_run_js_api_object_barcode_info>`.

    |example_tag|

    .. code-block:: javascript

        var barcodeInfo = displaylist.decodeBarcode([0, 0, 100, 100 ], 0);
