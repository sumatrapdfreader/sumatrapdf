.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_draw_device:


.. _mutool_run_js_api_draw_device:


`DrawDevice`
----------------------------

The `DrawDevice` can be used to render to a :ref:`Pixmap<mutool_run_js_api_pixmap>`; either by running a :ref:`Page<mutool_run_js_api_page>` with it or by calling its methods directly.


.. method:: new DrawDevice(transform, pixmap)

    *Constructor method*.

    Create a device for drawing into a `Pixmap`. The `Pixmap` bounds used should match the transformed page bounds, or you can adjust them to only draw a part of the page.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg pixmap: `Pixmap`.

    :return: `DrawDevice`.

    |example_tag|

    .. code-block:: javascript

        var drawDevice = new mupdf.DrawDevice(mupdf.Matrix.identity, pixmap);
