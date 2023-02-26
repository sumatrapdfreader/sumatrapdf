.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js


.. _mutool_object_document_writer:



.. _mutool_run_js_api_document_writer:




`DocumentWriter`
---------------------

`DocumentWriter` objects are used to create new documents in several formats.


.. method:: new DocumentWriter(filename, format, options)

    *Constructor method*.

    Create a new document writer to create a document with the specified format and output options. If format is `null` it is inferred from the `filename` extension. The `options` argument is a comma separated list of flags and key-value pairs.

    The output `format` & `options` are the same as in the :ref:`mutool convert<mutool_convert>` command.


    :arg filename: The file name to output to.
    :arg format: The file format.
    :arg options: The options as key-value pairs.
    :return: `DocumentWriter`.

    **Example**

    .. code-block:: javascript

        var writer = new DocumentWriter("out.pdf", "PDF", "");



.. method:: beginPage(mediabox)

    Begin rendering a new page. Returns a `Device` that can be used to render the page graphics.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    :return: `Device`.


.. method:: endPage(device)

    Finish the page rendering. The argument must be the same `Device` object that was returned by the `beginPage` method.

    :arg mediabox: `Device`.

.. method:: close()

    Finish the document and flush any pending output.
