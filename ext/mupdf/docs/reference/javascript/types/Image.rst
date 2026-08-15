.. default-domain:: js

.. highlight:: javascript

Image
=====

Constructors
------------

.. class::
	Image(pixmap, mask)
	Image(data, mask)
	Image(filename, mask)

	Create an Image and populate it with image data decoded either from a pixmap,
	a data buffer, or a file.

	:param Pixmap pixmap: A pixmap with the image data.
	:param Buffer | ArrayBuffer | Uint8Array data: Image data.
	:param string filename: Image file to load.
	:param Image mask: An optional image mask.

	.. code-block::

		var image1 = new mupdf.Image(pixmap, imagemask)
		var image2 = new mupdf.Image(buffer, imagemask)
		var image3 = new mupdf.Image("logo.png", imagemask)

Instance methods
----------------

.. method:: Image.prototype.getWidth()

	Get the image width in pixels.

	:returns: number

	.. code-block::

		var width = image.getWidth()

.. method:: Image.prototype.getHeight()

	Get the image height in pixels.

	:returns: number

	.. code-block::

		var height = image.getHeight()

.. method:: Image.prototype.getXResolution()

	Returns the x resolution for the `Image` in dots per inch.

	:returns: number

	.. code-block::

		var xRes = image.getXResolution()

.. method:: Image.prototype.getYResolution()

	Returns the y resolution for the `Image` in dots per inch.

	:returns: number

	.. code-block::

		var yRes = image.getYResolution()

.. method:: Image.prototype.getColorSpace()

	Returns the `ColorSpace` for the `Image`. Returns null if the image has
	no colors (for example if it is an opacity mask with only an alpha
	channel).

	:returns: `ColorSpace` | null

	.. code-block::

		var cs = image.getColorSpace()

.. method:: Image.prototype.getNumberOfComponents()

	Number of colors; plus one if an alpha channel is present.

	:returns: number

	.. code-block::

		var num = image.getNumberOfComponents()

.. method:: Image.prototype.getBitsPerComponent()

	Returns the number of bits per component.

	:returns: number

	.. code-block::

		var bits = image.getBitsPerComponent()

.. method:: Image.prototype.getImageMask()

	Returns ``true`` if this image is an image mask.

	:returns: boolean

	.. code-block::

		var hasMask = image.getImageMask()

.. method:: Image.prototype.getMask()

	Get another `Image` used as a mask for this one.

	:returns: `Image` | null

	.. code-block::

		var mask = image.getMask()

.. method:: Image.prototype.getInterpolate()

	|only_mutool|

	Returns whether interpolation was used during decoding.

	:returns: boolean

	.. code-block:: javascript

		var interpolate = image.getInterpolate()

.. method:: Image.prototype.getColorKey()

	|only_mutool|

	Returns an array with ``2 * getNumberOfComponents()`` integers
	for an image with color key masking, or ``null`` if masking is
	not used. Each pair of integers define an interval, and
	component values within those intervals are not painted.

	:returns: Array of number | null

	.. code-block:: javascript

		var result = image.getColorKey()

.. method:: Image.prototype.getDecode()

	|only_mutool|

	Returns an array with ``2 * getNumberOfComponents()`` numbers
	for an image with color mapping, or ``null`` if mapping is not
	used. Each pair of numbers define the lower and upper values to
	which the component values are mapped linearly.

	:returns: Array of number | null

	.. code-block:: javascript

		var arr = image.getDecode()

.. method:: Image.prototype.getOrientation()

	|only_mutool|

	Returns the orientation of the image.

	:returns: number

	.. code-block:: javascript

		var orientation = image.getOrientation()

.. method:: Image.prototype.setOrientation(orientation)

	|only_mutool|

	Set the image orientation to the given orientation.

	:param number orientation:
		Orientation value described in this table:

		======= ===========
		Value	Description
		======= ===========
		0	Undefined
		1	0 degree ccw rotation. (Exif = 1)
		2	90 degree ccw rotation. (Exit = 8)
		3	180 degree ccw rotation. (Exif = 3)
		4	270 degree ccw rotation. (Exif = 6)
		5	flip on X. (Exif = 2)
		6	flip on X, then rotate ccw by 90 degrees. (Exif = 5)
		7	flip on X, then rotate ccw by 180 degrees. (Exif = 4)
		4	flip on X, then rotate ccw by 270 degrees. (Exif = 7)
		======= ===========

.. method:: Image.prototype.toPixmap(scaledWidth, scaledHeight)

	Create a `Pixmap` from this image. The ``scaledWidth`` and
	``scaledHeight`` arguments are optional, but may be used to decode a
	down-scaled `Pixmap`.

	:param number scaledWidth:
	:param number scaledHeight:

	:returns: `Pixmap`

	.. code-block:: javascript

		var pixmap = image.toPixmap()
		var scaledPixmap = image.toPixmap(100, 100)
