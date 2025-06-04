.. default-domain:: js

.. highlight:: javascript

StructuredText
===================

StructuredText objects hold text from a page that has been analyzed and grouped
into blocks, lines and spans.

Constructors
------------

.. class:: StructuredText

	|no_new|

To obtain a StructuredText instance use `Page.prototype.toStructuredText()`.

Instance methods
----------------

.. method:: StructuredText.prototype.search(needle, maxHits)

	Search the text for all instances of needle, and return an array with all matches found on the page.

	Each match in the result is an array containing one or more Quads that cover the matching text.

	:param string needle: The text to search for.
	:param number maxHits: Maximum number of hits to return. Default 500.

	:returns: Array of Array of `Quad`

	.. code-block::

		var result = sText.search("Hello World!")

.. method:: StructuredText.prototype.highlight(p, q, maxHits)

	Return an array of `Quad` used to highlight a selection defined by the start and end points.

	:param Point p: Start point.
	:param Point q: End point.
	:param number maxHits: The maximum number of hits to return. Default 500.

	:returns: Array of `Quad`

	.. code-block::

		var result = sText.highlight([100, 100], [200, 100])

.. method:: StructuredText.prototype.copy(p, q)

	Return the text from the selection defined by the start and end points.

	:param Point p: Start point.
	:param Point q: End point.

	:returns: string

	.. code-block::

		var result = sText.copy([100, 100], [200, 100])

.. method:: StructuredText.prototype.walk(walker)

	:param StructuredTextWalker walker: Callback object.

	Walk through the blocks (images or text blocks) of the structured text.
	For each text block walk over its lines of text, and for each line each
	of its characters. For each block, line or character the walker will
	have a method called.

	.. code-block::

		var sText = page.toStructuredText()
		sText.walk({
			beginLine: function (bbox, wmode, direction) {
				console.log("beginLine", bbox, wmode, direction)
			},
			endLine: function () {
				console.log("endLine")
			},
			beginTextBlock: function (bbox) {
				console.log("beginTextBlock", bbox)
			},
			endTextBlock: function () {
				console.log("endTextBlock")
			},
			beginStruct: function (standard, raw, index) {
				console.log("beginStruct", standard, raw, index)
			},
			endStruct: function () {
				console.log("endStruct")
			},
			onChar: function (utf, origin, font, size, quad, argb) {
				console.log("onChar", utf, origin, font, size, quad, argb)
			},
			onImageBlock: function (bbox, transform, image) {
				console.log("onImageBlock", bbox, transform, image)
			},
			onVector: function (isStroked, isRectangle, argb) {
				console.log("onVector", isStroked, isRectangle, argb)
			},
		})

.. method:: StructuredText.prototype.asText()

	Returns a plain text representation.

	:returns: string

.. method:: StructuredText.prototype.asHTML(id)

	Returns a string containing an HTML rendering of the text.

	:param number id:
		Used to number the "id" on the top div tag (as ``"page" + id``).

	:returns: string

.. method:: StructuredText.prototype.asJSON(scale)

	Returns a JSON string representing the structured text data.

	This is a simplified serialization of the information that
	`StructuredText.prototype.walk()` provides.

	Note: You must extract the structured text with "preserve-spans"!
	If you forget to set this option, any font changes in the middle of the
	line will not be present in the JSON output.

	:param number scale: Optional scaling factor to multiply all the coordinates by.

	:returns: string containing JSON of the following schema:

		.. code-block:: typescript

			type StructuredTextPage = {
				blocks: StructuredTextBlock[]
			}
			type StructuredTextBlock = {
				type: "image" | "text",
				bbox: {
					x: number,
					y: number,
					w: number,
					h: number
				},
				lines: StructuredTextLine[],
			}
			type StructuredTextLine = {
				wmode: 0 | 1,	// 0=horizontal, 1=vertical
				bbox: {
					x: number,
					y: number,
					w: number,
					h: number
				},
				font: {
					name: string,
					family: "serif" | "sans-serif" | "monospace",
					weight: "normal" | "bold",
					style: "normal" | "italic",
					size: number
				},
				// text origin point for first character in line
				x: number,
				y: number,
				text: string
			}

	.. code-block::

		var data = JSON.parse(page.toStructuredText("preserve-spans").asJSON())
