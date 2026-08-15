.. default-domain:: js

.. highlight:: javascript

Rect
====

A Rect describes an axis-aligned rectangle by specifying the coordinates
of its upper left and lower right corners. In Javascript they are
represented by a 4-element array: ``[minX, minY, maxX, maxY]``

The relationship between the ``minX``, ``minY``, ``maxX`` and ``maxY``
values can be set so that a rectangle is categorized as invalid, empty or
infinite. This matters when passing such rectangles to
`Rect.transform()`. There are convenience functions to obtain such
rectangles: `Rect.empty`, `Rect.invalid` and
`Rect.infinite`.

In TypeScript the Rect type is defined as follows:

.. code-block::

	type Rect = [number, number, number, number]

Constructors
------------

.. class:: Rect

	|interface_type|

Rects are not represented by a class; they are just plain arrays of four numbers.

Static properties
-----------------

.. data:: Rect.empty

	A rectangle whose coordinates are such that it is categorized as empty.

.. data:: Rect.invalid

	A rectangle whose coordinates are such that it is categorized as invalid.

.. data:: Rect.infinite

	A rectangle whose coordinates are such that it is categorized as infinite.

Static methods
--------------

.. function:: Rect.isEmpty(rect)

	Returns whether the rectangle is empty or not.

	:param Rect rect: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isEmpty = mupdf.Rect.isEmpty([0, 0, 0, 0]) // true
		var isEmpty = mupdf.Rect.isEmpty([0, 0, 100, 100]) // false

.. function:: Rect.isValid(rect)

	Returns whether the rectangle is valid or not.

	:param Rect rect: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isValid = mupdf.Rect.isValid([0, 0, 100, 100]) // true
		var isValid = mupdf.Rect.isValid([0, 0, -100, 100]) // false

.. function:: Rect.isInfinite(rect)

	Returns whether the rectangle is infinite or not.

	:param Rect rect: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isInfinite = mupdf.Rect.isInfinite([0x80000000, 0x80000000, 0x7fffff80, 0x7fffff80]) //true
		var isInfinite = mupdf.Rect.isInfinite([0, 0, 100, 100]) // false

.. function:: Rect.transform(rect, matrix)

	Transforms the supplied rectangle by the given transformation matrix.

	Transforming an invalid, empty or infinite rectangle results in the
	supplied rectangle being returned without change.

	:param Rect rect: Rectangle to transform.
	:param Matrix matrix: Matrix describing transformation to perform.

	:returns: `Rect`

	.. code-block::

		var m = mupdf.Rect.transform([0, 0, 100, 100], [1, 0.5, 1, 1, 1, 1])

.. function:: Rect.isPointInside(rect, point)

	Return whether the point is inside the rectangle.

	:returns boolean

	.. code-block::

		var inside = mupdf.Rect.isPointInside([0, 0, 100, 100], [50, 50])

.. function:: Rect.rectFromQuad(quad)

	Create a Rect that encompasses the entire quad.

	:param Quad quad:

	:returns: `Rect`
