.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

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



.. _mutool_run_js_api_link_isExternal:
.. method:: isExternal()

    |wasm_tag|

    Returns a boolean indicating if the link is external or not. If the link URI has a valid scheme followed by `:` then it considered to be external, e.g. https://example.com.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isExternal = link.isExternal();
