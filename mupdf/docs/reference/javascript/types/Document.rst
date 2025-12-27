.. default-domain:: js

.. highlight:: javascript

Document
========

MuPDF can open many document types such as PDF, XPS, CBZ, EPUB, FictionBook 2
and a handful of image formats.

.. class:: Document

	|no_new|

Constants
---------

Permission flags for `Document.prototype.hasPermission`:

.. data:: Document.PERMISSION_PRINT

	``"print"`` -- Print the document.

.. data:: Document.PERMISSION_EDIT

	``"edit"`` -- Modify the contents of the document by operations other than those controlled by the other flags: (annotate, form, assemble).

.. data:: Document.PERMISSION_COPY

	``"edit"`` -- Copy or otherwise extract text from the document.

.. data:: Document.PERMISSION_ANNOTATE

	``"annotate"`` -- Add or modify annotations.

.. data:: Document.PERMISSION_FORM

	``"form"`` -- Fill in existing form fields.

.. data:: Document.PERMISSION_ACCESSIBILITY

	``"accessibility"`` --
	Copy or otherwise extract text from the document in support of accessibility.

.. data:: Document.PERMISSION_ASSEMBLE

	``"assemble"`` -- Insert, rotate, or delete pages and create bookmarks.

.. data:: Document.PERMISSION_PRINT_HQ

	``"print-hq"`` -- Print the document to a representation from which a faithful digital
	copy of the PDF content could be generated.

Static methods
--------------

.. function::
	Document.openDocument(filename)
	Document.openDocument(filename, dir)
	Document.openDocument(filename, accelerator, dir)
	Document.openDocument(buffer, magic)
	Document.openDocument(buffer, magic, acceleratorbuffer)
	Document.openDocument(buffer, magic, acceleratorbuffer, dir)

	Open the named or given document.

	:param string filename: File name to open.
	:param Buffer | ArrayBuffer | Uint8Array | string buffer: Buffer containing a document file.
	:param string magic: An optional :term:`MIME-type` or file extension. Defaults to "application/pdf".
	:param string accelerator: File name of accelerator file.
	:param Buffer | ArrayBuffer | Uint8Array | string acceleratorbuffer: Buffer containing an accelerator file.
	:param Archive dir: An archive from which to load resources for rendering.

	:returns: Document

	.. code-block::

		var document1 = mupdf.Document.openDocument("my_pdf.pdf", "application/pdf")
		var document2 = mupdf.Document.openDocument("my_pdf.pdf", dir)
		var document3 = mupdf.Document.openDocument("my_pdf.pdf", acceleratorfile, dir)
		var document4 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), "application/pdf")
		var document5 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), acceleratorbuffer, "application/pdf")
		var document6 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), acceleratorbuffer, dir, "application/pdf")

.. function::
	Document.recognize(magic)
	Document.recognizeContent(filename)
	Document.recognizeContent(buffer, magic)
	Document.recognizeContent(buffer, dir, magic)

	Check if MuPDF can open a document with the provided magic, or with the contents in the given file/buffer.

	:param string magic: An optional :term:`MIME-type` or file extension. Defaults to "application/pdf".
	:param string filename: File name to check contents of.
	:param Buffer | ArrayBuffer | Uint8Array | string buffer: Buffer containing a PDF file.
	:param Archive dir: An archive from which to load resources for rendering.

	:returns: boolean

	.. code-block::

		var recognized1 = mupdf.Document.recognize("application/pdf")
		var recognized2 = mupdf.Document.recognizeContent("my_pdf.pdf")
		var recognized3 = mupdf.Document.recognizeContent(buffer, "application/pdf")
		var recognized4 = mupdf.Document.recognizeContent(buffer, dir, "application/pdf")

Instance methods
----------------

.. method:: Document.prototype.needsPassword()

	Returns ``true`` if a password is required to open a password protected PDF.

	:returns: boolean

	.. code-block::

		var needsPassword = document.needsPassword()

.. method:: Document.prototype.authenticatePassword(password)

	Returns a bitfield value against the password authentication result.

	:param string password: The password to attempt authentication with.

	:returns: number

	.. list-table::
		:header-rows: 1

		* - **Bitfield value**
		  - **Description**
		* - 0
		  - Failed
		* - 1
		  - No password needed
		* - 2
		  - Is User password and is okay
		* - 4
		  - Is Owner password and is okay
		* - 6
		  - Is both User & Owner password and is okay

	.. code-block::

		var auth = document.authenticatePassword("abracadabra")

.. method:: Document.prototype.hasPermission(permission)

	Check if a user is allowed permission to perform certain operations on the document.

	:param "print" | "edit" | "copy" | "annotate" | "form" | "accessibility" | "assemble" | "print-hq" permission:

	See `Document.PERMISSION_PRINT`, etc.


	:returns: boolean

	.. code-block::

		var canEdit1 = document.hasPermission("edit")
		var canEdit2 = document.hasPermission(Document.PERMISSION_EDIT)

.. method:: Document.prototype.getMetaData(key)

	Return various meta data information. The common keys are: format, encryption, info:ModDate, and info:Title. Returns ``undefined`` if the meta data does not exist.

	:param string key: What metadata type to return.

	:returns: string | null

	.. code-block::

		var format = document.getMetaData("format")
		var modificationDate = doc.getMetaData("info:ModDate")
		var author = doc.getMetaData("info:Author")

.. method:: Document.prototype.setMetaData(key, value)

	Set document meta data information field to a new value.

	:param string key: Metadata key to set.
	:param string value: New value to set for the given key.

	.. code-block::

		document.setMetaData("info:Author", "My Name")

.. method:: Document.prototype.isReflowable()

	Returns true if the document is reflowable, such as EPUB, FB2 or XHTML.

	:returns: boolean

	.. code-block::

		var isReflowable = document.isReflowable()

.. method:: Document.prototype.layout(pageWidth, pageHeight, fontSize)

	Layout a reflowable document (EPUB, FictionBook2, HTML or XHTML) to fit
	the specified page and font sizes.

	:param number pageWidth: Desired page width.
	:param number pageHeight: Desired page height.
	:param number fontSize: Desire font size.

	.. code-block::

		document.layout(300, 300, 16)

.. method:: Document.prototype.countPages()

	Count the number of pages in the document. This may change if you call
	the layout function with different parameters.

	:returns: number

	.. code-block::

		var numPages = document.countPages()

.. method:: Document.prototype.loadPage(number)

	Returns a `Page` object for the given page number.

	For documents where `Document.prototype.isPDF()` returns true,
	the returned `Page` is of the subclass `PDFPage`.

	:param number number: Number of page to load, 0 means the first page in the document.

	:returns: `Page` | `PDFPage`.

	.. code-block::

		var page = document.loadPage(0) // loads the 1st page of the document

.. method:: Document.prototype.loadOutline()

	Returns an array with the outline (also known as table of contents or
	bookmarks). In the array is an object for each heading with the
	property 'title', and a property 'page' containing the page number. If
	the object has a 'down' property, it contains an array with all the
	sub-headings for that entry.

	:returns: Array of `OutlineItem` (nested).

	.. code-block::

		var outline = document.loadOutline()

.. method:: Document.prototype.outlineIterator()

	Returns an `OutlineIterator` for the document outline.

	:returns: `OutlineIterator`

	.. code-block::

		var obj = document.outlineIterator()

.. method:: Document.prototype.resolveLink(link)

	Resolve a document internal link URI to a page index.

	:param Link | string link: A link or a link URI string to resolve.

	:returns: number

	.. code-block::

		var pageNumber = document.resolveLink(my_link)

.. method:: Document.prototype.resolveLinkDestination(link)

	Resolve a document internal link URI to a link destination.

	:param Link | string link: A link or a link URI string to resolve.

	:returns: `LinkDestination`

	.. code-block::

		var linkDestination = document.resolveLinkDestination(linkuri)

.. method:: Document.prototype.isPDF()

	Returns ``true`` if the document is a `PDFDocument`.

	:returns: boolean

	.. code-block::

		var isPDF = document.isPDF()

.. method:: Document.prototype.asPDF()

	Returns a PDF version of the document (if possible).
	PDF documents return themselves.
	Documents that have an underlying PDF representation return that.
	Other document types return null.

	:returns: `PDFDocument` | null

	.. code-block::

		var doc = mupdf.Document.openDocument(filename)
		var pdf = doc.asPDF()
		if (pdf) {
			// the document has a native PDF representation
		} else {
			// it does not have a native PDF representation
		}

.. method:: Document.prototype.formatLinkURI(linkDestination)

	Format a document internal link destination object to a URI string suitable for `Page.prototype.createLink()`.

	:param LinkDestination linkDestination: The link destination object to format.

	:returns: string

	.. code-block::

		var uri = document.formatLinkURI({
			chapter: 0,
			page: 42,
			type: "FitV",
			x: 0,
			y: 0,
			width: 100,
			height: 50,
			zoom: 1
		})
		page.createLink([0, 0, 100, 100], uri)
