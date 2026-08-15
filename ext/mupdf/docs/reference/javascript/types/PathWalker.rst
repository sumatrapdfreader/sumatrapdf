.. default-domain:: js

.. highlight:: javascript

PathWalker
==========

Constructors
------------

.. class:: PathWalker

	|interface_type|

An object implementing this interface of optional callback functions
can be used to get calls whenever `Path.prototype.walk()` iterates over a
basic drawing operation corresponding to that of the function name.

.. function:: closePath()

	Called when `Path.prototype.walk()` encounters a close subpath operation.

.. function:: curveTo(x1, y1, x2, y2, x3, y3)

	Called when `Path.prototype.walk()` encounters an operation drawing a Bézier
	curve from the current point to (x3, y3) using (x1, y1) and (x2, y2)
	as control points.

	.. imagesvg:: ../../../images/curveTo.svg
	   :tagtype: object

	:param number x1: X1 coordinate.
	:param number y1: Y1 coordinate.
	:param number x2: X2 coordinate.
	:param number y2: Y2 coordinate.
	:param number x3: X3 coordinate.
	:param number y3: Y3 coordinate.

.. function:: lineTo(x, y)

	Called when `Path.prototype.walk()` encounters an operation drawing a straight
	line from the current point to the given point.

	.. imagesvg:: ../../../images/lineTo.svg
	   :tagtype: object

	:param number x: X coordinate.
	:param number y: Y coordinate.

.. function:: moveTo(x, y)

	Called when `Path.prototype.walk()` encounters an operation moving the pen to
	the given point, beginning a new subpath and sets the current point.

	:param number x: X coordinate.
	:param number y: Y coordinate.
