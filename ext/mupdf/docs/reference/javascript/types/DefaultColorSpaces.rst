.. default-domain:: js

.. highlight:: javascript

DefaultColorSpaces
==================

|only_mutool|

An object given to a `Device` with the `Device.prototype.setDefaultColorSpaces()` call so it
can obtain the current default `ColorSpace` objects.

Constructors
------------

.. class:: DefaultColorSpaces()

Instance methods
----------------

.. method:: DefaultColorSpaces.prototype.getDefaultGray()

	Get the default gray colorspace.

	:returns: `ColorSpace`

.. method:: DefaultColorSpaces.prototype.getDefaultRGB()

	Get the default RGB colorspace.

	:returns: `ColorSpace`

.. method:: DefaultColorSpaces.prototype.getDefaultCMYK()

	Get the default CMYK colorspace.

	:returns: `ColorSpace`

.. method:: DefaultColorSpaces.prototype.getOutputIntent()

	Get the output intent.

	:returns: `ColorSpace`

.. method:: DefaultColorSpaces.prototype.setDefaultGray(colorspace)

	:param ColorSpace colorspace: The new default gray colorspace.

.. method:: DefaultColorSpaces.prototype.setDefaultRGB(colorspace)

	:param ColorSpace colorspace: The new default RGB  colorspace.

.. method:: DefaultColorSpaces.prototype.setDefaultCMYK(colorspace)

	:param ColorSpace colorspace: The new default CMYK colorspace.

.. method:: DefaultColorSpaces.prototype.setOutputIntent(colorspace)

	:param ColorSpace colorspace: The new default output intent.
