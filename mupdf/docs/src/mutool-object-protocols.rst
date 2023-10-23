.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_protocols:



.. _mutool_run_js_api_object_protocols:



Object Protocols
---------------------------


The following objects are standard :title:`JavaScript` objects with assumed properties (i.e. they follow their outlined protocol). They are used throughout the :title:`mutool API` to support object types for various methods.



.. _mutool_run_js_api_link_dest:


Link Destination Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A link destination points to a location within a document and how a document viewer should show that destination.

It consists of a dictionary with keys for:

`chapter`
    The chapter within the document.

`page`
    The page within the document.

`type`
    Either "Fit", "FitB", "FitH", "FitBH", "FitV", "FitBV", "FitR" or "XYZ", controlling which of the keys below exist.

`x`
    The left coordinate, valid for "FitV", "FitBV", "FitR" and "XYZ".

`y`
    The top coordinate, valid for "FitH", "FitBH", "FitR" and "XYZ".

`width`
    The width of the zoomed in region, valid for "XYZ".

`height`
    The height of the zoomed in region, valid for "XYZ".

`zoom`
    The zoom factor, valid for "XYZ".



.. _mutool_run_js_api_file_spec_object:

File Specification Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This object is used to represent a file.

In order to retrieve information from this object see methods described within :ref:`Embedded files in PDFs<mutool_object_pdf_document_embedded_files>`.



.. _mutool_run_js_api_pdf_document_embedded_file_object:

Embedded File Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This `Object` contains metadata about an embedded file, it has properties for:

`filename`
    The name of the embedded file.

`mimetype`
    The :title:`MIME` type of the embedded file, or `undefined` if none exists.

`size`
    The size in bytes of the embedded file contents.

`creationDate`
    The creation date of the embedded file.

`modificationDate`
    The modification date of the embedded file.


.. _mutool_run_js_api_pdf_journal_object:

PDF Journal Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This `Object` contains a numbered array of operations and a reference into this list indicating the current position.

`position`
    The current position in the journal.

`steps`
    An array containing the name of each step in the journal.




.. _mutool_run_js_api_stroke_dictionary:
.. _mutool_run_js_api_stroke_object:

Stroking State Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The stroking state is a dictionary with keys for:

    - `startCap`, `dashCap`, `endCap`
        "Butt", "Round", "Square", or "Triangle".

    - `lineCap`
        Set `startCap`, `dashCap`, and `endCap` all at once.

    - `lineJoin`
        "Miter", "Round", "Bevel", or "MiterXPS".

    - `lineWidth`
        Thickness of the line.

    - `miterLimit`
        Maximum ratio of the miter length to line width, before beveling the join instead.

    - `dashPhase`
        Starting offset for dash pattern.

    - `dashes`
        Array of on/off dash lengths.


|example_tag|

    `{dashes:[5,10], lineWidth:3, lineCap:'Round'}`




.. _mutool_run_js_api_outline_iterator_object:

Outline Iterator Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This `Object` has properties for:

`title`
    The title of the item.

`uri`
    A :title:`URI` pointing to the destination. Likely to be a document internal link that can be resolved by :ref:`Document.resolveLink()<mutool_run_js_api_document_resolveLink>`, otherwise a link to a web page.

`open`
    *True* if the item should be opened when shown in a tree view.




.. _mutool_run_js_api_pdf_widget_text_layout_object:

.. note ``index`` in the parameters list here, otherwise it inserts a hyperlink to index.rst "Welcome"

Text Layout Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A description of layouted text value from a text widget with keys:

`matrix`
    Normal transform matrix for the layouted text.

`invMatrix`
    Inverted transform matrix for the layouted text.

`lines`
    An array of text lines belonging to the layouted text, a `lines` object contains:

    - `x` The X coordinate for the text line.
    - `y` The Y coordinate for the text line.
    - `fontSize` The text size used for the layouted text line.
    - ``index`` The index of the beginning of the line in the text string.
    - `rect` The bounding rectangle for the text line.
    - `chars` An array of characters in the text line.

        A `chars` object contains:

        - `x` The position of the character.
        - `advance` The advance of the character.
        - ``index`` The index of the character in the text string.
        - `rect` The bounding :ref:`Rectangle<mutool_run_js_api_rectangle>` for the character.



.. _mutool_object_pdf_widget_signature_configuration:

Signature Configuration Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A signature configuration object has properties with `Boolean` values as follows:

`showLabels`
    Whether to include both labels and values or just values on the right hand side.

`showDN`
    Whether to include the distinguished name on the right hand side.

`showTextName`
    Whether to include the name of the signatory on the right hand side.

`showDate`
    Whether to include the date of signing on the right hand side.

`showGraphicName`
    Whether to include the signatory name on the left hand side.

`showLogo`
    Whether to include the :title:`MuPDF` logo in the background.



.. _mutool_run_js_api_object_story_placement_result_object:

Placement Result Object
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

`filled`
    The rectangle of the actual area that was used.

`more`
    *True* if more content remains to be placed, otherwise *false* if all content fits in the `Story`.
