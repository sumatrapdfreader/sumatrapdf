.. default-domain:: js

.. highlight:: javascript

TextWalker
==========

.. class:: TextWalker

An object implementing this interface of optional callback functions
can be used to get calls whenever `Text.prototype.walk()` iterates over a text object.

.. function:: beginSpan(font, trm, wmode, bidiLevel, markupDirection, language)

	Called before every text span in the `Text` being walked.

	:param Font font:
	:param Matrix trm:
	:param number wmode:
	:param int bidiLevel:
	:param int markupDirection:
	:param string language:

.. function:: endSpan()

	Called at the end of every span in the text.

.. function:: showGlyph(font, trm, gid, ucs, wmode, bidiLevel)

	Called once per character for in a text span.

	:param Font font:
	:param Matrix trm:
	:param number gid:
	:param number ucs:
	:param number wmode:
	:param number bidiLevel:
