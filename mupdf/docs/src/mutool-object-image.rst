.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_image:

.. _mutool_run_js_api_image:

`Image`
------------

`Image` objects are similar to `Pixmaps`, but can contain compressed data.


.. method:: new Image(ref)

    *Constructor method*.

    Create a new image from a `Pixmap` data, or load an image file data.

    :return: `Image`.

    |example_tag|

    .. code-block:: javascript

        var imageFromPixmap = new mupdf.Image(pixmap);
        var imageFromBuffer = new mupdf.Image(buffer);


|instance_methods|


.. method:: getWidth()

    Get the image width in pixels.

    :return: The width value.

    |example_tag|

    .. code-block:: javascript

        var width = image.getWidth();


.. method:: getHeight()

    Get the image height in pixels.

    :return: The height value.

    |example_tag|

    .. code-block:: javascript

        var height = image.getHeight();

.. method:: getXResolution()

    Returns the x resolution for the `Image`.

    :return: `Int` Image resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var xRes = image.getXResolution();


.. method:: getYResolution()

    Returns the y resolution for the `Image`.

    :return: `Int` Image resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var yRes = image.getYResolution();


.. method:: getColorSpace()

    Returns the `ColorSpace` for the `Image`.

    :return: `ColorSpace`.

    |example_tag|

    .. code-block:: javascript

        var cs = image.getColorSpace();


.. method:: getNumberOfComponents()

    Number of colors; plus one if an alpha channel is present.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var num = image.getNumberOfComponents();


.. method:: getBitsPerComponent()

    Returns the number of bits per component.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var bits = image.getBitsPerComponent();


.. method:: getInterpolate()

    Returns *true* if interpolated was used during decoding.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var interpolate = image.getInterpolate();


.. method:: getColorKey()

    Returns an array with 2 * N integers for an N component image with color key masking, or `null` if masking is not used. Each pair of integers define an interval, and component values within that interval are not painted.

    :return: `[...]` or `null`.


    |example_tag|

    .. code-block:: javascript

        var result = image.getColorKey();

.. method:: getDecode()

    Returns an array with 2 * N numbers for an N component image with color mapping, or `null` if mapping is not used. Each pair of numbers define the lower and upper values to which the component values are mapped linearly.

    :return: `[...]` or `null`.

    |example_tag|

    .. code-block:: javascript

        var arr = image.getDecode();


.. method:: getOrientation()

    Returns the orientation of the image.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var orientation = image.getOrientation();

.. method:: setOrientation(orientation)

    Set the image orientation to the given orientation.

    :arg orientation: `Integer` Orientation value from the table below:


    .. list-table::
       :header-rows: 0

       * - **0**
         - Undefined
       * - **1**
         - 0 degree ccw rotation. (Exif = 1)
       * - **2**
         - 90 degree ccw rotation. (Exif = 8)
       * - **3**
         - 180 degree ccw rotation. (Exif = 3)
       * - **4**
         - 270 degree ccw rotation. (Exif = 6)
       * - **5**
         - flip on X. (Exif = 2)
       * - **6**
         - flip on X, then rotate ccw by 90 degrees. (Exif = 5)
       * - **7**
         - flip on X, then rotate ccw by 180 degrees. (Exif = 4)
       * - **8**
         - flip on X, then rotate ccw by 270 degrees. (Exif = 7)

    |example_tag|

    .. code-block:: javascript

        var orientation = image.setOrientation(4);


.. method:: getImageMask()

    Returns *true* if this image is an image mask.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var mask = image.getImageMask();


.. method:: getMask()

    Get another `Image` used as a mask for this one.

    :return: `Image` (or `null`).

    |example_tag|

    .. code-block:: javascript

        var img = image.getMask();



.. method:: toPixmap(scaledWidth, scaledHeight)

    Create a `Pixmap` from the image. The `scaledWidth` and `scaledHeight` arguments are optional, but may be used to decode a down-scaled `Pixmap`.

    :arg scaledWidth: `Float`.
    :arg scaledHeight: `Float`.

    :return: `Pixmap`.


    |example_tag|

    .. code-block:: javascript

        var pixmap = image.toPixmap();
        var scaledPixmap = image.toPixmap(100, 100);
