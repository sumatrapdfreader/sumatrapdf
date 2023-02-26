.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_graft_map:



.. _mutool_run_js_api_pdf_graft_map:


`PDFGraftMap`
----------------

.. method:: graftObject(object)

    Use the graft map to copy objects, with the ability to remember previously copied objects.

    :arg object: Object to graft.



.. method:: graftPage(map, dstPageNumber, srcDoc, srcPageNumber)

    Graft a page at the given page number from the source document to the requested page number in the destination document connected to the map.

    :arg map: `PDFGraftMap`.
    :arg dstPageNumber: Destination page number.
    :arg srcDoc: Source document.
    :arg srcPageNumber: Source page number.
