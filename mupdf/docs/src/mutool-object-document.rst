.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_document:


.. _mutool_run_js_api_document:


`Document`
--------------------

:title:`MuPDF` can open many document types (:title:`PDF`, :title:`XPS`, :title:`CBZ`, :title:`EPUB`, :title:`FB2` and a handful of image formats).




.. method:: new Document(fileName)

    *Constructor method*.

    Open the named document.

    :arg fileName: Filename to open.


    :return: `Document`.


    **Example**

    .. code-block:: javascript

        var document = new Document("my_pdf.pdf");



.. method:: needsPassword()

    Returns *true* if a password is required to open this password protected PDF.

    :return: `Boolean`.


.. method:: authenticatePassword(password)

    Returns *true* if the password matches.

    :arg password: The password to attempt authentication with.
    :return: `Boolean`.


.. method:: hasPermission(permission)

    Returns *true* if the document has permission for "print", "annotate", "edit" or "copy".

    :arg permission: `String` The permission to seek for, e.g. "edit".
    :return: `Boolean`.


.. method:: getMetaData(key)

    Return various meta data information. The common keys are: `format`, `encryption`, `info:Author`, and `info:Title`.

    :arg key: `String`.
    :return: `String`.


.. method:: setMetaData(key, value)

    Set document meta data information field to a new value.

    :arg key: `String`.
    :arg value: `String`.

.. method:: isReflowable()

    Returns true if the document is reflowable, such as :title:`EPUB`, :title:`FB2` or :title:`XHTML`.

    :return: `Boolean`.


.. method:: layout(pageWidth, pageHeight, fontSize)

    Layout a reflowable document (:title:`EPUB`, :title:`FB2`, or :title:`XHTML`) to fit the specified page and font size.

    :arg pageWidth: `Int`.
    :arg pageHeight: `Int`.
    :arg fontSize: `Int`.

.. method:: countPages()

    Count the number of pages in the document. This may change if you call the layout function with different parameters.

    :return: `Int`.

.. method:: loadPage(number)

    Returns a `Page` (or `PDFPage`) object for the given page number. Page number zero (0) is the first page in the document.

    :return: `Page` or `PDFPage`.

.. method:: loadOutline()

    Returns an array with the outline (also known as "table of contents" or "bookmarks"). In the array is an object for each heading with the property 'title', and a property 'page' containing the page number. If the object has a 'down' property, it contains an array with all the sub-headings for that entry.

    :return: `[]`.



.. method:: outlineIterator()

    Returns an :ref:`OutlineIterator<mutool_object_outline_iterator>` for the document outline.

    :return: `OutlineIterator`.


.. _mutool_run_js_api_document_resolveLink:


.. method:: resolveLink(uri)

    Resolve a document internal link :title:`URI` to a link destination.

    :arg uri: `String`.
    :return: :ref:`Link destination<mutool_run_js_api_link_dest>`.


.. method:: formatLinkURI(linkDestination)

    Format a document internal link destination object to a :title:`URI` string suitable for :ref:`createLink()<mutool_run_js_api_page_create_link>`.

    :arg linkDestination: :ref:`Link destination<mutool_run_js_api_link_dest>`.
    :return: `String`.


.. method:: isPDF()

    Returns *true* if the document is a :title:`PDF` document.

    :return: `Boolean`.
