.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pixmap:


.. _mutool_run_js_api_pixmap:

`Pixmap`
----------------------------

A `Pixmap` object contains a color raster image (short for pixel map). The components in a pixel in the `Pixmap` are all byte values, with the transparency as the last component. A `Pixmap` also has a location (x, y) in addition to its size; so that they can easily be used to represent tiles of a page.


.. method:: new Pixmap(colorspace, bounds, alpha)

    *Constructor method*.

    Create a new pixmap. The pixel data is **not** initialized; and will contain garbage.

    :arg colorspace: `Colorspace`.
    :arg bounds: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg alpha: `Boolean`.

    :return: `Pixmap`.

    **Example**

    .. code-block:: javascript

        var pixmap = new Pixmap(DeviceRGB, [0,0,100,100], true);


.. method:: clear(value)

    Clear the pixels to the specified value. Pass `255` for white, or `undefined` for transparent.

    :arg value: Pixel value.

.. method:: bound()

    Return the pixmap bounds.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

.. method:: getWidth()

    :return: `Int` The width value.

.. method:: getHeight()

    :return: `Int` The height value.

.. method:: getNumberOfComponents()

    Number of colors; plus one if an alpha channel is present.

    :return: `Int` Number of color components.

.. method:: getAlpha()

    *True* if alpha channel is present.

    :return: `Boolean`.

.. method:: getStride()

    Number of bytes per row.

    :return: `Int`.

.. method:: getColorSpace()

    Returns the `ColorSpace` for the `Pixmap`.

    :return: `ColorSpace`.

.. method:: getXResolution()

    Returns the `x` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.


.. method:: getYResolution()

    Returns the `y` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.



.. method:: getSample(x, y, k)

    Get the value of component `k` at position `x`, `y` (relative to the image origin: 0, 0 is the top left pixel).

    :arg x: X co-ordinate.
    :arg y: Y co-ordinate.
    :arg k: Component.
    :return: `Int`.


.. method:: setResolution(xRes, yRes)

    Set `x` & `y` resolution.

    :arg xRes: `Int` X resolution in dots per inch.
    :arg yRes: `Int` Y resolution in dots per inch.


.. method:: saveAsPNG(fileName)

    Save the `Pixmap` as a :title:`PNG`. Only works for :title:`Gray` and :title:`RGB` images.

    :arg fileName: `String`.


.. method:: invert()

    Invert all pixels. All components are processed, except alpha which is unchanged.

.. method:: invertLuminance()

    Transform all pixels so that luminance of each pixel is inverted, and the chrominance remains as unchanged as possible. All components are processed, except alpha which is unchanged.

.. method:: gamma(gamma)

    Apply gamma correction to `Pixmap`. All components are processed, except alpha which is unchanged.

    Values `>= 0.1 & < 1` = darken, `> 1 & < 10` = lighten.

    :arg gamma: `Float`.

.. method:: tint(black, white)

    Tint all pixels in a :title:`RGB`, :title:`BGR` or :title:`Gray` `Pixmap`. Map black and white respectively to the given hex :title:`RGB` values.

    :arg black: `Integer`.
    :arg white: `Integer`.


.. method:: warp(points, width, height)

    Return a warped subsection of the `Pixmap`, where the result has the requested dimensions.

    :arg points: `[x0, y0, x1, y1, x2, y2, x3, y3, ...]` Points give the corner points of a convex quadrilateral within the `Pixmap` to be warped.
    :arg width: `Int` .
    :arg height: `Int`.
