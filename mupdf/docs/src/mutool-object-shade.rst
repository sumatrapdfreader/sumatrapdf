.. _mutool_object_shade:

.. _mutool_run_js_api_shade:

`Shade`
--------------

A `Shade` object is used to define shadings.




|instance_methods|



.. method:: getBounds()


    Returns a rectangle containing the dimensions of the shading contents.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


    |example_tag|

    .. code-block:: javascript

        var bounds = shade.getBounds();
