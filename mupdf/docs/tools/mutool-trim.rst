mutool trim
===========

This command allows you to make a modified version of a PDF with content that falls inside or outside of various "boxes" removed.

See :doc:`/tools/mutool-pages` for how to list out the boxes used on a page.

.. code-block:: bash

	mutool trim [options] input.pdf

``[options]``
	Options are as follows:

	``-b`` box
		Which :term:`page box` to trim to (``mediabox`` (default), ``cropbox``, ``bleedbox``, ``trimbox``, ``artbox``).

	``-m`` margins
		Add margins to box (positive for inwards, negative for outwards).

		*<All>* or *<V>,<H>* or *<T>,<R>,<B>,<L>*

	``-e``
		Exclude contents of box, rather than include them.

	``-f``
		Fallback to ``mediabox`` if specified box not available.

	``-o`` filename
		Output file.

Examples
--------

Trim a document by trimming the MediaBox elements with a margin of 200 points inwards:

.. code-block::

	mutool trim -b mediabox -m 200 -o out.pdf in.pdf

Trim a document by trimming the MediaBox elements with a margin of 20 points from the top & bottom (vertical) and 30 points from the left & right (horizontal):

.. code-block::

	mutool trim -b mediabox -m 20,30 -o out.pdf in.pdf

Trim a document by trimming the MediaBox elements to a box offset 10 points from the top & right, 50 points from the bottom and 20 points in from the left:

.. code-block::

	mutool trim -b mediabox -m 10,10,50,20 -o out.pdf in.pdf

Exclude the contents of the ArtBox element:

.. code-block::

	mutool trim -b artbox -e -o out.pdf in.pdf
