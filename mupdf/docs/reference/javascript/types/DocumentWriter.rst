.. default-domain:: js

.. highlight:: javascript

DocumentWriter
===================

DocumentWriter objects are used to create new documents in several formats.

Constructors
------------

.. class::
	DocumentWriter(buffer, format, options)
	DocumentWriter(filename, format, options)

	Create a new document writer to create a document with the specified format and output options. If ``format`` is ``null`` it is inferred from the filename. The ``options`` argument is a comma separated list of flags and key-value pairs.

	For supported output ``format`` values and ``options`` see
	:doc:`/reference/common/document-writer-options`.

	:param Buffer buffer: The buffer to output to.
	:param Buffer filename: The file name to output to.
	:param string format: The file format.
	:param string options: The options as key-value pairs.

	.. code-block::

		var writer = new mupdf.DocumentWriter(buffer, "PDF", "")

Instance methods
----------------

.. method:: DocumentWriter.prototype.beginPage(mediabox)

	Begin rendering a new page. Returns a `Device` that can be used to render the page graphics.

	:param Rect mediaBox: The page size.

	:returns: `Device`

	.. code-block::

		var device = writer.beginPage([0, 0, 100, 100])

.. method:: DocumentWriter.prototype.endPage()

	Finish the page rendering.

	.. code-block::

		writer.endPage()

.. method:: DocumentWriter.prototype.close()

	Finish the document and flush any pending output. It is a
	requirement to make this call to ensure that the output file is
	complete.

	.. code-block::

		writer.close()
