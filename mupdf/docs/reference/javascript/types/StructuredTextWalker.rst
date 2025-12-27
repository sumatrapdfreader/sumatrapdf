.. default-domain:: js

.. highlight:: javascript

StructuredTextWalker
====================

A structured text walker is an object with (optional) callback methods
used to iterate over the contents of a `StructuredText`.

Constructors
------------

.. class:: StructuredTextWalker

	|interface_type|

On beginLine the direction parameter is a vector (e.g. [0, 1]) and
can you can calculate the rotation as an angle with some trigonometry on the vector.

.. function:: beginTextBlock(bbox)

	Called before every text block in the `StructuredText`.

	:param Rect bbox:

.. function:: endTextBlock()

	Called after every text block.

.. function:: beginLine(bbox, wmode, direction)

	Called before every line of text in a block.

	:param Rect bbox:
	:param number wmode:
	:param Point direction:

.. function:: endLine()

	Called after every line of text.

.. function:: beginStruct(standard, raw, index)

	Called to indicate that a new structure element begins. May not
	be neatly nested within blocks or lines.

	:param string standard:
	:param string raw:
	:param number index:

.. function:: endStruct()

	Called after every structure element.

.. function:: onChar(c, origin, font, size, quad, color, flags)

	Called for every character in a line of text.

	:param string c:
	:param Point origin:
	:param Font font:
	:param number size:
	:param Quad quad:
	:param Color color:
	:param number flags:

.. function:: onImageBlock(bbox, transform, image)

	Called for every image in a `StructuredText` if its options were
	set to preserve images.

	:param Rect bbox:
	:param Matrix transform:
	:param Image image:

.. function:: onVector(bbox, flags, rgb)

	Called for every vector in a `StructuredText` if its options
	were set to collect vectors.

	:param Rect bbox:
	:param Object flags:
	:param Array of number rgb:

	The flags object is of the form ``{ isStroked: boolean, isRectangle: boolean }``.
