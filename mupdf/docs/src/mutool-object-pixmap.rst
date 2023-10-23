.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_pixmap:

.. _mutool_run_js_api_pixmap:

`Pixmap`
----------------------------

A `Pixmap` object contains a color raster image (short for pixel map). The components in a pixel in the `Pixmap` are all byte values, with the transparency as the last component. A `Pixmap` also has a location (x, y) in addition to its size; so that they can easily be used to represent tiles of a page.


.. method:: new Pixmap(colorspace, bounds, alpha)

    *Constructor method*.

    Create a new pixmap. The pixel data is **not** initialized; and will contain garbage.

    :arg colorspace: `ColorSpace`.
    :arg bounds: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg alpha: `Boolean`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0,0,100,100], true);



|instance_methods|


.. method:: clear(value)

    Clear the pixels to the specified value. Pass `255` for white, or omit for transparent.

    :arg value: Pixel value.

    |example_tag|

    .. code-block:: javascript

        pixmap.clear(255);
        pixmap.clear();


.. method:: getBounds()

    Return the pixmap bounds.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        var rect = pixmap.getBounds();


.. method:: getWidth()

    :return: `Int` The width value.

    |example_tag|

    .. code-block:: javascript

        var w = pixmap.getWidth();

.. method:: getHeight()

    :return: `Int` The height value.

    |example_tag|

    .. code-block:: javascript

        var h = pixmap.getHeight();

.. method:: getNumberOfComponents()

    Number of colors; plus one if an alpha channel is present.

    :return: `Int` Number of color components.

    |example_tag|

    .. code-block:: javascript

        var num = pixmap.getNumberOfComponents();

.. method:: getAlpha()



    *True* if alpha channel is present.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var alpha = pixmap.getAlpha();

.. method:: getStride()

    Number of bytes per row.

    :return: `Int`.

    |example_tag|

    .. code-block:: javascript

        var stride = pixmap.getStride();

.. method:: getColorSpace()

    Returns the `ColorSpace` for the `Pixmap`.

    :return: `ColorSpace`.

    |example_tag|

    .. code-block:: javascript

        var cs = pixmap.getColorSpace();


.. method:: setResolution(xRes, yRes)

    Set `x` & `y` resolution.

    :arg xRes: `Int` X resolution in dots per inch.
    :arg yRes: `Int` Y resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        pixmap.setResolution(300, 300);


.. method:: getXResolution()

    Returns the `x` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var xRes = pixmap.getXResolution();


.. method:: getYResolution()

    Returns the `y` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var yRes = pixmap.getYResolution();


.. method:: getSample(x, y, index)

    |mutool_tag|

    Get the value of component ``index`` at position `x`, `y` (relative to the image origin: 0, 0 is the top left pixel).

    :arg x: X co-ordinate.
    :arg y: Y co-ordinate.
    :arg index: Component index. i.e. For CMYK ColorSpaces 0 = Cyan, for RGB 0 = Red etc.
    :return: `Int`.

    |example_tag|

    .. code-block:: javascript

        var sample = pixmap.getSample(0,0,0);





.. method:: saveAsPNG(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PNG`. Only works for :title:`Gray` and :title:`RGB` images.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPNG("fileName.png");


.. method:: saveAsJPEG(fileName, quality)

    |mutool_tag|

    Save the `Pixmap` as a :title:`JPEG`. Only works for :title:`Gray`, :title:`RGB` and :title:`CMYK` images.

    :arg fileName: `String`.
    :arg quality: `Int`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsJPEG("fileName.jpg", 80);


.. method:: saveAsPAM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PAM`.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPAM("fileName.pam");

.. method:: saveAsPNM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PNM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPNM("fileName.pnm");

.. method:: saveAsPBM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PBM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPBM("fileName.pbm");

.. method:: saveAsPKM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PKM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPKM("fileName.pkm");



.. method:: invert()

    Invert all pixels. All components are processed, except alpha which is unchanged.

    |example_tag|

    .. code-block:: javascript

        pixmap.invert();

.. method:: invertLuminance()

    Transform all pixels so that luminance of each pixel is inverted, and the chrominance remains as unchanged as possible. All components are processed, except alpha which is unchanged.

    |example_tag|

    .. code-block:: javascript

        pixmap.invertLuminance();

.. method:: gamma(gamma)

    Apply gamma correction to `Pixmap`. All components are processed, except alpha which is unchanged.

    Values `>= 0.1 & < 1` = darken, `> 1 & < 10` = lighten.

    :arg gamma: `Float`.

    |example_tag|

    .. code-block:: javascript

        pixmap.gamma(3);

.. method:: tint(black, white)

    Tint all pixels in a :title:`RGB`, :title:`BGR` or :title:`Gray` `Pixmap`. Map black and white respectively to the given hex :title:`RGB` values.

    :arg black: `Integer`.
    :arg white: `Integer`.

    |example_tag|

    .. code-block:: javascript

        pixmap.tint(0xffff00, 0xffff00);



.. method:: warp(points, width, height)

    |mutool_tag|

    Return a warped subsection of the `Pixmap`, where the result has the requested dimensions.

    :arg points: `[x0, y0, x1, y1, x2, y2, x3, y3, x4, y4]` Points give the corner points of a convex quadrilateral within the `Pixmap` to be warped.
    :arg width: `Int`.
    :arg height: `Int`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var warpedPixmap = pixmap.warp([0,0,100,0,0,100,100,100],200,200);


.. method:: convertToColorSpace(colorspace, proof, defaultColorSpaces, colorParams, keepAlpha)

    |mutool_tag|

    Convert pixmap into a new pixmap of a desired colorspace. A proofing colorspace, a set of default colorspaces and color parameters used during conversion may be specified. Finally a boolean indicates if alpha should be preserved (default is to not preserve alpha).

    :arg colorspace: `Colorspace`.
    :arg proof: `Colorspace`.
    :arg defaultColorSpaces: `DefaultColorSpaces`.
    :arg colorParams: `[]`.
    :arg keepAlpha: `Boolean`.

    :return: `Pixmap`.


    .. |tor_todo| Can't get any joy out of this one because of `DefaultColorSpaces` not working for me.


.. method:: getPixels()

    |wasm_tag|

    Returns an array of pixels for the `Pixmap`.


    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var pixels = pixmap.getPixels();


.. method:: asPNG()

    |wasm_tag|

    Returns a buffer of the `Pixmap` as a :title:`PNG`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPNG();



.. method:: asPSD()

    |wasm_tag|

    Returns a buffer of the `Pixmap` as a :title:`PSD`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPSD();


.. method:: asPAM()

    |wasm_tag|

    Returns a buffer of the `Pixmap` as a :title:`PAM`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPAM();



.. method:: asJPEG(quality)

    |wasm_tag|

    Returns a buffer of the `Pixmap` as a :title:`JPEG`. Note, if the `Pixmap` has an alpha channel then an exception will be thrown.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asJPEG(80);
