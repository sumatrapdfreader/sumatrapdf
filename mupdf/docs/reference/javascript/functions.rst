.. default-domain:: js

.. highlight:: javascript

Functions
=========

Most functionality is provided by member functions of class objects.
There are just a couple of top level configuration functions listed
here.

.. function:: mupdf.installLoadFontFunction(callback)

	Install a handler to load system (or missing) fonts.

	The callback function will be called with four arguments:

	.. code-block::

		callback(fontName, scriptName, isBold, isItalic)

	The callback should return either a `Font` object for the requested
	font, or ``null`` if an exact match cannot be found (so that the font
	loading machinery can keep looking through the chain of fallback
	fonts).

.. function:: mupdf.enableICC()

	Enable ICC-profiles based operation.

.. function:: mupdf.disableICC()

	Disable ICC-profiles based operation.

.. function:: mupdf.emptyStore()

	Empty all cached entries from the store.

.. function:: mupdf.shrinkStore(percent)

	Remove cached entries from the store until it holds less
	data than the specified threshold.

	If the store was initialized with a limit, the threshold is a
	percentage of the limit. If the store is unlimited in size, the
	threshold is a percentage of what the store currently holds.

	Returns a boolean indicating whether or not the store was able to be
	shrunk to below the threshold.

	:param number percent:
	:returns: boolean

.. function:: mupdf.setUserCSS(stylesheet, useDocumentStyles)

	Set a style sheet to apply to all reflowable documents.

	:param string stylesheet: The CSS text to use.
	:param boolean useDocumentStyles:
		Whether to respect the document's own style sheet.
