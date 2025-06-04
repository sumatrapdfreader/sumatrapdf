.. default-domain:: js

.. highlight:: javascript

Quad
====

An object representing a quadrilateral or :term:`QuadPoint`.

In TypeScript the Quad type is defined as follows:

.. code-block::

	type Quad = [ number, number, number, number number, number, number, number ]

Constructors
------------

.. class:: Quad

	|interface_type|

Quads are not represented by a class; they are just plain arrays of eight numbers.

Static properties
-----------------

.. data:: Quad.empty

	|only_mutool|

	A quad whose coordinates are such that it is categorized as empty.

.. data:: Quad.invalid

	|only_mutool|

	A quad whose coordinates are such that it is categorized as invalid.

.. data:: Quad.infinite

	|only_mutool|

	A quad whose coordinates are such that it is categorized as infinite.

Static methods
--------------

.. function:: Quad.isEmpty(quad)

	|only_mutool|

	Returns a boolean indicating if the quad is empty or not.

	:param Quad quad: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isEmpty = mupdf.Quad.isEmpty([0, 0, 0, 0]) // true
		var isEmpty = mupdf.Quad.isEmpty([0, 0, 100, 100]) // false

.. function:: Quad.isValid(quad)

	|only_mutool|

	Returns a boolean indicating if the quad is valid or not.

	:param Quad quad: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isValid = mupdf.Quad.isValid([0, 0, 100, 100]) // true
		var isValid = mupdf.Quad.isValid([0, 0, -100, 100]) // false

.. function:: Quad.isInfinite(quad)

	|only_mutool|

	Returns a boolean indicating if the quad is infinite or not.

	:param Quad quad: Rectangle to evaluate.

	:returns: boolean

	.. code-block::

		var isInfinite = mupdf.Quad.isInfinite([0x80000000, 0x80000000, 0x7fffff80, 0x7fffff80]) //true
		var isInfinite = mupdf.Quad.isInfinite([0, 0, 100, 100]) // false

.. function:: Quad.transform(quad, matrix)

	|only_mutool|

	Transforms the supplied quad by the given transformation matrix.

	Transforming an invalid, empty or infinite quad results in the
	supplied quad being returned without change.

	:param Quad quad: Quad to transform.
	:param Matrix matrix: Matrix describing transformation to perform.

	:returns: `Quad`

	.. code-block::

		var m = mupdf.Quad.transform([0, 0, 100, 100], [1, 0.5, 1, 1, 1, 1])

.. function:: Quad.isPointInside(quad, point)

	|only_mutool|

	Return whether the point is inside the quad.

	:returns boolean

	.. code-block::

		var inside = mupdf.Rect.isPointInside([0, 0, 100, 100], [50, 50])

.. function:: Quad.quadFromRect(rect)

	|only_mutool|

	Create a Quad that maps exactly to the coordinate of rectangle.

	:param Rect rect:

	:returns: `Quad`
