.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_pdf_document:



.. _mutool_run_js_api_pdf_document:



`PDFDocument`
--------------------

With :title:`MuPDF` it is also possible to create, edit and manipulate :title:`PDF` documents using low level access to the objects and streams contained in a :title:`PDF` file. A `PDFDocument` object is also a `Document` object. You can test a `Document` object to see if it is safe to use as a `PDFDocument` by calling `document.isPDF()`.


.. method:: new PDFDocument()



    *Constructor method*.

    Create a new empty :title:`PDF` document.

    :return: `PDFDocument`.

    |example_tag|

    .. code-block:: javascript

        var pdfDocument = new mupdf.PDFDocument();


.. method:: new PDFDocument(fileName)

    |mutool_tag|

    *Constructor method*.

    Load a :title:`PDF` document from file.

    :return: `PDFDocument`.

    |example_tag|

    .. code-block:: javascript

        var pdfDocument = new mupdf.PDFDocument("my-file.pdf");


|instance_methods|


.. method:: getVersion()

    Returns the :title:`PDF` document version as an integer multiplied by 10, so e.g. a PDF-1.4 document would return 14.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var version = pdfDocument.getVersion();



.. method:: setLanguage(lang)

    |wasm_tag|

    Sets the language for the document.

    :arg lang: `String`.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.setLanguage("en");



.. method:: getLanguage()

    |wasm_tag|

    Gets the language for the document.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var lang = pdfDocument.getLanguage();


.. method:: save(fileName, options)

    |mutool_tag|

    Write the `PDFDocument` to file. The options are a string of comma separated options (see the :ref:`mutool convert options<mutool_convert>`).

    :arg fileName: The name of the file to save to.
    :arg options: The options.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.save("my_fileName.pdf", "compress,compress-images,garbage=compact");



.. method:: saveToBuffer(options)

    |wasm_tag|

    Saves the document to a buffer. The options are a string of comma separated options (see the :ref:`mutool convert options<mutool_convert>`).

    :arg options: The options.
    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pdfDocument.saveToBuffer({"compress-images":true});



.. method:: canBeSavedIncrementally()



    Returns *true* if the document can be saved incrementally, e.g. repaired documents or applying redactions prevents incremental saves.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var canBeSavedIncrementally = pdfDocument.canBeSavedIncrementally();


.. method:: countVersions()

    Returns the number of versions of the document in a :title:`PDF` file, typically 1 + the number of updates.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var versionNum = pdfDocument.countVersions();


.. method:: countUnsavedVersions()

    Returns the number of unsaved updates to the document.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var unsavedVersionNum = pdfDocument.countUnsavedVersions();

.. method:: validateChangeHistory()

    Check the history of the document, return the last version that checks out OK. Returns `0` if the entire history is OK, `1` if the next to last version is OK, but the last version has issues, etc.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var changeHistory = pdfDocument.validateChangeHistory();

.. method:: hasUnsavedChanges()

    Returns *true* if the document has been saved since it was last opened or saved.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasUnsavedChanges = pdfDocument.hasUnsavedChanges();


.. method:: wasPureXFA()

    |mutool_tag|

    Returns *true* if the document was an :title:`XFA` form without :title:`AcroForm` fields.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var wasPureXFA = pdfDocument.wasPureXFA();

.. method:: wasRepaired()

    Returns *true* if the document was repaired when opened.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var wasRepaired = pdfDocument.wasRepaired();


.. method:: setPageLabels(index, style, prefix, start)

    Sets the page label numbering for the page and all pages following it, until the next page with an attached label.

    :arg index: `Integer`.
    :arg style: `String` Can be one of the following strings: `""` (none), `"D"` (decimal), `"R"` (roman numerals upper-case), `"r"` (roman numerals lower-case), `"A"` (alpha upper-case), or `"a"` (alpha lower-case).
    :arg prefix: `String`.
    :arg start: `Integer` The ordinal with which to start numbering.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.setPageLabels(0, "D", "Prefix", 1);


.. method:: deletePageLabels(index)

    Removes any associated page label from the page.

    :arg index: `Integer`.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.deletePageLabels(0);


.. method:: getTrailer()

    The trailer dictionary. This contains indirect references to the "Root" and "Info" dictionaries. See: :ref:`PDF object access<mutool_run_js_api_pdf_object_access>`.

    :return: `PDFObject` The trailer dictionary.

    |example_tag|

    .. code-block:: javascript

        var dict = pdfDocument.getTrailer();

.. method:: countObjects()

    Return the number of objects in the :title:`PDF`. Object number `0` is reserved, and may not be used for anything. See: :ref:`PDF object access<mutool_run_js_api_pdf_object_access>`.

    :return: `Integer` Object count.


    |example_tag|

    .. code-block:: javascript

        var num = pdfDocument.countObjects();


.. method:: createObject()

    Allocate a new numbered object in the :title:`PDF`, and return an indirect reference to it. The object itself is uninitialized.

    :return: The new object.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.createObject();


.. method:: deleteObject(obj)

    Delete the object referred to by an indirect reference or its object number.

    :arg obj: The object to delete.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.deleteObject(obj);

.. method:: formatURIWithPathAndDest(path, destination)

    Format a link :title:`URI` given a system independent path (see table 3.40 in the 1.7 specification) to a remote document and a destination object or a destination string suitable for :ref:`createLink()<mutool_run_js_api_page_create_link>`.

    :arg path: `String` An absolute or relative path to a remote document file.
    :arg destination: :ref:`Link destiation<mutool_run_js_api_link_dest>` or `String` referring to a destination using either a destination object or a destination name in the remote document.

.. method:: appendDestToURI(uri, destination)

    Append a fragment representing a document destination to a an existing :title:`URI` that points to a remote document. The resulting string is suitable for :ref:`createLink()<mutool_run_js_api_page_create_link>`.

    :arg uri: `String` An URI to a remote document file.
    :arg destination: :ref:`Link destiation<mutool_run_js_api_link_dest>` or `String` referring to a destination using either a destination object or a destination name in the remote document.

----


:title:`PDF` :title:`JavaScript` actions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. method:: enableJS()

    |mutool_tag|

    Enable interpretation of document :title:`JavaScript` actions.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.enableJS();

.. method:: disableJS()

    |mutool_tag|

    Disable interpretation of document :title:`JavaScript` actions.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.disableJS();

.. method:: isJSSupported()

    |mutool_tag|

    Returns *true* if interpretation of document :title:`JavaScript` actions is supported.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var jsIsSupported = pdfDocument.isJSSupported();

.. method:: setJSEventListener(listener)

    |mutool_tag|

    Calls the listener whenever a document :title:`JavaScript` action triggers an event.

    :arg listener: `{}` The :title:`JavaScript` listener function.


    .. note::

        At present this listener will only trigger when a document :title:`JavaScript` action triggers an alert.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.setJSEventListener({
                onAlert: function(message) {
                        print(message);
                }
        });

----

:title:`PDF` Journalling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. method:: enableJournal()

    Activate journalling for the document.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.enableJournal();

.. method:: getJournal()

    Returns a :ref:`PDF Journal Object<mutool_run_js_api_pdf_journal_object>`.

    :return: `Object` :ref:`PDF Journal Object<mutool_run_js_api_pdf_journal_object>`.

    |example_tag|

    .. code-block:: javascript

        var journal = pdfDocument.getJournal();

.. method:: beginOperation(op)

    Begin a journal operation.

    :arg length: `String` The name of the operation.


    |example_tag|

    .. code-block:: javascript

        pdfDocument.beginOperation("my_operation");

.. method:: beginImplicitOperation()

    Begin an implicit journal operation. Implicit operations are operations that happen due to other operations, e.g. updating an annotation.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.beginImplicitOperation();

.. method:: endOperation()

    End a previously started normal or implicit operation. After this it can be undone/redone using the methods below.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.beginImplicitOperation();

.. method:: abandonOperation()

    |mutool_tag|

    Abandon an operation. Reverts to the state before that operation began.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.abandonOperation();

.. method:: canUndo()

    Returns *true* if undo is possible in this state.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var canUndo = pdfDocument.canUndo();

.. method:: canRedo()

    Returns *true* if redo is possible in this state.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var canRedo = pdfDocument.canRedo();

.. method:: undo()

    Move backwards in the undo history. Changes to the document after this throws away all subsequent history.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.undo();

.. method:: redo()

    Move forwards in the undo history.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.redo();





----

.. _mutool_run_js_api_pdf_object_access:

:title:`PDF` Object Access
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A :title:`PDF` document contains objects, similar to those in :title:`JavaScript`: arrays, dictionaries, strings, booleans, and numbers. At the root of the :title:`PDF` document is the trailer object; which contains pointers to the meta data dictionary and the catalog object which contains the pages and other information.

Pointers in :title:`PDF` are also called indirect references, and are of the form "32 0 R" (where 32 is the object number, 0 is the generation, and R is magic syntax). All functions in :title:`MuPDF` dereference indirect references automatically.

:title:`PDF` has two types of strings: `/Names` and `(Strings)`. All dictionary keys are names.

Some dictionaries in :title:`PDF` also have attached binary data. These are called streams, and may be compressed.


.. note::

    `PDFObjects` are always bound to the document that created them. Do **NOT** mix and match objects from one document with another document!




----

.. method:: addObject(obj)

    Add `obj` to the :title:`PDF` as a numbered object, and return an indirect reference to it.

    :arg obj: Object to add.

    :return: `Object`.

    |example_tag|

    .. code-block:: javascript

        var ref = pdfDocument.addObject(obj);


.. method:: addStream(buffer, object)

    Create a stream object with the contents of `buffer`, add it to the :title:`PDF`, and return an indirect reference to it. If `object` is defined, it will be used as the stream object dictionary.

    :arg buffer: `Buffer` object.
    :arg object: The object to add the stream to.

    :return: `Object`.

    |example_tag|

    .. code-block:: javascript

        var stream = pdfDocument.addStream(buffer, object);



.. method:: addRawStream(buffer, object)

    Create a stream object with the contents of `buffer`, add it to the :title:`PDF`, and return an indirect reference to it. If `object` is defined, it will be used as the stream object dictionary. The `buffer` must contain already compressed data that matches "Filter" and "DecodeParms" set in the stream object dictionary.

    :arg buffer: `Buffer` object.
    :arg object: The object to add the stream to.

    :return: `Object`.

    |example_tag|

    .. code-block:: javascript

        var stream = pdfDocument.addRawStream(buffer, object);


.. method:: newNull()

    Create a new null object.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newNull();



.. method:: newBoolean(boolean)

    |mutool_tag|

    Create a new boolean object.

    :arg boolean: The boolean value.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newBoolean(true);


.. method:: newBool(boolean)

    |wasm_tag|

    Create a new boolean object.

    :arg boolean: The boolean value.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newBool(true);



.. method:: newInteger(number)

    Create a new integer object.

    :arg number: The number value.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newInteger(1);


.. method:: newReal(number)

    Create a new real number object.

    :arg number: The number value.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newReal(7.3);


.. method:: newString(string)

    Create a new string object.

    :arg string: `String`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newString("hello");


.. method:: newByteString(byteString)

    |mutool_tag|

    Create a new byte string object.

    :arg byteString: `String`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newByteString("hello");



.. method:: newName(string)

    Create a new name object.

    :arg string: The string value.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newName("hello");


.. method:: newIndirect(objectNumber, generation)

    Create a new indirect object.

    :arg objectNumber: `Integer`.
    :arg generation: `Integer`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newIndirect(100, 0);



.. method:: newArray(capacity)

    Create a new array object.

    :arg capacity: `Integer` Defaults to `8`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newArray();


.. method:: newDictionary(capacity)

    Create a new dictionary object.

    :arg capacity: `Integer` Defaults to `8`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.newDictionary();


----

:title:`PDF` Page Access
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All page objects are structured into a page tree, which defines the order the pages appear in.

.. method:: countPages()

    Number of pages in the document.

    :return: `Integer` Page number.

    |example_tag|

    .. code-block:: javascript

        var pageCount = pdfDocument.countPages();


.. method:: loadPage(number)

    Return the `PDFPage` for a page number.

    :arg number: `Integer` The page number, the first page is number zero.

    :return: `PDFPage`.

    |example_tag|

    .. code-block:: javascript

        var page = pdfDocument.loadPage(0);

.. method:: findPage(number)

    Return the `PDFObject` for a page number.

    :arg number: `Integer` The page number, the first page is number zero.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.findPage(0);


.. method:: findPageNumber(page)

    |mutool_tag|

    Given a `PDFPage` instance, find the page number in the document.

    :arg page: `PDFPage` instance.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var pageNumber = pdfDocument.findPageNumber(page);


.. method:: deletePage(number)

    Delete the numbered `PDFPage`.

    :arg number: The page number, the first page is number zero.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.deletePage(0);


.. _mutool_insertPage:

.. method:: insertPage(at, page)

    Insert the `PDFPage` object in the page tree at the location. If ``at`` is -1, at the end of the document.

    Pages consist of a content stream, and a resource dictionary containing all of the fonts and images used.

    :arg at: The index to insert at.
    :arg page: The `PDFPage` to insert.


    |example_tag|

    .. code-block:: javascript

        pdfDocument.insertPage(-1, page);



.. method:: addPage(mediabox, rotate, resources, contents)



    Create a new `PDFPage` object. Note: this function does NOT add it to the page tree, use :ref:`insertPage<mutool_insertPage>` to do that.

    :arg mediabox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg rotate: Rotation value.
    :arg resources: Resources object.
    :arg contents: Contents string. This represents the page content stream - see section 3.7.1 in the PDF 1.7 specification.


    :return: `PDFPage`.


    |example_tag|

    .. code-block:: javascript

        var helvetica = pdfDocument.newDictionary();
        helvetica.put("Type", pdfDocument.newName("Font"));
        helvetica.put("Subtype", pdfDocument.newName("Type1"));
        helvetica.put("Name", pdfDocument.newName("Helv"));
        helvetica.put("BaseFont", pdfDocument.newName("Helvetica"));
        helvetica.put("Encoding", pdfDocument.newName("WinAnsiEncoding"));
        var fonts = pdfDocument.newDictionary();
        fonts.put("Helv", helvetica);
        var resources = pdfDocument.addObject(pdfDocument.newDictionary());
        resources.put("Font", fonts);
        var blankPage = pdfDocument.addPage([0,0,300,350], 0, resources, "BT /Helv 12 Tf 100 100 Td (MuPDF!)Tj ET");


    |example_tag|

    |mutool_tag|

    .. literalinclude:: ../examples/pdf-create.js
       :caption: docs/examples/pdf-create.js
       :language: javascript


.. method:: addSimpleFont(font, encoding)



    Create a `PDFObject` from the `Font` object as a simple font.

    :arg font: `Font`.
    :arg encoding: The encoding to use. Encoding is either "Latin" (CP-1252), "Greek" (ISO-8859-7), or "Cyrillic" (KOI-8U). The default is "Latin".

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.addSimpleFont(new mupdf.Font("Times-Roman"), "Latin");


.. method:: addCJKFont(font, language, wmode, style)



    Create a `PDFObject` from the Font object as a UTF-16 encoded CID font for the given language ("zh-Hant", "zh-Hans", "ko", or "ja"), writing mode ("H" or "V"), and style ("serif" or "sans-serif").

    :arg font: `Font`.
    :arg language: `String`.
    :arg wmode: `0` for horizontal writing, and `1` for vertical writing.
    :arg style: `String`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.addCJKFont(new mupdf.Font("ja"), "ja", 0, "serif");


.. method:: addFont(font)



    Create a `PDFObject` from the `Font` object as an Identity-H encoded CID font.

    :arg font: `Font`.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.addFont(new mupdf.Font("Times-Roman"));


.. method:: addImage(image)



    Create a `PDFObject` from the `Image` object.

    :arg image: `Image`.

    :return: `PDFObject`.


    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.addImage(new mupdf.Image(pixmap));


.. method:: loadImage(obj)

    Load an `Image` from a `PDFObject` (typically an indirect reference to an image resource).

    :arg obj: `PDFObject`.

    :return: `Image`.

    |example_tag|

    .. code-block:: javascript

        var image = pdfDocument.loadImage(obj);


----


Copying objects across :title:`PDFs`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following functions can be used to copy objects from one :title:`PDF` document to another:


.. method:: newGraftMap()

    |mutool_tag|

    Create a graft map on the destination document, so that objects that have already been copied can be found again. Each graft map should only be used with one source document. Make sure to create a new graft map for each source document used.

    :return: `PDFGraftMap`.

    |example_tag|

    .. code-block:: javascript

        var graftMap = pdfDocument.newGraftMap();


.. method:: graftObject(object)

    |mutool_tag|

    Deep copy an object into the destination document. This function will not remember previously copied objects. If you are copying several objects from the same source document using multiple calls, you should use a graft map instead.

    :arg object: Object to graft.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.graftObject(obj);


.. method:: graftPage(dstDoc, dstPageNumber, srcDoc, srcPageNumber)

    |mutool_tag|

    Graft a page and its resources at the given page number from the source document to the requested page number in the document.

    :arg dstPageNumber: The page number where the source page will be inserted. Page numbers start at `0`, and `-1` means at the end of the document.
    :arg srcDoc: Source document.
    :arg srcPageNumber: Source page number.

    |example_tag|

    .. code-block:: javascript

        pdfDocument.graftObject(-1, srcdoc, 0);


----


.. _mutool_object_pdf_document_embedded_files:

Embedded files in :title:`PDFs`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



.. method:: addEmbeddedFile(filename, mimetype, contents, creationDate, modificationDate, addChecksum)

    Embedded a file into the document. If a checksum is added then the file contents can be verified later. An indirect reference to a :ref:`File Specification Object<mutool_run_js_api_file_spec_object>` is returned.


    :arg filename: `String`.
    :arg mimetype: `String` See: Mimetype_.
    :arg contents: `Buffer`.
    :arg creationDate: `Date`.
    :arg modificationDate: `Date`.
    :arg addChecksum: `Boolean`.

    :return: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.


    .. note::

        After embedding a file into a :title:`PDF`, it can be connected to an annotation using :ref:`PDFAnnotation.setFilespec()<mutool_run_js_api_pdf_annotation_setFilespec>`.


    |example_tag|

    .. code-block:: javascript

        var fileSpecObject = pdfDocument.addEmbeddedFile("my_file.jpg",
                                                         "image/jpeg",
                                                         buffer,
                                                         new Date(),
                                                         new Date(),
                                                         false);


    .. |tor_todo| MUTOOL - the `creationDate` & `modificationDate` are in milliseconds since 1970, not a JS Date object.

.. method:: getEmbeddedFiles()

    Returns the embedded files or null for the document.

    :return: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.


.. method:: getEmbeddedFileParams(fileSpecObject)

    Return an object describing the file referenced by the `fileSpecObject`.

    :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    :return: `Object` :ref:`Embedded File Object<mutool_run_js_api_pdf_document_embedded_file_object>`.

    |example_tag|

    .. code-block:: javascript

        var obj = pdfDocument.getEmbeddedFileParams(fileSpecObject);


.. method:: getEmbeddedFileContents(fileSpecObject)



    Returns a `Buffer` with the contents of the embedded file referenced by the `fileSpecObject`.

    :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    :return: :ref:`Buffer<mutool_object_buffer>`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pdfDocument.getEmbeddedFileContents(fileSpecObject);





.. method:: verifyEmbeddedFileChecksum(fileSpecObject)

    Verify the :title:`MD5` checksum of the embedded file contents.

    :arg fileSpecObject: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    :return: `Boolean`.


    |example_tag|

    .. code-block:: javascript

        var fileChecksumValid = pdfDocument.verifyEmbeddedFileChecksum(fileSpecObject);




.. External links

.. _Mimetype: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
