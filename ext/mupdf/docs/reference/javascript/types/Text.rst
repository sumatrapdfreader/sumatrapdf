.. default-domain:: js

.. highlight:: javascript

Text
====

A Text object contains text. See the `Device.prototype.fillText`.

Constructors
------------

.. class:: Text()

	Create a new empty text object.

	.. code-block::

		var text = new mupdf.Text()

Instance methods
----------------

.. method:: Text.prototype.getBounds(strokeState, transform)

	Get the bounds of the instance.

	:param StrokeState strokeState: The stroking state.
	:param Matrix transform: The transformation matrix.

.. method:: Text.prototype.showGlyph(font, trm, gid, uni, wmode)

	Add a glyph to the text object.

	Transform is the text matrix, specifying font size and glyph location.
	For example: ``[size, 0, 0, -size, x, y]``.

	Glyph and unicode may be ``-1`` for n-to-m cluster mappings. For
	example, the "fi" ligature would be added in two steps: first the glyph
	for the 'fi' ligature and the unicode value for 'f'; then glyph ``-1``
	and the unicode value for 'i'.

	:param Font font: Font object.
	:param Matrix trm: Transformation matrix.
	:param number gid: Glyph id.
	:param number uni: Unicode codepoint.
	:param number wmode: ``0`` (default) for horizontal writing, and ``1`` for vertical writing.

	.. code-block::

		text.showGlyph(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, 21, 0x66, 0)
		text.showGlyph(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, -1, 0x69, 0)

.. method:: Text.prototype.showString(font, trm, str, wmode)

	Add a simple string to this Text object. Will do font substitution if the font does not have all the unicode characters required.

	:param Font font: Font object.
	:param Matrix trm: Transformation matrix.
	:param string str: Content for Text object.
	:param number wmode: ``0`` (default) for horizontal writing, and ``1`` for vertical writing.

	.. code-block::

		text.showString(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, "Hello World")

.. method:: Text.prototype.walk(walker)

	:param TextWalker walker: Function with protocol methods, see example below for details.

	.. code-block::

		function print(...args) {
			console.log(args.join(" "))
		}

		var textPrinter = {
			beginSpan: function (f, m, wmode, bidi, dir, lang) {
				print("beginSpan", f, m, wmode, bidi, dir, Q(lang))
			},
			showGlyph: function (f, m, g, u, v, b) { print("glyph", f, m, g, String.fromCodePoint(u), v, b) },
			endSpan: function () { print("endSpan") }
		}

		var traceDevice = {
			fillText: function (text, ctm, colorSpace, color, alpha) {
				print("fillText", ctm, colorSpace, color, alpha)
				text.walk(textPrinter)
			},
			clipText: function (text, ctm) {
				print("clipText", ctm)
				text.walk(textPrinter)
			},
			strokeText: function (text, stroke, ctm, colorSpace, color, alpha) {
				print("strokeText", Q(stroke), ctm, colorSpace, color, alpha)
				text.walk(textPrinter)
			},
			clipStrokeText: function (text, stroke, ctm) {
				print("clipStrokeText", Q(stroke), ctm)
				text.walk(textPrinter)
			},
			ignoreText: function (text, ctm) {
				print("ignoreText", ctm)
				text.walk(textPrinter)
			}
		}

		var doc = mupdf.PDFDocument.openDocument(fs.readFileSync("test.pdf"), "application/pdf")
		var page = doc.loadPage(0)
		var device = new mupdf.Device(traceDevice)
		page.run(device, mupdf.Matrix.identity)
