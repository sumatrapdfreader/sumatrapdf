.. default-domain:: js

.. highlight:: javascript

DisplayList
===========

A display list is a sequence of device calls that can be replayed multiple
times. This is useful e.g. when you need to render a page at multiple
resolutions, or when you want to both render a page and later search for
text in it. Using a display list to do this, improves performance because
it avoids repeatedly reinterpreting the document from file

To populate a display list use the `DisplayListDevice`.

.. code-block::

	var list = new mupdf.DisplayList([0, 0, 595, 842])
	var listDevice = new mupdf.DisplayListDevice(list)
	page.run(listDevice, mupdf.Matrix.identity)

	var pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0, 0, 595, 842], false)
	var drawDevice = new mupdf.DrawDevice(mupdf.Matrix.identity, pixmap)
	list.run(drawDevice, mupdf.Matrix.identity)

	var searchHits = list.search("hello world")

Constructors
------------

.. class:: DisplayList(mediabox)

	Create an empty display list. The mediabox rectangle should be the
	bounds of the page.

	:param Rect mediabox: The size of the page.

	.. code-block::

		var displayList = new mupdf.DisplayList([0, 0, 595, 842])

Instance methods
----------------

.. method:: DisplayList.prototype.run(device, matrix)

	Play back this display lists sequence of device calls to the given device.

	:param Device device: The device to replay the device calls to.
	:param Matrix matrix: Transformation matrix to apply to coordinates in all device calls.

	.. code-block::

		displayList.run(device, mupdf.Matrix.identity)

.. method:: DisplayList.prototype.getBounds()

	Return a bounding rectangle that encompasses all the contents of the display list.

	:returns: `Rect`

	.. code-block::

		var bounds = displayList.getBounds()

.. method:: DisplayList.prototype.toPixmap(matrix, colorspace, alpha)

	Render a display list to a `Pixmap`.

	:param Matrix matrix: Transformation matrix.
	:param ColorSpace colorspace: The desired colorspace of the returned pixmap.
	:param boolean alpha: Whether the returned pixmap has transparency or not. If the pixmap handles transparency, it starts out transparent (otherwise it is filled white), before the contents of the display list are rendered onto the pixmap.

	:returns: `Pixmap`

	.. code-block::

		var pixmap = displayList.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, false)

.. method:: DisplayList.prototype.toStructuredText(options)

	Extract the text on the page into a `StructuredText` object.

	:param string options:
		See :doc:`/reference/common/stext-options`.

	:returns: `StructuredText`

	.. code-block::

		var sText = displayList.toStructuredText("preserve-whitespace")

.. method:: DisplayList.prototype.search(needle, max_hits)

	Search the display list text for all instances of the text value
	``needle``, and return an array of search hits. Each search hit is an
	array of `Quad`, each corresponding to a single character in the search
	hit.

	:param string needle: The text to search for.
	:param number options: Optional options for the search. A logical or of options such as `StructuredText.SEARCH_EXACT`.

	:returns: Array of Array of `Quad`

	.. code-block::

		var results = displayList.search("my search phrase")

.. method:: DisplayList.protoype.decodeBarcode(subarea, rotate)

	|only_mutool|

	Decodes a barcode detected in the display list, and returns an
	object with properties for barcode type and contents.

	:param Rect subarea: Only detect barcode within subarea. Defaults to the entire area returned by `DisplayList.prototype.getBounds()`.
	:param number rotate: Degrees of rotation to rotate display list before detecting barcode. Defaults to 0.

	:returns: Object with barcode information.

	.. code-block:: javascript

		var barcodeInfo = displayList.decodeBarcode([0, 0, 100, 100 ], 0)
