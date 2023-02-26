.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_annotation:


.. _mutool_run_js_api_pdf_annotation:


`PDFAnnotation`
----------------------

:title:`PDF` Annotations belong to a specific `PDFPage` and may be created/changed/removed. Because annotation appearances may change (for several reasons) it is possible to scan through the annotations on a page and query them whether a re-render is necessary. Finally redaction annotations can be applied to a `PDFPage`, destructively removing content from the page.


.. method:: bound()

    Returns a rectangle containing the location and dimension of the annotation.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: run(device, transform)

    Calls the device functions to draw the annotation.

    :arg device: `Device`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.


.. method:: toPixmap(transform, colorspace, alpha)

    Render the annotation into a `Pixmap`, using the transform and colorspace.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.


.. method:: toDisplayList()

    Record the contents of the annotation into a `DisplayList`.

    :return: `DisplayList`.


.. method:: getObject()

    Get the underlying `PDFObject` for an annotation.

    :return: `PDFObject`.


.. method:: process(processor)

    Run through the annotation appearance stream and call methods on the supplied :ref:`PDF processor<mutool_run_js_api_pdf_processor>`.

    :arg processor: User defined function.


.. method:: setAppearance(appearance, state, transform, displayList)

    Set the annotation appearance stream for the given appearance. The desired appearance is given as a transform along with a display list.

    :arg appearance: `String` Appearance stream ("N", "R" or "D").
    :arg state: `String` "On" or "Off".
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg displayList: `DisplayList`.

.. method:: setAppearance(appearance, state, transform, bbox, resources, contents)

    Set the annotation appearance stream for the given appearance. The desired appearance is given as a transform along with a bounding box, a :title:`PDF` dictionary of resources and a content stream.

    :arg appearance: `String` Appearance stream ("N", "R" or "D").
    :arg state: `String` "On" or "Off".
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg bbox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg resources: Resources object.
    :arg contents: Contents string.


**Appearance stream values**


.. list-table::
   :header-rows: 1

   * - Value
     - Description
   * - N
     - normal appearance
   * - R
     - roll-over appearance
   * - D
     - down (pressed) appearance


.. method:: update()

    Update the appearance stream to account for changes in the annotation.


.. method:: getHot()

    Get the annotation as being hot, *i.e.* that the pointer is hovering over the annotation.

    :return: `Boolean`.

.. method:: setHot(hot)

    Set the annotation as being hot, *i.e.* that the pointer is hovering over the annotation.

    :arg hot: `Boolean`.


----

These properties are available for all annotation types.

.. method:: getType()

    Return the annotation type.

    :return: `String` :ref:`Annotation type<mutool_run_js_api_annotation_types>`.

.. method:: getFlags()

    Get the annotation flags.

    :return: `Integer` which determines the bit-field value.

.. method:: setFlags(flags)

    Set the annotation flags.

    :arg flags: `Integer` which determines the bit-field value.


**Annotation flags**


.. list-table::
   :header-rows: 1

   * - **Bit position**
     - **Name**
   * - `1`
     - Invisible
   * - `2`
     - Hidden
   * - `3`
     - Print
   * - `4`
     - NoZoom
   * - `5`
     - NoRotate
   * - `6`
     - NoView
   * - `7`
     - ReadOnly
   * - `8`
     - Locked
   * - `9`
     - ToggleNoView
   * - `10`
     - LockedContents





.. method:: getContents()

    Get the annotation contents.

    :return: `String`.

.. method:: setContents(text)

    Set the annotation contents.

    :arg text: `String`.


.. method:: getBorder()

    Get the annotation border line width in points.

    :return: `Float`.

.. method:: setBorder(width)

    Set the annotation border line width in points. Use `setBorderWidth()` to avoid removing the border effect.

    :arg width: `Float` Border width.


.. method:: getColor()

    Get the annotation color, represented as an array of up to 4 component values.

    :return: The :ref:`color value<mutool_run_js_api_colors>`.

.. method:: setColor(color)

    Set the annotation color, represented as an array of up to 4 component values.

    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.

.. method:: getOpacity()

    Get the annotation opacity.

    :return: The :ref:`opacity<mutool_run_js_api_alpha>` value.

.. method:: setOpacity(opacity)

    Set the annotation opacity.

    :arg opacity: The :ref:`opacity<mutool_run_js_api_alpha>` value.

.. method:: getCreationDate()

    Get the annotation creation date as a :title:`JavaScript` `Date` object.

    :return: `Date`.

.. method:: setCreationDate(milliseconds)

    Set the creation date to the number of milliseconds since the epoch.

    :arg milliseconds: `Integer` Milliseconds value.



.. method:: getModificationDate()

    Get the annotation modification date as a :title:`JavaScript` `Date` object.

    :return: `Date`.


.. method:: setModificationDate(milliseconds)

    Set the annotation modification date to the number of milliseconds since the epoch.

    :arg milliseconds: `Integer` Milliseconds value.


.. method:: getQuadding()

    Get the annotation quadding (justification).

    :return: Quadding value, `0` for left-justified, `1` for centered, `2` for right-justified.


.. method:: setQuadding(value)

    Set the annotation quadding (justification).

    :arg value: Quadding value, `0` for left-justified, `1` for centered, `2` for right-justified.


.. method:: getLanguage()

    Get the annotation language (or the get the inherited document language).

    :return: `String`.


.. method:: setLanguage(language)

    Set the annotation language.

    :arg language: `String`.



----


These properties are only present for some annotation types, so support for them must be checked before use.

.. method:: getRect()

    Get the annotation bounding box.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

.. method:: setRect(rect)

    Set the annotation bounding box.

    :arg rect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: getDefaultAppearance()

    Get the default text appearance used for free text annotations.

    :return: `{font:String, size:Integer, color:[r,g,b]}` Returns an object with the key/value pairs.


.. method:: setDefaultAppearance(font, size, color)

    Set the default text appearance used for free text annotations.

    :arg font: `String`.
    :arg size: `Integer`.
    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.


.. method:: hasInteriorColor()

    Checks whether the annotation has support for an interior color.

    :return: `Boolean`.

.. method:: getInteriorColor()

    Gets the annotation interior color.

    :return: The :ref:`color value<mutool_run_js_api_colors>`.

.. method:: setInteriorColor(color)

    Sets the annotation interior color.

    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.



.. method:: hasAuthor()

    Checks whether the annotation has an author.

    :return: `Boolean`.

.. method:: getAuthor()

    Gets the annotation author.

    :return: `String`.


.. method:: setAuthor(author)

    Sets the annotation author.

    :arg author: `String`.


.. method:: hasLineEndingStyles()

    Checks the support for line ending styles.

    :return: `Boolean`.


.. method:: getLineEndingStyles()

    Gets the line ending styles object.

    :return: `{start:String, end:String}` Returns an object with the key/value pairs.


.. method:: setLineEndingStyles(start, end)

    Sets the line ending styles object.

    :arg start: `String`.
    :arg end: `String`.


.. list-table::
   :header-rows: 1

   * - **Line ending names**
   * - "None"
   * - "Square"
   * - "Circle"
   * - "Diamond"
   * - "OpenArrow"
   * - "ClosedArrow"
   * - "Butt"
   * - "ROpenArrow"
   * - "RClosedArrow"
   * - "Slash"




.. method:: hasIcon()

    Checks the support for annotation icon.

    :return: `Boolean`.

.. method:: getIcon()

    Gets the annotation icon.

    :return: `String`.


.. method:: setIcon(name)

    Sets the annotation icon.

    :arg name: `String`.


.. list-table::
   :header-rows: 1

   * - **Icon type**
     - **Icon name**
   * - File attachment
     - "Graph"
   * -
     - "PaperClip"
   * -
     - "PushPin"
   * -
     - "Tag"
   * - Sound
     - "Mic"
   * -
     - "Speaker"
   * - Stamp
     - "Approved"
   * -
     - "AsIs"
   * -
     - "Confidential"
   * -
     - "Departmental"
   * -
     - "Draft"
   * -
     - "Experimental"
   * -
     - "Expired"
   * -
     - "Final"
   * -
     - "ForComment"
   * -
     - "ForPublicRelease"
   * -
     - "NotApproved"
   * -
     - "NotForPublicRelease"
   * -
     - "Sold"
   * -
     - "TopSecret"
   * - Text
     - "Comment"
   * -
     - "Help"
   * -
     - "Insert"
   * -
     - "Key"
   * -
     - "NewParagraph"
   * -
     - "Note"
   * -
     - "Paragraph"



.. method:: hasLine()


    Checks the support for annotation line.

    :return: `Boolean`.


.. method:: getLine()

    Get line end points, represented by an array of two points, each represented as an `[x, y]` array.

    :return: `[[x,y],...]`.


.. method:: setLine(endpoints)


    Set line end points, represented by an array of two points, each represented as an `[x, y]` array.

    :arg endpoints: `[[x,y],...]`.


.. method:: hasOpen()

    Checks the support for annotation open state.

    :return: `Boolean`.


.. method:: isOpen()

    Get annotation open state.

    :return: `Boolean`.

.. method:: setIsOpen(state)

    Set annotation open state.

    :arg state: `Boolean`.


.. note::

    "Open" refers to whether the annotation has an open state or is opened - e.g. A note icon is considered "Open" if the user has clicked on it to view its contents.


.. method:: hasFilespec()

    Checks support for the annotation file specification.

    :return: `Boolean`.

.. method:: getFilespec()

    Gets the file specification object.

    :return: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.



.. _mutool_run_js_api_pdf_annotation_setFilespec:


.. method:: setFilespec(fileSpecObject)

    Sets the file specification object.

    :arg fileSpecObject: `Object` :ref:`File Specification object<mutool_run_js_api_file_spec_object>`.


----

The border drawn around some annotations can be controlled by:

.. method:: hasBorder()

    Check support for the annotation border style.

    :return: `Boolean`.

.. method:: getBorderStyle()

    Get the annotation border style, either of "Solid" or "Dashed".

    :return: `String`.


.. method:: setBorderStyle(style)

    Set the annotation border style, either of "Solid" or "Dashed".

    :arg: `String`.


.. method:: getBorderWidth()

    Get the border width in points.

    :return: `Float`.

.. method:: setBorderWidth(width)

    Set the border width in points. Retain any existing border effects.

    :arg width: `Float`


.. method:: getBorderDashCount()

    Returns the number of items in the border dash pattern.

    :return: `Integer`.

.. method:: getBorderDashItem(i)

    Returns the length of dash pattern item i.

    :arg i: `Integer` Item index.
    :return: `Integer`.


.. method:: setBorderDashPattern(dashPattern)

    Set the annotation border dash pattern to the given array of dash item lengths. The supplied array represents the respective line stroke and gap lengths, e.g. `[1,1]` sets a small dash and small gap, `[2,1,4,1]` would set a medium dash, a small gap, a longer dash and then another small gap.

    :arg dashpattern: [Integer, Integer, ....]

.. method:: clearBorderDash()

    Clear the entire border dash pattern for an annotation.

.. method:: addBorderDashItem(length)

    Append an item (of the given length) to the end of the border dash pattern.

    :arg length: `Integer`.


Annotations that have a border effect allows the effect to be controlled by:

.. method:: hasBorderEffect()

    Check support for annotation border effect.

    :return: `Boolean`.

.. method:: getBorderEffect()

    Get the annotation border effect, either of "None" or "Cloudy".

    :return: `String`.

.. method:: setBorderEffect(effect)

    Set the annotation border effect, either of "None" or "Cloudy".

    :arg: `String`.

.. method:: getBorderEffectIntensity()

    Get the annotation border effect intensity.

    :return: `Integer`.

.. method:: setBorderEffectIntensity(intensity)

    Set the annotation border effect intensity. Recommended values are between `0` and `2` inclusive.

    :arg: `Integer`.

----

Ink annotations consist of a number of strokes, each consisting of a sequence of vertices between which a smooth line will be drawn. These can be controlled by:

.. method:: hasInkList()

    Check support for the annotation ink list.

    :return: `Boolean`.

.. method:: getInkList()

    Get the annotation ink list, represented as an array of strokes, each an array of points each an array of its X/Y coordinates.

    :return: `[]`.

.. method:: setInkList(inkList)

    Set the annotation ink list, represented as an array of strokes, each an array of points each an array of its X/Y coordinates.

    :arg: `[]`.

.. method:: clearInkList()

    Clear the list of ink strokes for the annotation.

.. method:: addInkList(stroke)

    To the list of strokes, append a stroke, represented as an array of vertices each an array of its X/Y coordinates.

    :arg stroke: `[]`.

.. method:: addInkListStroke()

    Add a new empty stroke to the ink annotation.

.. method:: addInkListStrokeVertex(vertex)

    Append a vertex to end of the last stroke in the ink annotation. The vertex is an array of its X/Y coordinates.

    :arg vertex: `[]`.


Text markup and redaction annotations consist of a set of quadadrilaterals controlled by:

.. method:: hasQuadPoints()

    Check support for the annotation quadpoints.

    :return: `Boolean`.

.. method:: getQuadPoints()

    Get the annotation quadpoints, describing the areas affected by text markup annotations and link annotations.

    :return: `[]`.

.. method:: setQuadPoints(quadPoints)

    Set the annotation quadpoints, describing the areas affected by text markup annotations and link annotations.

    :arg quadPoints: `[]`.

.. method:: clearQuadPoints()

    Clear the list of quad points for the annotation.

.. method:: addQuadPoint(quadpoint)

    Append a single quad point as an array of 8 elements, where each pair are the X/Y coordinates of a corner of the quad.

    :arg quadpoint: `[]`.


Polygon and polyline annotations consist of a sequence of vertices with a straight line between them. Those can be controlled by:

.. method:: hasVertices()

    Check support for the annotation vertices.

    :return: `Boolean`.

.. method:: getVertices()

    Get the annotation vertices, represented as an array of vertices each an array of its X/Y coordinates.

    :return: `[]`.

.. method:: setVertices(vertices)

    Set the annotation vertices, represented as an array of vertices each an array of its X/Y coordinates.

    :arg vertices: `[]`.

.. method:: clearVertices()

    Clear the list of vertices for the annotation.


.. method:: addVertex(vertex)

    Append a single vertex as an array of its X/Y coordinates.

    :arg vertex: `[]`.


Stamp annotations have the option to set a custom image as its appearance.

.. method:: setStampImage(image)

    Set a custom image appearance for a stamp annotation.

    :arg image: `Image`.
