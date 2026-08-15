.. default-domain:: js

.. highlight:: javascript

OutlineIterator
===============

An outline iterator can be used to walk over all the items in an Outline and
query their properties. To be able to insert items at the end of a list of
sibling items, it can also walk one item past the end of the list. To get an
instance of `OutlineIterator` use `Document.prototype.outlineIterator()`.

In the context of a PDF file, the document's Outline is also known as Table of
Contents or Bookmarks.

Constructors
------------

.. class:: OutlineIterator

	|no_new|

OutlineIterator instances are returned by `Document.prototype.outlineIterator()`.

Constants
---------

Navigation return codes:

.. data:: OutlineIterator.ITERATOR_DID_NOT_MOVE

	Movement was not possible.

.. data:: OutlineIterator.ITERATOR_AT_ITEM

	New position has a valid item.

.. data:: OutlineIterator.ITERATOR_AT_EMPTY

	New position has no item, but one can be inserted here.

Style bit flags:

.. data:: OutlineIterator.FLAG_BOLD

	Bit is set outline item style is bold.

.. data:: OutlineIterator.FLAG_ITALIC

	Bit is set if outline item style is italic.

Instance methods
----------------

.. method:: OutlineIterator.prototype.item()

	Return a `OutlineItem` or ``null`` if out of range.

	:returns: `OutlineItem` | null

	.. code-block::

		var obj = outlineIterator.item()

.. method:: OutlineIterator.prototype.next()

	Move the iterator position to "next". The return value is negative if
	this movement is not possible, ``0`` if the new position has a valid
	item, or ``1`` if the new position has no item but one can be inserted
	here.

	:returns: number

	.. code-block::

		var result = outlineIterator.next()

.. method:: OutlineIterator.prototype.prev()

	Move the iterator position to "previous". The return value is negative
	if this movement is not possible, ``0`` if the new position has a valid
	item, or ``1`` if the new position has no item but one can be inserted
	here.

	:returns: number

	.. code-block::

		var result = outlineIterator.prev()

.. method:: OutlineIterator.prototype.up()

	Move the iterator position "up". The result value is negative if this
	movement is not possible, ``0`` if the new position has a valid item,
	or ``1`` if the new position has no item but one can be inserted here.

	:returns: number

	.. code-block::

		var result = outlineIterator.up()

.. method:: OutlineIterator.prototype.down()

	Move the iterator position "down". The return value is negative if this
	movement is not possible, ``0`` if the new position has a valid item,
	or ``1`` if the new position has no item but one can be inserted here.

	:returns: number

	.. code-block::

		var result = outlineIterator.down()

.. method:: OutlineIterator.prototype.insert(item)

	Insert item before the current item. The position does not change. The
	return value is ``0`` if the current position has a valid item, or
	``1`` if the position has no valid item.

	:param OutlineItem item: the item to insert

	:returns: number

	.. code-block::

		var result = outlineIterator.insert(item)

.. method:: OutlineIterator.prototype.delete()

	Delete the current item. This implicitly moves to the next item. The
	return value is ``0`` if the new position has a valid item, or ``1`` if
	the position contains no valid item, but one may be inserted at this
	position.

	:returns: number

	.. code-block::

		outlineIterator.delete()

.. method:: OutlineIterator.prototype.update(item)

	Updates the current item properties with values from the supplied item's properties.

	:param OutlineItem item: An item populated with the properties that should be stored.

	.. code-block::

		outlineIterator.update(item)
