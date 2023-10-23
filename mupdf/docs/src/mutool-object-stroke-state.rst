.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_stroke_state:

.. _mutool_run_js_api_stroke_state:

`StrokeState`
--------------

A `StrokeState` object is used to define stroke styles.


.. method:: new StrokeState()


    *Constructor method*.

    Create a new empty stroke state object.

    :return: `StrokeState`.

    |example_tag|

    .. code-block:: javascript

        var strokeState = new mupdf.StrokeState();



|instance_methods|



.. method:: setLineCap(style)


    :arg style: `String` One of "Butt", "Round" or "Square".

    |example_tag|

    .. code-block:: javascript

        strokeState.setLineCap("Butt");


.. method:: getLineCap()


    :return: `String` One of "Butt", "Round" or "Square".

    |example_tag|

    .. code-block:: javascript

        var lineCap = strokeState.getLineCap();


.. method:: setLineJoin(style)


    :arg style: `String` One of "Miter", "Round" or "Bevel".

    |example_tag|

    .. code-block:: javascript

        strokeState.setLineJoin("Butt");


.. method:: getLineJoin()


    :return: `String` One of "Miter", "Round" or "Bevel".

    |example_tag|

    .. code-block:: javascript

        var lineJoin = strokeState.getLineJoin();


.. method:: setLineWidth(width)


    :arg width: `Integer`.

    |example_tag|

    .. code-block:: javascript

        strokeState.setLineWidth(2);


.. method:: getLineWidth()


    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var width = strokeState.getLineWidth();


.. method:: setMiterLimit(width)


    :arg width: `Integer`.

    |example_tag|

    .. code-block:: javascript

        strokeState.setMiterLimit(2);


.. method:: getMiterLimit()


    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var limit = strokeState.getMiterLimit();
