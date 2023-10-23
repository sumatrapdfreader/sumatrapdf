.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_pdf_graft_map:



.. _mutool_run_js_api_pdf_graft_map:


`PDFGraftMap`
----------------

|mutool_tag|

|instance_methods|

.. method:: graftObject(object)

    Use the graft map to copy objects, with the ability to remember previously copied objects.

    :arg object: Object to graft.

    |example_tag|

    .. code-block:: javascript

        var map = document.newGraftMap();
        map.graftObject(obj);



.. method:: graftPage(map, dstPageNumber, srcDoc, srcPageNumber)

    Graft a page and its resources at the given page number from the source document to the requested page number in the destination document connected to the map.

    :arg dstPageNumber: The page number where the source page will be inserted. Page numbers start at `0`, and `-1` means at the end of the document.
    :arg srcDoc: Source document.
    :arg srcPageNumber: Source page number.

    |example_tag|

    .. code-block:: javascript

        var map = dstdoc.newGraftMap();
        map.graftObject(-1, srcdoc, 0);
