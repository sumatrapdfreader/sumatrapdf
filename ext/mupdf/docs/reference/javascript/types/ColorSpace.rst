.. default-domain:: js

.. highlight:: javascript

ColorSpace
==========

Defines what color system is used for an object, and which colorants exist.

For a `Pixmap` the interface `Pixmap.prototype.getColorSpace()` returns its colorspace,
which defines what colorants make up the pixel colors in the Pixmap and
how those colorants are stored. If that colorspace is e.g.,
`ColorSpace.DeviceRGB`, then the colorants **red**, **green** and
**blue** are combined to make up the color of each pixel.

Constructors
------------

.. class:: ColorSpace(from, name)

	Create a new ColorSpace from an ICC profile.

	:param Buffer | ArrayBuffer | Uint8Array | string from: A buffer containing an ICC profile.
	:param string name: A user descriptive name for the new colorspace.

	.. code-block::

		var icc_colorspace = new mupdf.ColorSpace(fs.readFileSync("SWOP.icc"), "SWOP")

Static properties
-----------------

.. data:: ColorSpace.DeviceGray

	The default Grayscale colorspace

.. data:: ColorSpace.DeviceRGB

	The default RGB colorspace

.. data:: ColorSpace.DeviceBGR

	The default RGB colorspace, but with components in reverse order

.. data:: ColorSpace.DeviceCMYK

	The default CMYK colorspace

.. data:: ColorSpace.Lab

	The default Lab colorspace

Instance methods
----------------

.. method:: ColorSpace.prototype.getName()

	Return the name of this colorspace.

.. method:: ColorSpace.prototype.getNumberOfComponents()

	A Grayscale colorspace has one component, RGB has 3, CMYK has 4, and DeviceN may have any number of components.

	:returns: number of components

	.. code-block::

		var cs = mupdf.ColorSpace.DeviceRGB
		var num = cs.getNumberOfComponents() // 3

.. method:: ColorSpace.prototype.getType()

	Returns a string indicating the type of this colorspace, one of:

	:returns: ``"None" | "Gray" | "RGB" | "BGR" | "CMYK" | "Lab" | "Indexed" | "Separation"``

.. method:: ColorSpace.prototype.isCMYK()

	Returns ``true`` if the object is a CMYK color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isCMYK()

.. method:: ColorSpace.prototype.isDeviceN()

	Returns ``true`` if the object is a Device N color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isDeviceN()

.. method:: ColorSpace.prototype.isGray()

	Returns ``true`` if the object is a gray color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isGray()

.. method:: ColorSpace.prototype.isIndexed()

	Returns ``true`` if the object is an Indexed color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isIndexed()

.. method:: ColorSpace.prototype.isLab()

	Returns ``true`` if the object is a Lab color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isLab()

.. method:: ColorSpace.prototype.isRGB()

	Returns ``true`` if the object is an RGB color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isRGB()

.. method:: ColorSpace.prototype.isSubtractive()

	Returns ``true`` if the object is a subtractive color space.

	:returns: boolean

	.. code-block::

		var bool = colorSpace.isSubtractive()

.. method:: ColorSpace.prototype.toString()

	Return name of this colorspace.

	:returns: string

	.. code-block::

		var cs = mupdf.ColorSpace.DeviceRGB
		var name = cs.toString() // "[ColorSpace DeviceRGB]"
