.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_document_writer:



.. _mutool_run_js_api_document_writer:



`DocumentWriter`
---------------------

`DocumentWriter` objects are used to create new documents in several formats.


.. method:: new DocumentWriter(filename, format, options)

    |mutool_tag|

    *Constructor method*.

    Create a new document writer to create a document with the specified format and output options. If format is `null` it is inferred from the `filename` extension. The `options` argument is a comma separated list of flags and key-value pairs.

    The output `format` & `options` are the same as in the :ref:`mutool convert<mutool_convert>` command.


    :arg filename: The file name to output to.
    :arg format: The file format.
    :arg options: The options as key-value pairs.
    :return: `DocumentWriter`.

    |example_tag|

    .. code-block:: javascript

        var writer = new mupdf.DocumentWriter("out.pdf", "PDF", "");



.. method:: new DocumentWriter(buffer, format, options)

    |wasm_tag|

    *Constructor method*.

    Create a new document writer to create a document with the specified format and output options. The `options` argument is a comma separated list of flags and key-value pairs.

    The output `format` & `options` are the same as in the :ref:`mutool convert<mutool_convert>` command.


    :arg buffer: The buffer to output to.
    :arg format: The file format.
    :arg options: The options as key-value pairs.
    :return: `DocumentWriter`.

    |example_tag|

    .. code-block:: javascript

        var writer = new mupdf.DocumentWriter(buffer, "PDF", "");




|instance_methods|


.. method:: beginPage(mediabox)

    Begin rendering a new page. Returns a `Device` that can be used to render the page graphics.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    :return: `Device`.

    |example_tag|

    .. code-block:: javascript

        var device = writer.beginPage([0,0,100,100]);


.. method:: endPage(device)

    Finish the page rendering. The argument must be the same `Device` object that was returned by the `beginPage` method.

    :arg device: `Device`.

    |example_tag|

    .. code-block:: javascript

        writer.endPage(device);


.. method:: close()

    Finish the document and flush any pending output.

    |example_tag|

    .. code-block:: javascript

        writer.close();
