.. default-domain:: js

.. highlight:: javascript

Page
====

A Page object is a page that has been loaded from a `Document`.

Page objects that belong to a `PDFDocument`, also provide
the interface described in `PDFPage`.

Constructors
------------

.. class:: Page

	|no_new|

Page instances are returned by `Document.prototype.loadPage()`.

Constants
---------

The :term:`page box` types:

.. data:: Page.MEDIA_BOX
.. data:: Page.CROP_BOX
.. data:: Page.BLEED_BOX
.. data:: Page.TRIM_BOX
.. data:: Page.ART_BOX

Instance methods
----------------

.. method:: Page.prototype.getBounds(box)

	Returns a rectangle describing the page dimensions.

	:param string box: Which :term:`page box` to query.

	:returns: `Rect`

	.. code-block::

		var rect = page.getBounds()

.. method:: Page.prototype.run(device, transform)

	Calls device functions for all the contents on the page, using the
	specified transform.

	The device can be one of the built-in devices (`DrawDevice` and `DisplayListDevice`)
	or a Javascript `Device`.

	The matrix transforms coordinates from user space to device space.

	:param Device device: The device object.
	:param Matrix matrix: The transformation matrix.

	.. code-block::

		page.run(dev, mupdf.Matrix.identity)

.. method:: Page.prototype.runPageContents(device, transform)

	This is the same as the `Page.prototype.run()` method above but it only
	runs the page itself and omits annotations and widgets.

	:param Device device: The device object.
	:param Matrix matrix: The transformation matrix.

	.. code-block::

		page.runPageContents(dev, mupdf.Matrix.identity)

.. method:: Page.prototype.runPageAnnots(device, transform)

	This is the same as the `Page.prototype.run()` method above but it only
	runs the page annotations.

	:param Device device: The device object.
	:param Matrix matrix: The transformation matrix.

	.. code-block::

		page.runPageAnnots(dev, mupdf.Matrix.identity)

.. method:: Page.prototype.runPageWidgets(device, transform)

	This is the same as the `Page.prototype.run()` method above but it only
	runs the page widgets.

	:param Device device: The device object.
	:param Matrix matrix: The transformation matrix.

	.. code-block::

		page.runPageWidgets(dev, mupdf.Matrix.identity)

.. method:: Page.prototype.toPixmap(matrix, colorspace, alpha, showExtras)

	Render the page into a `Pixmap` using the specified transform
	matrix and colorspace. If ``alpha`` is ``true``, the page will be drawn
	on a transparent background, otherwise white. If ``showExtras`` is
	``true`` then the operation will include any page annotations and/or
	widgets.

	:param Matrix matrix: The transformation matrix.
	:param ColorSpace colorspace: The desired colorspace of the returned pixmap.
	:param boolean alpha: Whether the resulting pixmap should have an alpha component. Defaults to ``true``.
	:param boolean showExtras: Whether to render annotations and widgets. Defaults to ``true``.

	:returns: `Pixmap`

	.. code-block::

		var pixmap = page.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true, true)

.. method:: Page.prototype.toDisplayList(showExtras)

	Record the contents on the page into a `DisplayList`. If
	``showExtras`` is ``true`` then the operation will include all
	annotations and/or widgets on the page.

	:param boolean showExtras: Whether to render annotations and widgets. Defaults to ``true``.

	:returns: `DisplayList`

	.. code-block::

		var displayList = page.toDisplayList(true)

.. method:: Page.prototype.toStructuredText(options)

	Extract the text on the page into a `StructuredText` object.

	:param string options:
		See :doc:`/reference/common/stext-options`.

	:returns: `StructuredText`

	.. code-block::

		var sText = page.toStructuredText("preserve-whitespace")

.. method:: Page.prototype.search(needle, maxHits)

	Search the page text for all instances of the ``needle`` value,
	and return an array of search hits.

	Each search hit is an array of `Quad`, each corresponding
	to a character in the search hit.

	:param string needle: The text to search for.
	:param number options: Optional options for the search. A logical or of options such as `StructuredText.SEARCH_EXACT`.

	:returns: Array of Array of `Quad`

	.. code-block::

		var results = page.search("my search phrase")

.. method:: Page.prototype.getLinks()

	Return an array of all the links on the page. If there are no
	links then an empty array is returned.

	Each link is an object with a 'bounds' property, and either a
	'page' or 'uri' property, depending on whether it's an internal or
	external link.

	:returns: Array of `Link`

	.. code-block::

		var links = page.getLinks()
		var link = links[0]
		var linkDestination = doc.resolveLink(link)

.. method:: Page.prototype.createLink(rect, uri)

	Create a new link with the supplied metrics for the page, linking to the destination URI string.

	To create links to other pages within the document see the `Document.prototype.formatLinkURI` method.

	:param Rect rect: Rectangle specifying the active area on the page the link should cover.
	:param string destinationUri: A URI string describing the desired link destination.

	:returns: `Link`.

	.. code-block::

		// create a link to an external URL
		var link = page.createLink([0, 0, 100, 50], "https://example.com")

		// create a link to another page in the document
		var link = page.createLink([0, 100, 100, 150], "#page=1&view=FitV,0")

.. method:: Page.prototype.deleteLink(link)

	Delete the link from the page.

	:param Link link: The link to remove.

	.. code-block::

		page.deleteLink(link_obj)

.. method:: Page.prototype.getLabel()

	Returns the page number as a string using the numbering scheme of the document.

	:returns: string

	.. code-block::

		var label = page.getLabel()

.. method:: Page.prototype.isPDF()

	Returns ``true`` if the page is from a PDF document.

	:returns: boolean

	.. code-block::

		var isPDF = page.isPDF()

.. method:: Page.prototype.decodeBarcode(subarea, rotate)

	|only_mutool|

	Decodes a barcode detected on the page, and returns an object with
	properties for barcode type and contents.

	:param Rect subarea: Only detect barcode within subarea. Defaults to the entire page.
	:param number rotate: Degrees of rotation to rotate page before detecting barcode. Defaults to 0.

	:returns: Object with barcode information.

	.. code-block:: javascript

		var info = page.decodeBarcode(page.getBounds(), 0)
