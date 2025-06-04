.. default-domain:: js

.. highlight:: javascript

OutlineItem
===========

Outline items are returned from the `Document.prototype.loadOutline` method and
represent a table of contents entry.

They are also used with the `OutlineIterator` interface.

Constructors
------------

.. class:: OutlineItem

	|interface_type|

Outline items are passed around as plain objects.

.. code-block:: javascript

	interface OutlineItem {
		title: string | undefined,
		uri: string | undefined,
		open: boolean,
		down?: OutlineItem[],
		page?: number,
	}
