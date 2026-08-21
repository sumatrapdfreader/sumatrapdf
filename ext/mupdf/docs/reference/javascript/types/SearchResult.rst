.. default-domain:: js

.. highlight:: javascript

SearchResult
============

A SearchResult has properties describing the current state of a `Search`:

	* does it need more pages?
	* has it found a search?
	* did it reach the end of the document?

Constructors
------------

.. class:: SearchResult()

	|no_new|

	Returned by `Search.prototype.searchForwards()` and `Search.prototype.searchBackwards()`.

Instance properties
-------------------

.. attribute:: SearchResult.prototype.reason

	The reason the search result was returned, one of `Search.MORE_INPUT`, `Search.MATCH`, or `Search.COMPLETE`

.. attribute:: SearchResult.prototype.needPage

	Set when `SearchResult.prototype.reason` is equal to `Search.MORE_INPUT`.

	Denotes the page number `Search` needs to progress the search.

.. attribute:: SearchResult.prototype.begin

	Set when `SearchResult.prototype.reason` is equal to `Search.MATCH`,

	It is an object that denotes the first character of the match, which has properties for the page number, the block index on that page, the line index in that block and the character index within that line. The indices refer to the blocks in the `StructuredText` fed into `Search.prototype.feedPage()` for that page.

.. attribute:: SearchResult.prototype.end

	Set when `SearchResult.prototype.reason` is equal to `Search.MATCH`.

	It is an object that denotes the last character of the match, which has properties for the page number, the block index on that page, the line index in that block and the character index within that line. The indices refer to the blocks in the `StructuredText` fed into `Search.prototype.feedPage()` for that page.

.. attribute:: SearchResult.prototype.quads

	Set when `SearchResult.prototype.reason` is equal to `Search.MATCH`.

	Contains an array of objects, each with properties for a `Quad` covering part of the match and the page number the quad appears on.
