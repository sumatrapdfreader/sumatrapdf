.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js


.. _mutool_object_page:

.. _mutool_run_js_api_page:

`Page`
-------------

.. method:: bound()

    Returns a :ref:`rectangle<mutool_run_js_api_rectangle>` containing the page dimensions.

    :return: `[ulx,uly,lrx,lry]`.

.. method:: run(device, transform, skipAnnotations)

    Calls device functions for all the contents on the page, using the specified `transform` :ref:`matrix<mutool_run_js_api_matrix>`. The `device` can be one of the built-in devices or a :title:`JavaScript` object with methods for the device calls. The `transform` maps from user space points to device space pixels. If `skipAnnotations` is *true* then annotations are ignored.

    :arg device: The device object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg skipAnnotations: `Boolean`.

.. method:: toPixmap(transform, colorspace, alpha, skipAnnotations)

    Render the page into a :ref:`Pixmap<mutool_run_js_api_pixmap>`, using the `transform` and `colorspace`. If `alpha` is *true*, the page will be drawn on a transparent background, otherwise white.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.
    :arg skipAnnotations: `Boolean`.

.. method:: toDisplayList(skipAnnotations)

    Record the contents on the page into a `DisplayList`.

    :arg skipAnnotations: `Boolean`.

.. method:: toStructuredText(options)

    Extract the text on the page into a `StructuredText` object. The options argument is a comma separated list of flags: "preserve-ligatures", "preserve-whitespace", "preserve-spans", and "preserve-images".

    :arg options: `String`.
    :return: `StructuredText`.



.. method:: search(needle)

    Search for `needle` text on the page, and return an array with :ref:`rectangles<mutool_run_js_api_rectangle>` of all matches found.

    :arg needle: `String`.
    :return: `[]`.



.. method:: getLinks()

    Return an array of all the links on the page. Each link is an object with a 'bounds' property, and either a 'page' or 'uri' property, depending on whether it's an internal or external link. See: :ref:`Links<mutool_run_js_api_links>`.

    :return: `[]`.


.. _mutool_run_js_api_page_create_link:


.. method:: createLink(rect, destinationUri)

    Create a new link within the rectangle on the page, linking to the destination URI string.

    :arg rect: :ref:`Rectangle<mutool_run_js_api_rectangle>` for the link.
    :arg destinationUri: `String`.
    :return: `Object` :ref:`Link dictionary<mutool_run_js_api_link_dict>`.

    **Example**

    .. code-block:: javascript

        var link = page.createLink([0,0,100,100],"http://mupdf.com");


.. method:: deleteLink(link)

    Delete the link from the page.

    :arg link: `Object` :ref:`Link dictionary<mutool_run_js_api_link_dict>`.

.. method:: isPDF()

    Returns *true* if the page is from a :title:`PDF` document.

    :return: `Boolean`.
