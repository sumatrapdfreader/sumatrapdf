.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_display_list_device:


.. _mutool_run_js_api_display_list_device:


`DisplayListDevice`
--------------------------------------------------------



.. method:: new DisplayListDevice(displayList)

    *Constructor method*.

    Create a device for recording onto a display list.

    :arg displayList: `DisplayList`.

    :return: `DisplayListDevice`.

    |example_tag|

    .. code-block:: javascript

        var my_display_list = new mupdf.DisplayList([0,0,100,100]);
        console.log("my_display_list="+my_display_list);
        var displayListDevice = new mupdf.DisplayListDevice(my_display_list);
