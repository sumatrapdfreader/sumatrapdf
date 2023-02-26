.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js


.. _mutool_object_story:

.. _mutool_run_js_api_object_story:


`Story`
-------------


.. method:: new Story(contents, userCSS, em, archive)

    *Constructor method*.

    Create a new story with the given `contents`, formatted according to the provided `userCSS` and `em` size, and an `archive` to lookup images, etc.

    :arg contents: `String` :title:`HTML` source code. If omitted, a basic minimum is generated.
    :arg userCSS: `String` :title:`CSS` source code. If provided, must contain valid :title:`CSS` specifications.
    :arg em: `Float` The default text font size.

    :arg archive: An `Archive` from which to load resources for rendering. Currently supported resource types are images and text fonts. If omitted, the `Story` will not try to look up any such data and may thus produce incomplete output.


    **Example**

    .. code-block:: javascript

        var story = new Story(<contents>, <css>, <em>, <archive>);

.. method:: document()

    Return an `XML` for an unplaced story. This allows adding content before placing the `Story`.

    :return: :ref:`XML<mutool_object_xml>`.

.. method:: place(rect)

    Place (or continue placing) a `Story` into the supplied rectangle, returning a :ref:`Placement Result Object<mutool_run_js_api_object_story_placement_result_object>`. Call `draw()` to draw the placed content before calling `place()` again to continue placing remaining content.

    :arg rect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: draw(device, transform)

    Draw the placed `Story` to the given `device` with the given `transform`.

    :arg device: :ref:`Device<mutool_object_device>`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
