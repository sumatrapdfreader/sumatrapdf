.. default-domain:: js

.. highlight:: javascript

DrawDevice
==========

The DrawDevice can be used to render to a `Pixmap`; either by running a Page
using `Page.prototype.run()` with it or by calling the device methods directly.

Constructors
------------

.. class:: DrawDevice(matrix, pixmap)

	Create a device for drawing into a `Pixmap`. The `Pixmap` bounds used should match the transformed page bounds, or you can adjust them to only draw a part of the page.

	:param Matrix matrix: The matrix to apply.
	:param Pixmap pixmap: The pixmap that will be drawn to.

	.. code-block::

		var drawDevice = new mupdf.DrawDevice(mupdf.Matrix.identity, pixmap)
