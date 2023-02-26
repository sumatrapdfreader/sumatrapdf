.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_image:



.. _mutool_run_js_api_image:




`Image`
------------

`Image` objects are similar to `Pixmaps`, but can contain compressed data.


.. method:: new Image(ref)

    *Constructor method*.

    Create a new image from a `Pixmap` data, or load an image from a file.

    :return: `Image`.

    **Example**

    .. code-block:: javascript

        var image = new Image("my-image-file.png");


.. method:: getWidth()

    Get the image width in pixels.

    :return: The width value.

.. method:: getHeight()

    Get the image height in pixels.

    :return: The height value.

.. method:: getXResolution()

    Returns the x resolution for the `Image`.

    :return: `Int` Image resolution in dots per inch.


.. method:: getYResolution()

    Returns the y resolution for the `Image`.

    :return: `Int` Image resolution in dots per inch.

.. method:: getColorSpace()

    Returns the `ColorSpace` for the `Image`.

    :return: `ColorSpace`.



.. method:: getNumberOfComponents()

    Number of colors; plus one if an alpha channel is present.

    :return: `Integer`.


.. method:: getBitsPerComponent()

    Returns the number of bits per component.

    :return: `Integer`.


.. method:: getInterpolate()

    Returns *true* if interpolated was used during decoding.

    :return: `Boolean`.


.. method:: getImageMask()

    Returns *true* if this image is an image mask.

    :return: `Boolean`.

.. method:: getMask()

    Get another `Image` used as a mask for this one.

    :return: `Image` (or `null`).



.. method:: toPixmap(scaledWidth, scaledHeight)

    Create a `Pixmap` from the image. The `scaledWidth` and `scaledHeight` arguments are optional, but may be used to decode a down-scaled `Pixmap`.

    :arg scaledWidth: Width value.
    :arg scaledHeight: Height value.

    :return: `Pixmap`.
