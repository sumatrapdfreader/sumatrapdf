.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_device:

.. _mutool_run_js_api_device:


`Device`
------------------

All built-in devices have the methods listed below. Any function that accepts a device will also accept a :title:`JavaScript` object with the same methods. Any missing methods are simply ignored, so you only need to create methods for the device calls you care about.

Many of the methods take graphics objects as arguments: `Path`, `Text`, `Image` and `Shade`.

Colors are specified as arrays with the appropriate number of components for the color space.

The methods that clip graphics must be balanced with a corresponding `popClip`.




|instance_methods|

.. method:: fillPath(path, evenOdd, transform, colorspace, color, alpha, colorParams)


    Fill a path.

    :arg path: `Path` object.
    :arg evenOdd: The `even odd rule`_ to use.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.


    |example_tag|

    .. code-block:: javascript

        device.fillPath(path, false, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1,0,0], true);


.. method:: strokePath(path, stroke, transform, colorspace, color, alpha, colorParams)


    Stroke a path.

    :arg path: `Path` object.
    :arg stroke: `StrokeState` The :ref:`stroke state object<mutool_object_stroke_state>`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.

    |example_tag|

    .. code-block:: javascript

        device.strokePath(path,
                          {dashes:[5,10], lineWidth:3, lineCap:'Round'},
                          mupdf.Matrix.identity,
                          mupdf.ColorSpace.DeviceRGB,
                          [0,1,0],
                          0.5);



.. method:: clipPath(path, evenOdd, transform)


    Clip a path.

    :arg path: `Path` object.
    :arg evenOdd: The `even odd rule`_ to use.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.


    |example_tag|

    .. code-block:: javascript

        device.clipPath(path, true, mupdf.Matrix.identity);



.. method:: clipStrokePath(path, stroke, transform)


    Clip & stroke a path.

    :arg path: `Path` object.
    :arg stroke: `StrokeState` The :ref:`stroke state object<mutool_object_stroke_state>`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        device.clipStrokePath(path, true, mupdf.Matrix.identity);




.. method:: fillText(text, transform, colorspace, color, alpha, colorParams)


    Fill a text object.

    :arg text: `Text` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.

    |example_tag|

    .. code-block:: javascript

        device.fillText(text, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1,0,0], 1);

.. method:: strokeText(text, stroke, transform, colorspace, color, alpha, colorParams)


    Stroke a text object.

    :arg text: `Text` object.
    :arg stroke: `StrokeState` The :ref:`stroke state object<mutool_object_stroke_state>`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.

    |example_tag|

    .. code-block:: javascript

        device.strokeText(text,
                          {dashes:[5,10], lineWidth:3, lineCap:'Round'},
                          mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB,
                          [1,0,0],
                          1);


.. method:: clipText(text, transform)


    Clip a text object.

    :arg text: `Text` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        device.clipText(text, mupdf.Matrix.identity);


.. method:: clipStrokeText(text, stroke, transform)


    Clip & stroke a text object.

    :arg text: `Text` object.
    :arg stroke: `StrokeState` The :ref:`stroke state object<mutool_object_stroke_state>`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        device.clipStrokeText(text, {dashes:[5,10], lineWidth:3, lineCap:'Round'},  mupdf.Matrix.identity);



.. method:: ignoreText(text, transform)


    Invisible text that can be searched but should not be visible, such as for overlaying a scanned OCR image.

    :arg text: `Text` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        device.ignoreText(text, mupdf.Matrix.identity);



.. method:: fillShade(shade, transform, alpha, colorParams)

    Fill a shade (a.k.a. gradient).

    .. note::

        The details of gradient fills are not exposed to :title:`JavaScript` yet.


    :arg shade: The gradient.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.


    |example_tag|

    .. code-block:: javascript

        device.fillShade(shade, mupdf.Matrix.identity, true, {overPrinting:true});



.. method:: fillImage(image, transform, alpha, colorParams)


    Draw an image. An image always fills a unit rectangle `[0,0,1,1]`, so must be transformed to be placed and drawn at the appropriate size.

    :arg image: `Image` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.


    |example_tag|

    .. code-block:: javascript

        device.fillImage(image, mupdf.Matrix.identity, false, {overPrinting:true});



.. method:: fillImageMask(image, transform, colorspace, color, alpha, colorParams)


    An image mask is an image without color. Fill with the color where the image is opaque.

    :arg image: `Image` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.


    |example_tag|

    .. code-block:: javascript

        device.fillImageMask(image, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, 0xff00ff, true, {});



.. method:: clipImageMask(image, transform)


    Clip graphics using the image to mask the areas to be drawn.

    :arg image: `Image` object.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.


    |example_tag|

    .. code-block:: javascript

        device.clipImageMask(image, mupdf.Matrix.identity);


.. method:: popClip()

    Pop the clip mask installed by the last clipping operation.

    |example_tag|

    .. code-block:: javascript

        device.popClip();


.. method:: beginMask(area, luminosity, backdropColorspace, backdropColor, backdropAlpha, colorParams)


    Create a soft mask. Any drawing commands between `beginMask` and `endMask` are grouped and used as a clip mask.

    :arg area: `Path` Mask area.
    :arg luminosity: `Boolean` If luminosity is *true*, the mask is derived from the luminosity (grayscale value) of the graphics drawn; otherwise the color is ignored completely and the mask is derived from the alpha of the group.
    :arg backdropColorspace: The :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg backdropColor: The :ref:`color value<mutool_run_js_api_colors>`.
    :arg backdropAlpha: The  :ref:`alpha value<mutool_run_js_api_alpha>`.
    :arg colorParams: The :ref:`color parameters object<mutool_run_js_api_color_params>`.


    |example_tag|

    .. code-block:: javascript

        device.beginMask(path, true, mupdf.ColorSpace.DeviceRGB, 0xff00ff, false, {});



.. method:: endMask()

    Ends the mask.

    |example_tag|

    .. code-block:: javascript

        device.endMask();



.. method:: beginGroup(area, colorspace, isolated, knockout, blendmode, alpha)


    Push/pop a transparency blending group. See the PDF reference for details on `isolated` and `knockout`.

    :arg area: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`. The blend area.
    :arg colorspace: :ref:`ColorSpace<mutool_run_javascript_api_colorspace>`.
    :arg isolated: `Boolean`.
    :arg knockout: `Boolean`.
    :arg blendmode: Blendmode is one of the standard :title:`PDF` blend modes: "Normal", "Multiply", "Screen", etc.
    :arg alpha: The :ref:`alpha value<mutool_run_js_api_alpha>`.


    .. image:: images/isolated-and-knockout.png
       :align: center
       :scale: 50%


    |example_tag|

    .. code-block:: javascript

        device.beginGroup([0,0,100,100], mupdf.ColorSpace.DeviceRGB, true, true, "Multiply", 0.5);



.. method:: endGroup()

    Ends the blending group.

    |example_tag|

    .. code-block:: javascript

        device.endGroup();


.. method:: beginTile(areaRect, viewRect, xStep, yStep, transform, id)

    Draw a tiling pattern. Any drawing commands between `beginTile` and `endTile` are grouped and then repeated across the whole page. Apply a clip mask to restrict the pattern to the desired shape.

    :arg areaRect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg viewRect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg xStep: `Integer` representing `x` step.
    :arg yStep: `Integer` representing `y` step.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg id: `Integer` The purpose of `id` is to allow for efficient caching of rendered tiles. If `id` is `0`, then no caching is performed. If it is non-zero, then it assumed to uniquely identify this tile.


    |example_tag|

    .. code-block:: javascript

        device.beginTile([0,0,100,100], [100,100,200,200], 10, 10, mupdf.Matrix.identity, 0);


.. method:: endTile()

    Ends the tiling pattern.

    |example_tag|

    .. code-block:: javascript

        device.endTile();


.. method:: beginLayer(tag)

    Begin a marked-content layer with the given tag.

    :arg tag: `String`.

    |example_tag|

    .. code-block:: javascript

        device.beginLayer("my tag");


.. method:: endLayer()

    End a marked-content layer.

    |example_tag|

    .. code-block:: javascript

        device.endLayer();


.. method:: renderFlags(set, clear)

    |mutool_tag_wasm_soon|

    Set/clear device rendering flags. Both set and clear are arrays where each element is a flag name:

    `"mask"`, `"color"`, `"uncacheable"`, `"fillcolor-undefined"`, `"strokecolor-undefined"`, `"startcap-undefined"`,
    `"dashcap-undefined"`, `"endcap-undefined"`, `"linejoin-undefined"`, `"miterlimit-undefined"`,
    `"linewidth-undefined"`, `"bbox-defined"`, or `"gridfit-as-tiled"`.

    :arg set: `[]`.
    :arg clear: `[]`.

    |example_tag|

    .. code-block:: javascript

        device.renderFlags(["mask","startcap-undefined"], []);

    .. |tor_todo| TypeError: device.renderFlags is not a function


.. method:: setDefaultColorSpaces(defaults)

    Change the set of default colorspaces for the device. See the :ref:`DefaultColorSpaces<mutool_object_default_color_spaces>` object.

    :arg defaults: `Object`.


    |example_tag|

    .. code-block:: javascript


    .. |tor_todo| Ask Tor, how to create a default color space object.


.. method:: beginStructure(standard, raw, uid)

    |mutool_tag_wasm_soon|

    Begin a standard structure element, the raw tag name and a unique identifier.

    :arg standard: `String`. One of the standard :title:`PDF` structure names: "Document", "Part", "BlockQuote", etc.
    :arg raw: `String`. The tag name.
    :arg uid: `Integer`. A unique identifier.

    |example_tag|

    .. code-block:: javascript

        device.beginStructure("Document", "my_tag_name", 123);

    .. |tor_todo| TypeError: device.beginStructure is not a function


.. method:: endStructure()

    |mutool_tag_wasm_soon|

    End a standard structure element.

    |example_tag|

    .. code-block:: javascript

        device.endStructure();


    .. |tor_todo| TypeError: device.endStructure is not a function


.. method:: beginMetatext(type, text)

    |mutool_tag_wasm_soon|

    Begin meta text information.

    :arg type: `String`. The type (either of `"ActualText"`, `"Alt"`, `"Abbreviation"`, or `"Title"`)
    :arg text: `String`. The text value.


    |example_tag|

    .. code-block:: javascript

        device.beginMetatext("Title", "My title");


    .. |tor_todo| WASM: TypeError: device.beginMetatext is not a function


.. method:: endMetatext()

    |mutool_tag_wasm_soon|

    End meta text information.

    |example_tag|

    .. code-block:: javascript

        device.endMetatext();


    .. |tor_todo| WASM: TypeError: device.endMetatext is not a function


.. method:: close()

    Tell the device that we are done, and flush any pending output. Ensure that no items are left on the stack before closing.


    |example_tag|

    .. code-block:: javascript

        device.close();



.. External links:

.. _even odd rule: https://en.wikipedia.org/wiki/Even–odd_rule
