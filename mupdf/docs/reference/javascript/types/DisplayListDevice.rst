.. default-domain:: js

.. highlight:: javascript

DisplayListDevice
=================

Constructors
------------

.. class:: DisplayListDevice(displayList)

	Create a device for recording onto a display list.

	:param DisplayList displayList: the display list to populate.

	.. code-block::

		var myDisplayList = new mupdf.DisplayList([0, 0, 100, 100])
		var displayListDevice = new mupdf.DisplayListDevice(myDisplayList)
