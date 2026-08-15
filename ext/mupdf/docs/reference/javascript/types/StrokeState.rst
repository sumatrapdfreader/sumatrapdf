.. default-domain:: js

.. highlight:: javascript

StrokeState
===========

A StrokeState controls the properties of how stroking operations are performed.
Besides controlling the line width, it is also possible to control
:term:`line cap style`, :term:`line join style`, and the :term:`miter limit`.

Constructors
------------

.. class:: StrokeState(template)

	Create a new empty stroke state object.

	The Javascript object used as template allows for setting
	the following attributes:

	* "lineCap": string: The :term:`line cap style` to be used. One of ``"Butt" | "Round" | "Square"``
	* "lineJoin": string: The :term:`line join style` to be used. One of ``"Miter" | "Round" | "Bevel"``
	* "lineWidth": number: The line width to be used.
	* "miterLimit": number: The :term:`miter limit` to be used.
	* "dashPhase": number: The dash phase to be used.
	* "dashPattern": Array of number: The sequence of dash lengths to be used.

	:param Object template: An object with the parameters to set.

	.. code-block::

		var strokeState = new mupdf.StrokeState({
			lineCap: "Square",
			lineJoin: "Bevel",
			lineWidth: 2.0,
			miterLimit: 1.414,
			dashPhase: 11,
			dashPattern: [ 2, 3 ]
		})

Instance methods
----------------

.. method:: StrokeState.prototype.getLineCap()

	Get the :term:`line cap style`.

	:returns: ``"Butt" | "Round" | "Square"``

	.. code-block::

		var lineCap = strokeState.getLineCap()

.. method:: StrokeState.prototype.getLineJoin()

	Get the :term:`line join style`.

	:returns: ``"Miter" | "Round" | "Bevel"``

	.. code-block::

		var lineJoin = strokeState.getLineJoin()

.. method:: StrokeState.prototype.getLineWidth()

	Get the line line width.

	:returns: number

	.. code-block::

		var width = strokeState.getLineWidth()

.. method:: StrokeState.prototype.getMiterLimit()

	Get the :term:`miter limit`.

	:returns: number

	.. code-block::

		var limit = strokeState.getMiterLimit()

.. method:: StrokeState.prototype.getDashPhase()

	Get the dash pattern phase (where in the dash pattern stroking starts).

	:returns: number

	.. code-block:: javascript

		var limit = strokeState.getDashPhase()

.. method:: StrokeState.prototype.getDashPattern()

	Get the dash pattern as an array of numbers specifying alternating
	lengths of dashes and gaps, or null if none set.

	:returns: Array of number | null

	.. code-block:: javascript

		var dashPattern = strokeState.getDashPattern()
