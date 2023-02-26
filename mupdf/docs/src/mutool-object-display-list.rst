.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js


.. _mutool_object_display_list:



.. _mutool_run_js_api_display_list:


`DisplayList`
------------------------

A display list records all the device calls for playback later. If you want to run a page through several devices, or run it multiple times for any other reason, recording the page to a display list and replaying the display list may be a performance gain since then you can avoid reinterpreting the page each time. Be aware though, that a display list will keep all the graphics required in memory, so will increase the amount of memory required.




.. method:: new DisplayList(mediabox)

    *Constructor method*.

    Create an empty display list. The mediabox rectangle has the bounds of the page in points.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    :return: `DisplayList`.

    **Example**

    .. code-block:: javascript

        var displayList = new DisplayList([0,0,100,100]);



.. method:: run(device, transform)

    Play back the recorded device calls onto the device.

    :arg device: `Device`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.


.. method:: bound()

    Returns a rectangle containing the dimensions of the display list contents.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: toPixmap(transform, colorspace, alpha)

    Render display list to a `Pixmap`.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`. If alpha is *true*, the annotation will be drawn on a transparent background, otherwise white.



.. method:: toStructuredText(options)


    Extract the text on the page into a `StructuredText` object. The options argument is a comma separated list of flags: "preserve-ligatures", "preserve-whitespace", "preserve-spans", and "preserve-images".

    :arg options: `String`.
    :return: `StructuredText`.


.. method:: search(needle)

    Search the display list text for all instances of 'needle', and return an array with :ref:`rectangles<mutool_run_js_api_rectangle>` of all matches found.

    :arg needle: `String`.
    :return: `[]`.
