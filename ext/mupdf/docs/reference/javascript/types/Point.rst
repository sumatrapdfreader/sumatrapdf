.. default-domain:: js

.. highlight:: javascript

Point
=====

A Point describes a point in space. It is represented by an array with two coordinates.

Constructors
------------

.. class:: Point

	|interface_type|

Points are not represented by a class; they are just plain arrays of two numbers.

Static methods
--------------

.. function:: Point.transform(point, matrix)

	|only_mutool|

	Transforms the supplied point by the given transformation matrix.

	:param Point point: Point to transform.
	:param Matrix matrix: Matrix describing transformation to perform.

	:returns: `Point`

	.. code-block::

		var m = mupdf.Point.transform([100, 100], [1, 0.5, 1, 1, 1, 1])
