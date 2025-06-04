.. _mutool_object_link:



.. _mutool_run_js_api_link:




`Link`
------------

`Link` objects contain information about page links.


.. method:: getBounds()

    Returns a :ref:`rectangle<mutool_run_js_api_rectangle>` describing the link's location on the page.

    :return: `[ulx,uly,lrx,lry]`.

    |example_tag|

    .. code-block:: javascript

        var rect = link.getBounds();


.. method:: getURI()

    Returns a string URI describing the link's destination. If :ref:`isExternal<mutool_run_js_api_link_isExternal>` returns *true*, this is a URI for a suitable browser, if it returns *false* pass it to :ref:`resolveLink<mutool_run_js_api_document_resolveLink>` to access to the destination page in the document.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var uri = link.getURI();


.. method:: setBounds(rect)

    Set a :ref:`rectangle<mutool_run_js_api_rectangle>` describing the link's location on the page.

    :arg rect: `[ulx,uly,lrx,lry]`.

    |example_tag|

    .. code-block:: javascript

        link.setBounds([0, 0, 100, 100]);


.. method:: setURI(uri)

    Set the URI describing the link's destination from the given uri string.

    :arg url: `String` The new URI for the link.

    |example_tag|

    .. code-block:: javascript

        link.setURI("https://artifex.com");




.. _mutool_run_js_api_link_isExternal:
.. method:: isExternal()

    Returns a boolean indicating if the link is external or not. If the link URI has a valid scheme followed by `:` then it considered to be external, e.g. https://example.com.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isExternal = link.isExternal();
