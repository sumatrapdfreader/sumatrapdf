.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_structured_text:




.. _mutool_run_js_api_structured_text:


`StructuredText`
----------------------------

`StructuredText` objects hold text from a page that has been analyzed and grouped into blocks, lines and spans.

.. method:: search(needle)

    Search the text for all instances of `needle`, and return an array with :ref:`rectangles<mutool_run_js_api_rectangle>` of all matches found.

    :arg needle: `String`.
    :return: `[]`.

.. method:: highlight(p, q)

    Return an array with :ref:`rectangles<mutool_run_js_api_rectangle>` needed to highlight a selection defined by the start and end points.

    :arg p: Start point in format `{x:y}`.
    :arg q: End point in format `{x:y}`.

.. method:: copy(p, q)

    Return the text from the selection defined by the start and end points.

    :arg p: Start point in format `{x:y}`.
    :arg q: End point in format `{x:y}`.
