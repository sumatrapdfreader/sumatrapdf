.. default-domain:: js

.. highlight:: javascript

Search
======

Search objects hold the current search position in a `Document`, the search
pattern and the options related to a search in a `StructuredText` object.

Constructors
------------

.. class:: Search(needle, options)

	Create a new search looking for the specified pattern according to the given options.

	:param string needle: Pattern to search for.
	:param string options: A comma-separated string of search options, see :doc:`/reference/common/search-options`.

	.. code-block:

		var search1 = new mupdf.Search("hello", "exact");
		var search2 = new mupdf.Search("hello", "exact,ignore-case");
		var search3 = new mupdf.Search("bår", "ignore-case,ignore-diacritics");

Constants
---------

.. data:: Search.MORE_INPUT

	indicates that `Search.prototype.feedPage()` should be called with the `StructuredText` (or ``null`` if not available) of the desired page.

.. data:: Search.MATCH

	indicates that a search match has been found.

.. data:: Search.COMPLETE

	indicatres that the search has reached either the end, or the beginning, of the document without any found match.

Instance methods
----------------

.. method:: Search.prototype.feedPage(stextPage, pageNumber)

	Feed given `StructuredText` page with the given page number to the search.

	This can be done for the initial page before `Search.prototype.searchForwards()` or `Search.prototype.searchBackwards()` are called,
	but is normally done whenever either of those two functions returns

	Each match in the result is an array containing one or more Quads that cover the matching text.

	.. code-block::

		var result = searchForwards();
		if (result.reason == Search.MORE_INPUT)
			search.feedPage(doc.loadPage(result.needPage).toStructuredText(), result.needPage);

.. method:: Search.prototype.searchForwards()

	Searches forwards from the current position in the document for the next match, if any.

	:returns: `SearchResult`

	.. code-block::

		var result = search.searchForwards()

.. method:: Search.prototype.searchBackwards()

	Searches backwards from the current position in the document for the next match, if any.

	:returns: `SearchResult`

	.. code-block::

		var result = search.searchBackwards()

Example
-------

.. code-block::

	var filename = "pdfref17.pdf"
	var doc = Document.openDocument(filename);

	var search = new Search("baseline", "ignore-case");
	var firstPageToSearch = 500;
	search.feedPage(doc.loadPage(firstPageToSearch).toStructuredText(), firstPageToSearch);

	while (true)
	{
		var result = search.searchForwards();
		if (result.reason == Search.COMPLETE)
			break;
		else if (result.reason == Search.MORE_INPUT)
		{
			if (result.needPage >= 0 && result.needPage < doc.countPages())
				search.feedPage(doc.loadPage(result.needPage).toStructuredText(), result.needPage);
			else
				search.feedPage(null, result.needPage);
		}
		else if (result.reason == Search.MATCH)
		{
			var begin = result.begin;
			var end = result.end;
			var page = -1;
			var block = -1;
			var line = -1;
			var char = -1;
			var capture = false;
			var str = "";
			var walker = {
				beginTextBlock: function (rect, props) {
					block++;
				},
				beginLine: function (rect, wmode, dir) {
					line++;
					str = "";
					if (page == begin.page && block == begin.block && line == begin.line)
						capture = true;
				},
				onChar: function (utf, origin, font, size, quad, color) {
					char++;
					if (page == begin.page && block == begin.block && line == begin.line && char == begin.char)
						str = str + "{";
					str = str + utf;
					if (page == end.page && block == end.block && line == end.line && char == end.char)
						str = str + "}";
				},
				endLine: function () {
					char = -1;
					if (capture)
						console.log(filename + ":" + (page+1) + ":" + (block+1) + ":" + (line+1) + ":" + str);
					if (page == end.page && block == end.block && line == end.line)
						capture = false;
				},
				endTextBlock: function () {
					line = -1;
				},
			};

			for (var pageno = begin.page; pageno <= end.page; pageno++)
			{
				page = pageno;
				block = -1;
				doc.loadPage(pageno).toStructuredText().walk(walker);
			}
		}
	}
