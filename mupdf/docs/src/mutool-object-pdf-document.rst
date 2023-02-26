.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_document:



.. _mutool_run_js_api_pdf_document:



`PDFDocument`
--------------------

With :title:`MuPDF` it is also possible to create, edit and manipulate :title:`PDF` documents using low level access to the objects and streams contained in a :title:`PDF` file. A `PDFDocument` object is also a `Document` object. You can test a `Document` object to see if it is safe to use as a `PDFDocument` by calling `document.isPDF()`.


.. method:: new PDFDocument()

    *Constructor method*.

    Create a new empty :title:`PDF` document.

    :return: `PDFDocument`.

    **Example**

    .. code-block:: javascript

        var pdfDocument = new PDFDocument();


.. method:: new PDFDocument(fileName)

    *Constructor method*.

    Load a :title:`PDF` document from file.

    :return: `PDFDocument`.

    **Example**

    .. code-block:: javascript

        var pdfDocument = new PDFDocument("my-file.pdf");




.. method:: save(fileName, options)

    Write the `PDFDocument` to file. The write options are a string of comma separated options (see the :ref:`mutool convert options<mutool_convert>`).

    :arg fileName: The name of the file to save to.
    :options: The options as key-value pairs.


.. method:: canBeSavedIncrementally()

    Returns *true* if the document can be saved incrementally, e.g. repaired documents or applying redactions prevents incremental saves.

    :return: `Boolean`.

.. method:: countVersions()

    Returns the number of versions of the document in a :title:`PDF` file, typically 1 + the number of updates.

    :return: `Integer`.


.. method:: countUnsavedVersions()

    Returns the number of unsaved updates to the document.

    :return: `Integer`.

.. method:: validateChangeHistory()

    Check the history of the document, return the last version that checks out OK. Returns `0` if the entire history is OK, `1` if the next to last version is OK, but the last version has issues, etc.

    :return: `Integer`.

.. method:: hasUnsavedChanges()

    Returns *true* if the document has been saved since it was last opened or saved.

    :return: `Boolean`.

.. method:: wasPureXFA()

    Returns *true* if the document was an :title:`XFA` form without :title:`AcroForm` fields.

    :return: `Boolean`.

.. method:: wasRepaired()

    Returns *true* if the document was repaired when opened.

    :return: `Boolean`.


.. method:: getTrailer()

    The trailer dictionary. This contains indirect references to the "Root" and "Info" dictionaries. See: :ref:`PDF object access<mutool_run_js_api_pdf_object_access>`.

    :return: The trailer dictionary.

.. method:: countObjects()

    Return the number of objects in the :title:`PDF`. Object number `0` is reserved, and may not be used for anything. See: :ref:`PDF object access<mutool_run_js_api_pdf_object_access>`.

    :return: Object count.

.. method:: createObject()

    Allocate a new numbered object in the :title:`PDF`, and return an indirect reference to it. The object itself is uninitialized.

    :return: The new object.


.. method:: deleteObject(obj)

    Delete the object referred to by the indirect reference.

    :arg obj: The object to delete.

----


**PDF JavaScript actions**

.. method:: enableJS()

    Enable interpretation of document :title:`JavaScript` actions.

.. method:: disableJS()

    Disable interpretation of document :title:`JavaScript` actions.

.. method:: isJSSupported()

    Returns *true* if interpretation of document :title:`JavaScript` actions is supported.

    :return: `Boolean`.

.. method:: setJSEventListener(listener)

    Calls the listener whenever a document :title:`JavaScript` action triggers an event.

    :arg listener: `{}` The :title:`JavaScript` listener function.


    .. note::

        At present this listener will only trigger when a document :title:`JavaScript` action triggers an alert.

----

**PDF journalling**

.. method:: enableJournal()

    Activate journalling for the document.

.. method:: getJournal()

    Returns a :ref:`PDF Journal Object<mutool_run_js_api_pdf_journal_object>`.

    :return: `Object` :ref:`PDF Journal Object<mutool_run_js_api_pdf_journal_object>`.

.. method:: beginOperation()

    Begin a journal operation

.. method:: beginImplicitOperation()

    Begin an implicit journal operation. Implicit operations are operations that happen due to other operations, e.g. updating an annotation.

.. method:: endOperation()

    End a previously started normal or implicit operation. After this it can be undone/redone using the methods below.

.. method:: canUndo()

    Returns *true* if undo is possible in this state.

    :return: `Boolean`.

.. method:: canRedo()

    Returns *true* if redo is possible in this state.

    :return: `Boolean`.

.. method:: undo()

    Move backwards in the undo history. Changes to the document after this throws away all subsequent history.

.. method:: redo()

    Move forwards in the undo history.





----

.. _mutool_run_js_api_pdf_object_access:

**PDF Object Access**

A :title:`PDF` document contains objects, similar to those in :title:`JavaScript`: arrays, dictionaries, strings, booleans, and numbers. At the root of the :title:`PDF` document is the trailer object; which contains pointers to the meta data dictionary and the catalog object which contains the pages and other information.

Pointers in :title:`PDF` are also called indirect references, and are of the form "32 0 R" (where 32 is the object number, 0 is the generation, and R is magic syntax). All functions in :title:`MuPDF` dereference indirect references automatically.

:title:`PDF` has two types of strings: `/Names` and `(Strings)`. All dictionary keys are names.

Some dictionaries in :title:`PDF` also have attached binary data. These are called streams, and may be compressed.


.. note::

    `PDFObjects` are always bound to the document that created them. Do **NOT** mix and match objects from one document with another document!




----

.. method:: addObject(obj)

    Add 'obj' to the :title:`PDF` as a numbered object, and return an indirect reference to it.

    :arg obj: Object to add.


.. method:: addStream(buffer, object)

    Create a stream object with the contents of `buffer`, add it to the :title:`PDF`, and return an indirect reference to it. If `object` is defined, it will be used as the stream object dictionary.

    :arg buffer: `Buffer` object.
    :arg object: The object to stream to.




.. method:: addRawStream(buffer, object)

    Create a stream object with the contents of `buffer`, add it to the :title:`PDF`, and return an indirect reference to it. If `object` is defined, it will be used as the stream object dictionary. The `buffer` must contain already compressed data that matches the "Filter" and "DecodeParms".

    :arg buffer: `Buffer` object.
    :arg object: The object to stream to.




.. method:: newNull()

    Create a new null object.

    :return: `PDFObject`.

.. method:: newBoolean(boolean)

    Create a new boolean object.

    :arg boolean: The boolean value.

    :return: `PDFObject`.

.. method:: newInteger(number)

    Create a new integer object.

    :arg number: The number value.

    :return: `PDFObject`.

.. method:: newReal(number)

    Create a new real number object.

    :arg number: The number value.

    :return: `PDFObject`.


.. method:: newString(string)

    Create a new string object.

    :arg string: `String`.

    :return: `PDFObject`.


.. method:: newByteString(byteString)

    Create a new byte string object.

    :arg byteString: `String`.

    :return: `PDFObject`.


.. method:: newName(string)

    Create a new name object.

    :arg string: The string value.

    :return: `PDFObject`.

.. method:: newIndirect(objectNumber, generation)

    Create a new indirect object.

    :arg objectNumber: The string value.
    :arg generation: The string value.

    :return: `PDFObject`.

.. method:: newArray()

    Create a new array object.

    :return: `PDFObject`.

.. method:: newDictionary()

    Create a new dictionary object.

    :return: `PDFObject`.



----

**PDF Page Access**

All page objects are structured into a page tree, which defines the order the pages appear in.

.. method:: countPages()

    Number of pages in the document.

    :return: Page number.

.. method:: findPage(number)

    Return the `PDFPage` object for a page number.

    :arg number: The page number, the first page is number zero.

    :return: `PDFPage`.


.. method:: findPageNumber(page)

    Given a `PDFPage` object, find the page number in the document.

    :return: `Integer`.


.. method:: deletePage(number)

    Delete the numbered `PDFPage`.

    :arg number: The page number, the first page is number zero.


.. method:: insertPage(at, page)

    Insert the `PDFPage` object in the page tree at the location. If 'at' is -1, at the end of the document.

    Pages consist of a content stream, and a resource dictionary containing all of the fonts and images used.

    :arg at: The index to insert at.
    :arg page: The `PDFPage` to insert.


.. method:: addPage(mediabox, rotate, resources, contents)

    Create a new page object. Note: this function does NOT add it to the page tree.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg rotate: Rotation value.
    :arg resources: Resources object.
    :arg contents: Contents string. This represents the page content stream - see section 3.7.1 in the PDF 1.7 specification.


    :return: `PDFPage`.


    **Example**

    .. literalinclude:: ../examples/pdf-create.js
       :caption: docs/examples/pdf-create.js
       :language: javascript


.. method:: addSimpleFont(font, encoding)

    Create a PDF object from the `Font` object as a simple font.

    :arg font: `Font`.
    :arg encoding: The encoding to use. Encoding is either "Latin" (CP-1252), "Greek" (ISO-8859-7), or "Cyrillic" (KOI-8U). The default is "Latin".


.. method:: addCJKFont(font, language, wmode, style)

    Create a PDF object from the Font object as a UTF-16 encoded CID font for the given language ("zh-Hant", "zh-Hans", "ko", or "ja"), writing mode ("H" or "V"), and style ("serif" or "sans-serif").

    :arg font: `Font`.
    :arg language: `String`.
    :arg wmode: `0` for horizontal writing, and `1` for vertical writing.
    :arg style: `String`.

.. method:: addFont(font)

    Create a :title:`PDF` object from the `Font` object as an Identity-H encoded CID font.

    :arg font: `Font`.


.. method:: addImage(image)

    Create a :title:`PDF` object from the `Image` object.

    :arg image: `Image`.

.. method:: loadImage(obj)

    Load an `Image` from a :title:`PDF` object (typically an indirect reference to an image resource).

    :arg obj: `PDFObject`.



----


The following functions can be used to copy objects from one document to another:



.. method:: graftObject(object)

    Deep copy an object into the destination document. This function will not remember previously copied objects. If you are copying several objects from the same source document using multiple calls, you should use a graft map instead.

    :arg object: Object to graft.


.. method:: graftPage(dstDoc, dstPageNumber, srcDoc, srcPageNumber)

    Graft a page and its resources at the given page number from the source document to the requested page number in the destination document.

    :arg dstDoc: Destination document.
    :arg dstPageNumber: Destination page number.
    :arg srcDoc: Source document.
    :arg srcPageNumber: Source page number.

.. method:: newGraftMap()

    Create a graft map on the destination document, so that objects that have already been copied can be found again. Each graft map should only be used with one source document. Make sure to create a new graft map for each source document used.

    :return: `PDFGraftMap`.


----


.. _mutool_object_pdf_document_embedded_files:

**Embedded files in PDFs**




.. method:: addEmbeddedFile(filename, mimetype, contents, creationDate, modificationDate, addChecksum)

    Embedded a file into the document. If a checksum is added then the file contents can be verified later. An indirect reference to a :ref:`File Specification Object<mutool_run_js_api_file_spec_object>` is returned.


    :arg filename: `String`.
    :arg mimetype: `String` See: Mimetype_.
    :arg contents: Contents string. This represents the page content stream - see section 3.7.1 in the PDF 1.7 specification.
    :arg creationDate: `Integer` Milliseconds value.
    :arg modificationDate: `Integer` Milliseconds value.
    :arg addChecksum: `Boolean`.

    :return: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.


    .. note::

        After embedding a file into a :title:`PDF`, it can be connected to an annotation using :ref:`PDFAnnotation.setFilespec()<mutool_run_js_api_pdf_annotation_setFilespec>`.


.. method:: getEmbeddedFileParams(fileSpecObject)

    Return an object describing the file referenced by the `filespecObject`.

    :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    :return: `Object` :ref:`Embedded File Object<mutool_run_js_api_pdf_document_embedded_file_object>`.

.. method:: getEmbeddedFileContents(fileSpecObject)

    Returns a `Buffer` with the contents of the embedded file referenced by the `filespecObject`.

    :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    :return: :ref:`Buffer<mutool_object_buffer>`.

.. method:: verifyEmbeddedFileChecksum(fileSpecObject)

    Verify the :title:`MD5` checksum of the embedded file contents.

     :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

     :return: `Boolean`.






.. External links

.. _Mimetype: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
