.. default-domain:: js

.. highlight:: javascript

PDFDocument
===========

The PDFDocument is a specialized subclass of `Document` which has
additional methods that are only available for PDF files.

PDF Objects
-----------

A PDF document contains objects: dictionaries, arrays, names, strings, numbers,
booleans, and indirect references.
Some dictionaries also have attached data. These are called streams,
and may be compressed.

At the root of the PDF document is the trailer object; which contains pointers to the meta
data dictionary and the catalog object, which in turn contains references to the pages and
forms and everything else.

Pointers in PDF are called indirect references, and are of the form
32 0 R (where 32 is the object number, 0 is the generation, and R is
magic syntax). All functions in MuPDF dereference indirect
references automatically.

	PDFObjects are always bound to the document that created them. Do
	**NOT** mix and match objects from one document with another
	document!

Constructors
------------

.. class:: PDFDocument()

	Create a brand new PDF document instance that begins empty with no pages.

To open an existing PDF document, use `Document.openDocument()`.

Static methods
--------------

.. method:: PDFDocument.formatURIFromPathAndDest(path, destination)

	|only_mutool|

	Format a link URI given a
	`system independent path <https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G8.1640868>`_
	to a remote document and a destination object or a destination
	string suitable for `Page.prototype.createLink()`.

	:param string path: An absolute or relative path to a remote document file.
	:param Link | string destination: Link or string referring to a destination using either a destination object or a destination name in the remote document.

.. method:: PDFDocument.appendDestToURI(uri, destination)

	|only_mutool|

	Append a fragment representing a document destination to a an
	existing URI that points to a remote document. The resulting
	string is suitable for `Page.prototype.createLink()`.

	:param string uri: An URI to a remote document file.
	:param Link | string | number destination: A Link or string referring to a destination using either a destination object or a destination name in the remote document, or a page number.

Instance methods
----------------

.. method:: PDFDocument.prototype.needsPassword()

	Returns true if a password is required to open a password protected PDF.

	:returns: boolean

	.. code-block::

		var needsPassword = document.needsPassword()

.. method:: PDFDocument.prototype.authenticatePassword(password)

	Returns a bitfield value against the password authentication result.

	The values returned by this interface are interpreted like this:

	.. table::
		:align: left

		=======	===========
		Bit	Description
		=======	===========
		0	Failed
		1	No password needed
		2	User password is okay
		4	Owner password is okay
		=======	===========

	:param string password: The password to attempt authentication with.

	:returns: number

	.. code-block::

		var auth = document.authenticatePassword("abracadabra")

.. method:: PDFDocument.prototype.hasPermission(permission)

	Returns true if the document has permission for the supplied permission parameter.

	These are the recognized permission strings:

	.. table::
		:align: left

		===============	==========================================================
		String		The Document may...
		===============	==========================================================
		print		... be printed.
		edit		... be edited.
		copy		... be copied.
		annotate	... have annotations added/removed.
		form		... have form field contents edited.
		accessibility	... be copied for accessibility.
		assemble	... have its pages rearranged.
		print-hq	... be printed in high quality be printed in high quality.
		===============	==========================================================

	:param string permission: The permission to seek for, e.g. "edit".

	:returns: boolean

	.. code-block::

		var canEdit = document.hasPermission("edit")

.. method:: PDFDocument.prototype.getVersion()

	Returns the PDF document version as an integer multiplied by
	10, so e.g. a PDF-1.4 document would return 14.

	:returns: number

	.. code-block::

		var version = pdfDocument.getVersion()

.. method:: PDFDocument.prototype.setLanguage(lang)

	Set the document's :term:`language code`.

	:param string lang:

	.. code-block::

		pdfDocument.setLanguage("en")

.. method:: PDFDocument.prototype.getLanguage()

	Get the document's :term:`language code`.

	:returns: string | null

	.. code-block::

		var lang = pdfDocument.getLanguage()

.. method:: PDFDocument.prototype.wasPureXFA()

	|only_mutool|

	Returns whether the document was an XFA form without AcroForm
	fields.

	:returns: boolean

	.. code-block::

		var wasPureXFA = pdfDocument.wasPureXFA()

.. method:: PDFDocument.prototype.wasRepaired()

	Returns whether the document was repaired when opened.

	:returns: boolean

	.. code-block::

		var wasRepaired = pdfDocument.wasRepaired()

.. method:: PDFDocument.prototype.loadNameTree(treeName)

	Return an object whose properties and their values come from
	corresponding names/values from the given name tree.

	:returns: Object

	.. code-block::

		var dests = pdfDocument.loadNameTree("Dests")
		for (var p in dests) {
			console.log("Destination: " + p)
		}

Objects
-------------------

.. method:: PDFDocument.prototype.newNull()

	Create a new null object.

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newNull()

.. method:: PDFDocument.prototype.newBoolean(v)

	Create a new boolean object.

	:param boolean v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newBoolean(true)

.. method:: PDFDocument.prototype.newInteger(v)

	Create a new integer object.

	:param number v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newInteger(1)

.. method:: PDFDocument.prototype.newReal(v)

	Create a new real number object.

	:param number v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newReal(7.3)

.. method:: PDFDocument.prototype.newString(v)

	Create a new string object.

	:param string v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newString("hello")

.. method:: PDFDocument.prototype.newByteString(v)

	Create a new byte string object.

	:param Uint8Array | Array of number v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newByteString([21, 31])

.. method:: PDFDocument.prototype.newName(v)

	Create a new name object.

	:param string v:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newName("hello")

.. method:: PDFDocument.prototype.newIndirect(objectNumber, generation)

	Create a new indirect object.

	:param number objectNumber:
	:param number generation:

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newIndirect(42, 0)

.. method:: PDFDocument.prototype.newArray()

	Create a new array object.

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newArray()

.. method:: PDFDocument.prototype.newDictionary()

	Create a new dictionary object.

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.newDictionary()

Indirect objects
----------------

.. method:: PDFDocument.prototype.getTrailer()

	The trailer dictionary. This contains indirect references to the "Root"
	and "Info" dictionaries.

	:returns: `PDFObject`

	.. code-block::

		var dict = doc.getTrailer()

.. method:: PDFDocument.prototype.countObjects()

	Return the number of objects in the PDF.

	:returns: number

	.. code-block::

		var num = doc.countObjects()

.. method:: PDFDocument.prototype.createObject()

	Allocate a new numbered object in the PDF, and return an indirect
	reference to it. The object itself is uninitialized.

	:returns: `PDFObject`

	.. code-block::

		var obj = doc.createObject()

.. method:: PDFDocument.prototype.deleteObject(num)

	Delete the object referred to by an indirect reference or its object number.

	:param PDFObject | number num: Delete the referenced object number.

	.. code-block::

		doc.deleteObject(obj)

.. method:: PDFDocument.prototype.addObject(obj)

	Add obj to the PDF as a numbered object, and return an indirect reference to it.

	:param PDFObject obj: Object to add.

	:returns: `PDFObject`

	.. code-block::

		var ref = doc.addObject(obj)

.. method:: PDFDocument.prototype.addStream(buf, obj)

	Create a stream object with the contents of buffer, add it to the PDF, and return an indirect reference to it. If object is defined, it will be used as the stream object dictionary.

	:param Buffer | ArrayBuffer | Uint8Array | string buf: Buffer whose data to put into stream.
	:param PDFObject obj: The object to add the stream to.

	:returns: `PDFObject`

	.. code-block::

		var stream = doc.addStream(buffer, object)

.. method:: PDFDocument.prototype.addRawStream(buf, obj)

	Create a stream object with the contents of buffer, add it to the PDF, and return an indirect reference to it. If object is defined, it will be used as the stream object dictionary. The buffer must contain already compressed data that matches "Filter" and "DecodeParms" set in the stream object dictionary.

	:param Buffer | ArrayBuffer | Uint8Array | string buf: Buffer whose data to put into stream.
	:param PDFObject obj: The object to add the stream to.

	:returns: `PDFObject`

	.. code-block::

		var stream = doc.addRawStream(buffer, object)

Page Tree
-----------------

.. method:: PDFDocument.prototype.setPageTreeCache(enabled)

	Enable or disable the page tree cache that is used to speed up page object lookups.
	The page tree cache is used unless explicitly disabled with this function.

	Disabling the page tree cache reduces the number objects that we need to read from the file when loading a single page.
	However it will make page lookups slower overall!

	:param boolean enabled:

.. method:: PDFDocument.protoype.findPage(number)

	Return the `PDFObject` for a page number.

	:param number number: The page number, the first page is number zero.

	:throws: Error on out of range page numbers.

	:returns: `PDFObject`

	.. code-block::

		var obj = pdfDocument.findPage(0)

.. method:: PDFDocument.prototype.findPageNumber(page)

	|only_mutool|

	Find a given `PDFPage` and return its page number.
	If the page can not be found, returns -1.

	:param PDFPage page:

	:returns: number

	.. code-block::

		var pageNumber = pdfDocument.findPageNumber(page)

.. method:: PDFDocument.prototype.lookupDest(obj)

	|only_mutool|

	Find the destination corresponding to a specific named
	destination given as a name or byte string in the form of a
	`PDFObject`.

	Returns null if the named destination does not exist.

	:param PDFObject obj:

	:returns: `PDFObject` | null

	.. code-block::

		var destination = pdfDocument.lookupDest(nameobj)

.. method:: PDFDocument.prototype.rearrangePages(pages)

	Rearrange (re-order and/or delete) pages in the PDFDocument.

	The pages in the document will be rearranged according to the
	input list. Any pages not listed will be removed, and pages may
	be duplicated by listing them multiple times.

	The PDF objects describing removed pages will remain in the
	file and take up space (and can be recovered by forensic tools)
	unless you save with the "garbage" option set, see `PDFDocument.prototype.save()`.

	:param Array of number pages: An array of page numbers, each page number is 0-based.

	.. code-block::

		var document = new Document.openDocument("my_pdf.pdf")
		pdfDocument.rearrangePages([3,2])
		pdfDocument.save("fewer_pages.pdf", "garbage")

.. method:: PDFDocument.prototype.insertPage(at, page)

	Insert the page's `PDFObject` into the page tree at the page
	number specified by ``at`` (numbered from 0). If ``at`` is -1,
	the page is inserted at the end of the document.

	:param number at: The index to insert at.
	:param PDFObject page: The PDFObject representing the page to insert.

	.. code-block::

		pdfDocument.insertPage(-1, page)

.. method:: PDFDocument.prototype.deletePage(index)

	Delete the page at the given index.

	:param number index: The page number, the first page is number zero.

	.. code-block::

		pdfDocument.deletePage(0)

.. method:: PDFDocument.prototype.addPage(mediabox, rotate, resources, contents)

	Create a new `PDFPage` object. Note: this function does NOT add
	it to the page tree, use `PDFDocument.prototype.insertPage()`
	to do that.

	Creation of page contents is described in detail in the PDF
	specification's section on `Content Streams
	<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G8.1913072>`_.

	:param Rect mediabox: Describes the dimensions of the page.
	:param number rotate: Rotation value.
	:param PDFObject resources: Resources dictionary object.
	:param Buffer | ArrayBuffer | Uint8Array | string contents: Contents string. This represents the page content stream.

	:returns: `PDFObject`

	.. code-block::

		var helvetica = pdfDocument.addSimpleFont(new mupdf.Font("Helvetica"), "Latin")
		var fonts = pdfDocument.newDictionary()
		fonts.put("F1", helvetica)
		var resources = pdfDocument.addObject(pdfDocument.newDictionary())
		resources.put("Font", fonts)
		var pageObject = pdfDocument.addPage(
			[0,0,300,350],
			0,
			resources,
			"BT /F1 12 Tf 100 100 Td (Hello, world!) Tj ET"
		)
		pdfDocument.insertPage(-1, pageObject)

Resources
-----------------

.. method:: PDFDocument.prototype.addSimpleFont(font, encoding)

	Create a `PDFObject` from the `Font` object as a simple font.

	:param Font font:
	:param "Latin" | "Greek" | "Cyrillic" encoding: Which 8-bit encoding to use. Defaults to "Latin".

	See `Font.SIMPLE_ENCODING_LATIN`, etc.

	:returns: `PDFObject`

	.. code-block::

		var obj = pdfDocument.addSimpleFont(new mupdf.Font("Times-Roman"), "Latin")

.. method:: PDFDocument.prototype.addCJKFont(font, language, wmode, style)

	Create a `PDFObject` from the `Font` object as a UTF-16 encoded
	CID font for the given language ("zh-Hant", "zh-Hans", "ko", or
	"ja"), writing mode ("H" or "V"), and style ("serif" or
	"sans-serif").

	:param Font font:
	:param string language:
	:param number wmode: ``0`` for horizontal writing, and ``1`` for vertical writing.
	:param string style:

	:returns: `PDFObject`

	.. code-block::

		var obj = pdfDocument.addCJKFont(new mupdf.Font("ja"), "ja", 0, "serif")

.. method:: PDFDocument.prototype.addFont(font)

	Create a `PDFObject` from the `Font` object as an Identity-H
	encoded CID font.

	:param Font font:

	:returns: `PDFObject`

	.. code-block::

		var obj = pdfDocument.addFont(new mupdf.Font("Times-Roman"))

.. method:: PDFDocument.prototype.addImage(image)

	Create a `PDFObject` from the `Image` object.

	:param Image image:

	:returns: `PDFObject`

	.. code-block::

		var obj = pdfDocument.addImage(new mupdf.Image(pixmap))

.. method:: PDFDocument.prototype.loadImage(obj)

	Load an `Image` from a `PDFObject` (typically an indirect
	reference to an image resource).

	:param PDFObject obj:

	:returns: `Image`

	.. code-block::

		var image = pdfDocument.loadImage(obj)

Embedded/Associated files
-------------------------

.. method:: PDFDocument.protoype.addEmbeddedFile(filename, mimetype, contents, creationDate, modificationDate, addChecksum)

	Embedded a file into the document. If a checksum is added then
	the file contents can be verified later. An indirect reference
	to a :term:`file specification` object is returned.

	The returned :term:`file specification` object can later be e.g.
	connected to an annotation using
	`PDFAnnotation.prototype.setFilespec()`.

	:param string filename:
	:param string mimetype: The :term:`MIME-type`.
	:param Buffer | ArrayBuffer | Uint8Array | string contents:
	:param Date creationDate:
	:param Date modificationDate:
	:param boolean addChecksum: Defaults to false.

	:returns: `PDFObject`

	.. code-block::

		var fileSpecObject = pdfDocument.addEmbeddedFile(
			"my_file.jpg",
			 "image/jpeg",
			 buffer,
			 new Date(),
			 new Date(),
			 false
		)

.. method:: PDFDocument.prototype.getEmbeddedFiles()

	Returns a record of any embedded files on the this PDFDocument.

	:returns: Record<string, PDFObject>

.. method:: PDFDocument.prototype.deleteEmbeddedFile(filename)

	Delete an embedded file by filename.

	:param string filename: Name of embedded file to delete.

	.. code-block::

		doc.deleteEmbeddedFile("test.txt")

.. method:: PDFDocument.prototype.insertEmbeddedFile(filename, fileSpecObject)

	Insert the given file specification as an embedded file using
	the given filename.

	:param string filename: Name of the file to insert.
	:param PDFObject fileSpecObject: :term:`File specification`.

	.. code-block::

		pdfDocument.insertEmbeddedFile("test.txt", fileSpecObject)
		pdfDocument.deleteEmbeddedFile("test.txt")

.. method:: PDFDocument.prototype.getFilespecParams(fileSpecObject)

	Get the file specification parameters from the :term:`file specification`.

	:param PDFObject fileSpecObject: :term:`file specification` object.

	:returns: `PDFFilespecParams`

	.. code-block::

		var obj = pdfDocument.getFilespecParams(fileSpecObject)

.. method:: PDFDocument.prototype.getEmbeddedFileContents(fileSpecObject)

	Returns a `Buffer` with the contents of the embedded file
	referenced by ``fileSpecObject``.

	:param Object fileSpecObject: :term:`file specification`

	:returns: `Buffer` | null

	.. code-block::

		var buffer = pdfDocument.getEmbeddedFileContents(fileSpecObject)

.. method:: PDFDocument.prototype.verifyEmbeddedFileChecksum(fileSpecObject)

	|only_mutool|

	Verify the MD5 checksum of the embedded file contents.

	:param Object fileSpecObject: :term:`file specification`.

	:returns: boolean

	.. code-block::

		var fileChecksumValid = pdfDocument.verifyEmbeddedFileChecksum(fileSpecObject)

.. method:: PDFDocument.prototype.isFilespec(object)

	|only_mutool|

	Check if the given ``object`` is a :term:`file specification`.

	:param PDFObject object:

	:returns: boolean

	.. code-block::

		var isFilespec = pdfDocument.isFilespec(obj)

.. method:: PDFDocument.prototype.isEmbeddedFile(object)

	Check if the given ``object`` is a :term:`file specification` representing a file
	embedded into the PDF document.

	:param PDFObject object:

	:returns: boolean

	.. code-block::

		var isFilespecObject = pdfDocument.isEmbeddedFile(obj)

.. method:: PDFDocument.prototype.countAssociatedFiles()

	|only_mutool|

	Return the number of :term:`associated files <associated file>`
	associated with this document. Note that this is the number of
	files associated at the document level, not necessarily the
	total number of files associated with elements throughout the
	entire document.

	:returns: number

	.. code-block::

		var count = pdfDocument.countAssociatedFiles()

.. method:: PDFDocument.prototype.associatedFile(n)

	|only_mutool|

	Return the :term:`file specification` object that represents the nth
	:term:`associated file` for this document.

	``n`` should be in the range ``0 <= n < countAssociatedFiles()``.

	Returns null if no associated file exists or index is out of range.

	:returns: `PDFObject` | null

	.. code-block::

		var obj = pdfDocument.associatedFile(0)

Grafting
----------

.. method:: PDFDocument.prototype.newGraftMap()

	Create a graft map on the destination document, so that objects that have already been copied can be found again. Each graft map should only be used with one source document. Make sure to create a new graft map for each source document used.

	:returns: `PDFGraftMap`

	.. code-block::

		var graftMap = doc.newGraftMap()

.. method:: PDFDocument.prototype.graftObject(obj)

	Deep copy an object into the destination document. This function will
	not remember previously copied objects. If you are copying several
	objects from the same source document using multiple calls, you
	should use a graft map instead, see
	`PDFDocument.prototype.newGraftMap()`.

	:param PDFObject obj: The object to graft.

	:returns: `PDFObject`

	.. code-block::

		var copiedObj = doc.graftObject(obj)

.. method:: PDFDocument.prototype.graftPage(to, srcDoc, srcPage)

	Graft a page and its resources at the given page number from the source document to the requested page number in the document.

	:param number to: The page number to insert the page before. Page numbers start at 0 and -1 means at the end of the document.
	:param PDFDocument srcDoc: Source document.
	:param number srcPage: Source page number.

	This would copy the first page of the source document (0) to the last page (-1) of the current PDF document.

	.. code-block::

		doc.graftPage(-1, srcDoc, 0)

Journalling
-----------

.. method:: PDFDocument.prototype.enableJournal()

	Activate journalling for the document.

	.. code-block::

		pdfDocument.enableJournal()

.. method:: PDFDocument.prototype.getJournal()

	Returns a Javascript object with a property indicating the
	current position, and the names of each entry in the
	undo/redo journal history:

	``{ position: number, steps: Array of string }``

	:returns: Object

	.. code-block::

		var journal = pdfDocument.getJournal()

.. method:: PDFDocument.prototype.beginOperation(op)

	Begin a journal operation. Each call to begin an operation
	should be paired with a call to either
	`PDFDocument.prototype.endOperation()` if the operation was
	successful, or to `PDFDocument.prototype.abandonOperation()` if
	it failed.

	:param string op: The name of the operation.

	.. code-block::

		pdfDocument.beginOperation("Change annotation color")
		// Change the annotation
		pdfDocument.endOperation()

.. method:: PDFDocument.prototype.beginImplicitOperation()

	Begin an implicit journal operation. Implicit operations are
	operations that happen due to other operations, and that should
	not be subdivided into separate undo steps. E.g. editing several
	attributes of a `PDFAnnotation` might be desirable to do in a
	single undo step. See `PDFDocument.prototype.beginOperation()`
	for the requirements about paired calls.

	.. code-block::

		pdfDocument.beginOperation("Complex operation")
		pdfDocument.beginImplicitOperation()
		pdfDocument.endOperation()
		pdfDocument.beginImplicitOperation()
		pdfDocument.endOperation()
		pdfDocument.endOperation()

.. method:: PDFDocument.prototype.endOperation()

	End a previously started normal or implicit operation. After
	this it can be `undone <PDFDocument.prototype.undo()>` and
	`redone <PDFDocument.prototype.redo()>`.

	.. code-block::

		pdfDocument.endOperation()

.. method:: PDFDocument.prototype.abandonOperation()

	Abandon a normal or implicit operation. Reverts to the state
	before that operation began. This is normally called if an
	operation failed for some reason.

	.. code-block::

		pdfDocument.abandonOperation()

.. method:: PDFDocument.prototype.canUndo()

	Returns whether undo is possible in this state.

	:returns: boolean

	.. code-block::

		var canUndo = pdfDocument.canUndo()

.. method:: PDFDocument.prototype.canRedo()

	Returns whether redo is possible in this state.

	:returns: boolean

	.. code-block::

		var canRedo = pdfDocument.canRedo()

.. method:: PDFDocument.prototype.undo()

	Move backwards in the undo history. Changes to the document
	after this call will throw away all subsequent undo history.

	.. code-block::

		pdfDocument.undo()

.. method:: PDFDocument.prototype.redo()

	Move forwards in the undo history.

	.. code-block::

		pdfDocument.redo()

.. method:: PDFDocument.prototype.saveJournal(filename)

	|only_mutool|

	Save the undo/redo journal to a file.

	:param string filename: File to save the journal to.

	.. code-block::

		pdfDocument.saveJournal("test.journal")

Layers
------

.. method:: PDFDocument.prototype.countLayerConfigs()

	Return the number of optional content layer configurations in this document.

	:returns: number

	.. code-block::

		var configs = pdfDocument.countLayerConfigs()

.. method:: PDFDocument.prototype.getLayerConfigName(n)

	Return the name of configuration number ``n``, where ``n`` is
	``0 <= n < countLayerConfigs()``.

	:returns: string

	.. code-block::

		var name = pdfDocument.getLayerConfigName(0)

.. method:: PDFDocument.prototype.getLayerConfigInfo(n)

	Return the creator of configuration number ``n``, where ``n`` is
	``0 <= n < countLayerConfigs()``.

	:returns: string

	.. code-block::

		var creator = pdfDocument.getLayerConfigCreator(0)

.. method:: PDFDocument.prototype.selectLayerConfig(n)

	Select layer configuration number ``n``, where ``n`` is
	``0 <= n < countLayerConfigs()``.

	.. code-block::

		var info = pdfDocument.selectLayerConfig(1)

.. method:: PDFDocument.prototype.countLayerConfigUIs()

	Return the number of optional content layer UI elements in this document
	given the selected optional content layer configuration.

	:returns: number

.. method:: PDFDocument.prototype.getLayerConfigUIInfo(n)

	Return the information about optional content layer UI element number ``n``,
	where ``n`` is ``0 <= n < countLayerConfigUIs()``.

	:returns: ``{ type: number, depth: number, selected: boolean, locked: boolean, text: string }``

.. method:: PDFDocument.prototype.countLayers()

	Return the number of optional content layers in this document.

	:returns: number

	.. code-block::

		var layers = pdfDocument.countLayers()

.. method:: PDFDocument.prototype.isLayerVisible(n)

	Return whether layer ``n`` is visible, where ``n`` should be in
	the interval ``0 <= n < countLayers()``.

	:param number n: What layer to check visibility of.

	:returns: boolean

	.. code-block::

		var visible = pdfDocument.isLayerVisible(1)

.. method:: PDFDocument.prototype.setLayerVisible(n, visible)

	Set layer ``n`` to be visible or invisible, where ``n`` is
	in the interval ``0 <= n < countLayers()``.

	Pages affected by a visibility change, need to be processed
	again for the layers to be visible/invisible.

	:param number n: What layer to change visibility for.
	:param boolean visible: Whether the layer should be visible.

	:returns: number

	.. code-block::

		pdfDocument.setLayerVisible(1, true)

.. method:: getLayerName(n)

	Return the name of layer number ``n``, where ``n`` is
	``0 <= n < countLayers()``.

	:returns: string

	.. code-block::

		var name = pdfDocument.getLayerName(0)

Page Labels
-----------

.. method:: PDFDocument.prototype.setPageLabels(index, style, prefix, start)

	Sets the page label numbering for the page and all pages following it, until the next page with an attached label.

	:param number index: The start page index to start labeling from.
	:param string style: Can be one of the following strings: "" (none), "D" (decimal), "R" (roman numerals upper-case), "r" (roman numerals lower-case), "A" (alpha upper-case), or "a" (alpha lower-case).
	:param string prefix: Define a prefix for the labels.
	:param number start: The ordinal with which to start numbering.

	.. code-block::

		doc.setPageLabels(0, "D", "Prefix", 1)

.. method:: PDFDocument.prototype.deletePageLabels(index)

	Removes any associated page label from the page.

	:param number index:

	.. code-block::

		doc.deletePageLabels(0)

Saving
------------

.. method:: PDFDocument.prototype.check()

	Check the file for syntax errors, and run a repair pass if any are
	found. This is a costly operation, but may be necessary to prevent any
	changes to a document from being potentially lost.

	If a syntax error is discovered after a file has been edited, those
	edits may be lost during the file repair pass.
	In practice this rarely happens because syntax errors that trigger a
	repair usually happen either when first opening the document or when
	loading a page; but you can never be certain!

.. method:: PDFDocument.prototype.canBeSavedIncrementally()

	Returns whether the document can be saved incrementally, e.g.
	repaired documents or applying redactions prevents incremental
	saves.

	:returns: boolean

	.. code-block::

		var canBeSavedIncrementally = pdfDocument.canBeSavedIncrementally()

.. method:: PDFDocument.prototype.saveToBuffer(options)

	Saves the document to a Buffer.

	:param string options: See :doc:`/reference/common/pdf-write-options`.

	:returns: `Buffer`

	.. code-block::

		var buffer = doc.saveToBuffer("garbage=2,compress=yes")

.. method:: PDFDocument.prototype.save(filename, options)

	Saves the document to a file.

	:param string filename:
	:param string options: See :doc:`/reference/common/pdf-write-options`

	.. code-block::

		doc.save("out.pdf", "incremental")

.. method:: PDFDocument.prototype.countVersions()

	Returns the number of versions of the document in a PDF file,
	typically 1 + the number of updates.

	:returns: number

	.. code-block::

		var versionNum = pdfDocument.countVersions()

.. method:: PDFDocument.prototype.hasUnsavedChanges()

	Returns true if the document has been changed since it was last
	opened or saved.

	:returns: boolean

	.. code-block::

		var hasUnsavedChanges = pdfDocument.hasUnsavedChanges()

.. method:: PDFDocument.prototype.countUnsavedVersions()

	Returns the number of unsaved updates to the document.

	:returns: number

	.. code-block::

		var unsavedVersionNum = pdfDocument.countUnsavedVersions()

.. method:: PDFDocument.prototype.validateChangeHistory()

	Check the history of the document, and determine the last
	version that checks out OK. Returns ``0`` if the entire history
	is OK, ``1`` if the next to last version is OK, but the last
	version has issues, etc.

	:returns: number

	.. code-block::

		var changeHistory = pdfDocument.validateChangeHistory()

Processing
-------------

.. method:: PDFDocument.prototype.subsetFonts()

	Scan the document and establish which glyphs are used from each
	font, next rewrite the font files such that they only contain
	the used glyphs. By removing unused glyphs the size of the font
	files inside the PDF will be reduced.

	.. code-block::

		pdfDocument.subsetFonts()

.. method:: PDFDocument.prototype.bake(bakeAnnots, bakeWidgets)

	*Baking* a document changes all the annotations and/or form fields (otherwise known as widgets) in the document into static content. It "bakes" the appearance of the annotations and fields onto the page, before removing the interactive objects so they can no longer be changed.

	Effectively this removes the "annotation or "widget" type of these objects, but keeps the appearance of the objects.

	:param boolean bakeAnnots: Whether to bake annotations or not. Defaults to true.
	:param boolean bakeWidgets: Whether to bake widgets or not. Defaults to true.

AcroForm Javascript
-------------------

.. method:: PDFDocument.prototype.enableJS()

	Enable interpretation of document Javascript actions.

	.. code-block::

		pdfDocument.enableJS()

.. method:: PDFDocument.prototype.disableJS()

	Disable interpretation of document Javascript actions.

	.. code-block::

		pdfDocument.disableJS()

.. method:: PDFDocument.prototype.isJSSupported()

	Returns whether interpretation of document Javascript actions
	is supported. Interpretation of Javascript may be disabled at
	build time.

	:returns: boolean

	.. code-block::

		var jsIsSupported = pdfDocument.isJSSupported()

.. method:: PDFDocument.prototype.setJSEventListener(listener)

	|only_mutool|

	Calls the listener whenever a document Javascript action
	triggers an event.

	At present the only callback the listener will be used for
	is an alert event.

	:param Object listener: The Javascript listener function.

	.. code-block::

		pdfDocument.setJSEventListener({
				onAlert: function(message) {
						print(message)
				}
		})

ZUGFeRD
-------

.. method:: PDFDocument.prototype.zugferdProfile()

	|only_mutool|

	Determine if the current PDF is a ZUGFeRD PDF, and, if so,
	return the profile type in use. Possible return values include:
	"NOT ZUGFERD", "COMFORT", "BASIC", "EXTENDED", "BASIC WL",
	"MINIMUM", "XRECHNUNG", and "UNKNOWN".

	:returns: string

	.. code-block::

		var profile = pdfDocument.zugferdProfile()

.. method:: PDFDocument.prototype.zugferdVersion()

	|only_mutool|

	Determine if the current PDF is a ZUGFeRD PDF, and, if so,
	return the version of the spec it claims to conform to.
	Returns 0 for non-zugferd PDFs.

	:returns: number

	.. code-block::

		var version = pdfDocument.zugferdVersion()

.. method:: PDFDocument.prototype.zugferdXML()

	|only_mutool|

	Return a buffer containing the embedded ZUGFeRD XML data from
	this ZUGFeRD PDF.

	:returns: `Buffer` | null

	.. code-block::

		var buf = pdfDocument.zugferdXML()
