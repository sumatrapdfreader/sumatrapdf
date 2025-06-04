.. default-domain:: js

.. highlight:: javascript

Matrix
======

A Matrix is used to compute transformations of points and graphics
objects.

3-by-3 matrices of this form can perform 2-dimensional transformations
such as translations, rotations, scaling, and skewing:

.. code-block:: default

	/ a b 0 \
	| c d 0 |
	\ e f 1 /

Because of the fixes values in the right-most column, such a matrix is
represented in Javascript as an array of six numbers:

.. code-block::

	[a, b, c, d, e, f]

In TypeScript the Matrix type is defined as follows:

.. code-block::

	type Matrix = [number, number, number, number, number, number]

Constructors
------------

.. class:: Matrix

	|interface_type|

Matrices are not represented by a class; they are just plain arrays of six numbers.

Static properties
-----------------

.. data:: Matrix.identity

	The identity matrix, short hand for ``[1, 0, 0, 1, 0, 0]``.

	.. code-block::

		var m = mupdf.Matrix.identity

Static methods
--------------

.. function:: Matrix.scale(sx, sy)

	Returns a scaling matrix, short hand for ``[sx, 0, 0, sy, 0, 0]``.

	:param number sx: X scale as a floating point number.
	:param number sy: Y scale as a floating point number.

	:returns: `Matrix`

	.. code-block::

		var m = mupdf.Matrix.scale(2, 2)

.. function:: Matrix.translate(tx, ty)

	Return a translation matrix, short hand for ``[1, 0, 0, 1, tx, ty]``.

	:param number tx: X translation as a floating point number.
	:param number ty: Y translation as a floating point number.

	:returns: `Matrix`

	.. code-block::

		var m = mupdf.Matrix.translate(2, 2)

.. function:: Matrix.rotate(theta)

	Return a rotation matrix, short hand for
	``[cos(theta), sin(theta), -sin(theta), cos(theta), 0, 0]``.

	:param number theta: Rotation in degrees, positive for CW and negative for CCW.

	:returns: `Matrix`

	.. code-block::

		var m = mupdf.Matrix.rotate(90)

.. function:: Matrix.concat(a, b)

	Concatenate matrices ``a`` and ``b``. Bear in mind that matrix
	multiplication is not commutative.

	:param Matrix a: Left side matrix.
	:param Matrix b: Right side matrix.

	:returns: `Matrix`

	.. code-block::

		var m = mupdf.Matrix.concat([1, 1, 1, 1, 1, 1], [2, 2, 2, 2, 2, 2])
		// expected result [4, 4, 4, 4, 6, 6]

.. function:: Matrix.invert(matrix)

	Inverts the supplied matrix and returns the result.

	:param Matrix matrix: Matrix to invert.

	:returns: `Matrix`

	.. code-block::

		var m = mupdf.Matrix.invert([1, 0.5, 1, 1, 1, 1])
