.. _mutool_object_pdf_annotation:

.. _mutool_run_js_api_pdf_annotation:


`PDFAnnotation`
----------------------

:title:`PDF` Annotations belong to a specific `PDFPage` and may be created/changed/removed. Because annotation appearances may change (for several reasons) it is possible to scan through the annotations on a page and query them to see whether a re-render is necessary. Finally redaction annotations can be applied to a `PDFPage`, destructively removing content from the page.

To get the annotations on a page see: :ref:`PDFPage getAnnotations()<mutool_run_js_api_pdf_page>`, to create an annotation see: :ref:`PDFPage createAnnotation()<mutool_run_js_api_pdf_page_createAnnotation>`.


|instance_methods|

.. method:: getBounds()

    Returns a rectangle containing the location and dimension of the annotation.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


    |example_tag|

    .. code-block:: javascript

        var bounds = annotation.getBounds();


.. method:: run(device, transform)

    Calls the device functions to draw the annotation.

    :arg device: `Device`.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.

    |example_tag|

    .. code-block:: javascript

        annotation.run(device, mupdf.Matrix.identity);


.. method:: toPixmap(transform, colorspace, alpha)

    Render the annotation into a :ref:`Pixmap <mutool_object_pixmap>`, using the transform and colorspace.

    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg colorspace: `ColorSpace`.
    :arg alpha: `Boolean`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pixmap = annotation.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true);




.. method:: toDisplayList()

    Record the contents of the annotation into a `DisplayList`.

    :return: `DisplayList`.

    |example_tag|

    .. code-block:: javascript

        var displayList = annotation.toDisplayList();



.. method:: getObject()

    Get the underlying `PDFObject` for an annotation.

    :return: `PDFObject`.

    |example_tag|

    .. code-block:: javascript

        var obj = annotation.getObject();


.. method:: process(processor)

    |mutool_tag_wasm_soon|

    Run through the annotation appearance stream and call methods on the supplied :ref:`PDF processor<mutool_run_js_api_pdf_processor>`.

    :arg processor: User defined function.

    |example_tag|

    .. code-block:: javascript

        annotation.process(processor);

    .. TODO(tor): WASM process is not a function and no processor interface exists.


.. method:: setAppearance(appearance, state, transform, displayList)

    Set the annotation appearance stream for the given appearance. The desired appearance is given as a transform along with a display list.

    :arg appearance: `String` Appearance stream ("N", "R" or "D").
    :arg state: `String` The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg displayList: `DisplayList`.

    |example_tag|

    .. code-block:: javascript

        annotation.setAppearance("N", null, mupdf.Matrix.identity, displayList);

.. method:: setAppearance(appearance, state, transform, bbox, resources, contents)

    Set the annotation appearance stream for the given appearance. The desired appearance is given as a transform along with a bounding box, a :title:`PDF` dictionary of resources and a content stream.

    :arg appearance: `String` Appearance stream ("N", "R" or "D").
    :arg state: `String` The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
    :arg transform: `[a,b,c,d,e,f]`. The transform :ref:`matrix<mutool_run_js_api_matrix>`.
    :arg bbox: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg resources: Resources object.
    :arg contents: Contents string.

    |example_tag|

    .. code-block:: javascript

        annotation.setAppearance("N", null, mupdf.Matrix.identity, [0,0,100,100], resources, contents);

.. method:: setAppearance(image)

    |mutool_tag|

    Set a stamp annotation's appearance to that of an image.

    :arg image: `Image` containing the desired appearance.

    |example_tag|

    .. code-block:: javascript

        var img = new Image("photo.jpg");
        annotation.setAppearance(img);


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

    |example_tag|

    .. code-block:: javascript

        annotation.update();

.. method:: requestSynthesis()

    |mutool_tag|

    Request that an appearance stream should be generated for an annotation if none present.
    The appearance stream itself will be created in future calls to update() on
    :ref:`PDFAnnotation<mutool_run_js_api_pdf_annotation>` or :ref:`PDFPage<mutool_run_js_api_pdf_page>`.

    |example_tag|

    .. code-block:: javascript

        annotation.requestSynthesis();

.. method:: requestResynthesis()

    |mutool_tag|

    Request that an appearance stream should be re-generated for an annotation next time update() is
    called on :ref:`PDFAnnotation<mutool_run_js_api_pdf_annotation>` or :ref:`PDFPage<mutool_run_js_api_pdf_page>`.
    This is a side-effect of setting annotation properties, so normally this call does not need to be done explicitly.

    |example_tag|

    .. code-block:: javascript

        annotation.requestResynthesis();

.. method:: getHot()

    |mutool_tag|

    Get the annotation as being hot, *i.e.* that the pointer is hovering over the annotation.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        annotation.getHot();


.. method:: setHot(hot)

    |mutool_tag|

    Set the annotation as being hot, *i.e.* that the pointer is hovering over the annotation.

    :arg hot: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        annotation.setHot(true);



.. method:: getHiddenForEditing()

    Get a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hidden = annotation.getHiddenForEditing();

.. method:: setHiddenForEditing(hidden)

    Set a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

    :arg hidden: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        annotation.setHiddenForEditing(true);


.. method:: getType()

    Return the annotation type.

    :return: `String` :ref:`Annotation type<mutool_run_js_api_annotation_types>`.

    |example_tag|

    .. code-block:: javascript

        var type = annotation.getType();


.. method:: getFlags()

    Get the annotation flags.

    :return: `Integer` representation of a bit-field of flags specified below.

    |example_tag|

    .. code-block:: javascript

        var flags = annotation.getFlags();


.. method:: setFlags(flags)

    Set the annotation flags.

    :arg flags: `Integer` representation of a bit-field of flags specified below.

    |example_tag|

    .. code-block:: javascript

        annotation.setFlags(8); // Clears all other flags and sets NoZoom.



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




.. _mutool_object_pdf_annotation_get_contents:

.. method:: getContents()

    Get the annotation contents.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var contents = annotation.getContents();

.. _mutool_object_pdf_annotation_set_contents:

.. method:: setContents(text)

    Set the annotation contents.

    :arg text: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setContents("Hello World");


.. method:: getBorder()

    |mutool_tag|

    Get the annotation border line width in points.

    :return: `Float`.

    |example_tag|

    .. code-block:: javascript

        var border = annotation.getBorder();


.. method:: setBorder(width)

    |mutool_tag|

    Set the annotation border line width in points. Use `setBorderWidth()` to avoid removing the border effect.

    :arg width: `Float` Border width.

    |example_tag|

    .. code-block:: javascript

        annotation.setBorder(1.0);

.. method:: getColor()

    Get the annotation color, represented as an array of 1, 3, or 4 component values.

    :return: The :ref:`color value<mutool_run_js_api_colors>`.

    |example_tag|

    .. code-block:: javascript

        var color = annotation.getColor();



.. method:: setColor(color)

    Set the annotation color, represented as an array of 1, 3, or 4 component values.

    :arg color: The :ref:`color value<mutool_run_js_api_colors>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setColor([0,1,0]);


.. method:: getOpacity()

    Get the annotation opacity.

    :return: The :ref:`opacity<mutool_run_js_api_alpha>` value.

    |example_tag|

    .. code-block:: javascript

        var opacity = annotation.getOpacity();



.. method:: setOpacity(opacity)

    Set the annotation opacity.

    :arg opacity: The :ref:`opacity<mutool_run_js_api_alpha>` value.

    |example_tag|

    .. code-block:: javascript

        annotation.setOpacity(0.5);


.. method:: getCreationDate()

    Get the annotation creation date as a :title:`JavaScript` `Date` object.

    :return: `Date`.

    |example_tag|

    .. code-block:: javascript

        var date = annotation.getCreationDate();



.. method:: setCreationDate(date)

    Set the creation date.

    :arg date: `Date`.

    |example_tag|

    .. code-block:: javascript

        annotation.setCreationDate(new Date());

    .. TODO(tor): In mutool this is milliseconds we need to change it to Date


.. method:: getModificationDate()

    Get the annotation modification date as a :title:`JavaScript` `Date` object.

    :return: `Date`.

    |example_tag|

    .. code-block:: javascript

        var date = annotation.getModificationDate();


.. method:: setModificationDate(date)

    Set the modification date.

    :arg date: `Date`.

    |example_tag|

    .. code-block:: javascript

        annotation.setModificationDate(new Date());

    .. TODO(tor): In mutool this is milliseconds we need to change it to Date



.. method:: getLanguage()

    Get the annotation language (or get the inherited document language).

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var language = annotation.getLanguage();


.. method:: setLanguage(language)

    Set the annotation language.

    :arg language: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setLanguage("en");



----


These properties are only present for some annotation types, so support for them must be checked before use.

.. method:: hasRect()

    Returns whether the annotation is capable of supporting a bounding box.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasRect = annotation.hasRect();

.. method:: getRect()

    Get the annotation bounding box.

    :return: `Array`. `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        var rect = annotation.getRect();



.. method:: setRect(rect)

    Set the annotation bounding box.

    :arg rect: `Array`. `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setRect([0,0,100,100]);


.. method:: hasDefaultAppearance()

    |mutool_tag|

    Returns whether the annotation is capable of supporting a bounding box.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasRect = annotation.hasRect();



.. method:: getDefaultAppearance()

    Get the :ref:`default text appearance <mutool_run_js_api_object_default_appearance_text_object>` used for free text annotations.

    :return: `{font:String, size:Integer, color:[r,g,b]}` Returns :ref:`a default text appearance <mutool_run_js_api_object_default_appearance_text_object>` with the key/value pairs.

    |example_tag|

    .. code-block:: javascript

        var appearance = annotation.getDefaultAppearance();



.. method:: setDefaultAppearance(font, size, color)

    Set the default text appearance used for free text annotations.

    :arg font: `String` ("Helv" = Helvetica, "TiRo" = Times New Roman, "Cour" = Courier).
    :arg size: `Integer`.
    :arg color: `Array`. The :ref:`color value<mutool_run_js_api_colors>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setDefaultAppearance("Helv", 16, [0,0,0]);



.. method:: hasQuadding()

    |mutool_tag|

    Returns whether the annotation is capable of supporting quadding (justification).

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasQuadding = annotation.hasQuadding();


.. method:: getQuadding()

    Get the annotation quadding (justification).

    :return: Quadding value, `0` for left-justified, `1` for centered, `2` for right-justified.

    |example_tag|

    .. code-block:: javascript

        var quadding = annotation.getQuadding();


.. method:: setQuadding(value)

    Set the annotation quadding (justification).

    :arg value: `Number`. Quadding value, `0` for left-justified, `1` for centered, `2` for right-justified.

    |example_tag|

    .. code-block:: javascript

        annotation.setQuadding(1);



.. method:: hasInteriorColor()

    Returns whether the annotation has support for an interior color.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasInteriorColor = annotation.hasInteriorColor();




.. method:: getInteriorColor()



    Gets the annotation interior color.

    :return: The :ref:`color value<mutool_run_js_api_colors>`.

    |example_tag|

    .. code-block:: javascript

        var interiorColor = annotation.getInteriorColor();



.. method:: setInteriorColor(color)



    Sets the annotation interior color.

    :arg color: `Array`. The :ref:`color value<mutool_run_js_api_colors>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setInteriorColor([0,1,1]);






.. method:: hasAuthor()


    Returns whether the annotation has support for an author.


    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasAuthor = annotation.hasAuthor();



.. method:: getAuthor()

    Gets the annotation author.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var author = annotation.getAuthor();


.. method:: setAuthor(author)

    Sets the annotation author.

    :arg author: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setAuthor("Jane Doe");


.. method:: hasLineEndingStyles()


    Checks the support for :ref:`line ending styles<mutool_pdf_annotation_line_ending_styles>`.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasLineEndingStyles = annotation.hasLineEndingStyles();




.. method:: getLineEndingStyles()



    Gets the :ref:`line ending styles<mutool_pdf_annotation_line_ending_styles>` object.

    :return: `{start:String, end:String}` Returns an object with the key/value pairs.

    |example_tag|

    .. code-block:: javascript

        var lineEndingStyles = annotation.getLineEndingStyles();



.. method:: setLineEndingStyles(start, end)



    Sets the :ref:`line ending styles<mutool_pdf_annotation_line_ending_styles>` object.

    :arg start: `String`.
    :arg end: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setLineEndingStyles("Square", "OpenArrow");




.. _mutool_pdf_annotation_line_ending_styles:

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



.. method:: hasLine()

    Returns whether the annotation is capable of supporting a line or not.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasLine = annotation.hasLine();




.. method:: getLine()



    Get line end points, represented by an array of two points, each represented as an `[x, y]` array.

    :return: `[[x,y],...]`.

    |example_tag|

    .. code-block:: javascript

        var line = annotation.getLine();



.. method:: setLine(endpoints)

    Set the two line end points, represented by an array of two points, each represented as an `[x, y]` array.

    :arg endpoint1: `[x,y]`.
    :arg endpoint2: `[x,y]`.

    |example_tag|

    .. code-block:: javascript

        annotation.setLine([100,100], [150, 175]);



Line Leaders
~~~~~~~~~~~~~~~


In a PDF line annotation, "line leaders" refer to visual elements that can be added to the endpoints of a line annotation to enhance its appearance or meaning.

.. image:: images/leader-lines.webp
          :alt: Leader lines explained
          :width: 100%


.. method:: setLineLeader(ll)

    Sets the line leader length.

    :arg ll: `Number`. The length of leader lines that extend from each endpoint of the line perpendicular to the line itself. A positive value means that the leader lines appear in the direction that is clockwise when traversing the line from its starting point to its ending point a negative value indicates the opposite direction.

    .. note::

        Setting a value of `0` effectively removes the line leader.


.. method:: getLineLeader()

    Gets the line leader length.

    :return: `Number`


.. method:: setLineLeaderExtension(lle)

    Sets the line leader extension.

    :arg lle: `Number`. A non-negative number representing the length of leader line extensions that extend from the line proper 180 degrees from the leader lines.

    .. note::

        Setting a value of `0` effectively removes the line leader extension.

.. method:: getLineLeaderExtension()

    Gets the line leader extension.

    :return: `Number`


.. method:: setLineLeaderOffset(llo)

    Sets the line leader offset.

    :arg llo: `Number`. A non-negative number representing the length of the leader line offset, which is the amount of empty space between the endpoints of the annotation and the beginning of the leader lines.

    .. note::

        Setting a value of `0` effectively removes the line leader offset.

.. method:: getLineLeaderOffset()

    |mutool_tag|

    Gets the line leader offset.

    :return: `Number`


.. method:: setLineCaption(enable)

    Sets whether line caption is enabled or not.

    :arg enable: `Boolean`.


    .. note::

        When line captions are enabled then using the :meth:`setContents` method on the Line will graphically render the caption contents onto the line.


.. method:: getLineCaption()

    Returns whether the line caption is enabled or not.

    :return: `Boolean`.


.. method:: setLineCaptionOffset(point)

    Sets any line caption offset.


    :arg point: `Array`. A point, `[x, y]`, specifying the offset of the caption text from its normal position. The first value is the horizontal offset along the annotation line from its midpoint, with a positive value indicating offset to the right and a negative value indicating offset to the left. The second value is the vertical offset perpendicular to the annotation line, with a positive value indicating a shift up and a negative value indicating a shift down.


    .. image:: images/offset-caption.webp
          :alt: Offset caption explained
          :width: 100%

    .. note::

        Setting a point of `[0,0]` effectively removes the caption offset.



.. method:: getLineCaptionOffset()

    Returns the line caption offset as a point, `[x, y]`.

    :return: `Array`.


----


Callouts
~~~~~~~~~~~~

Callouts are used with :ref:`"FreeText" annotations <mutool_run_js_api_annotation_types>` and allow for a graphical line to point to an area on a page.

.. image:: images/callout-annot.webp
          :alt: Callout annotation
          :width: 100%


.. method:: hasCallout()

    Returns whether the annotation is capable of supporting a callout or not.


    :return: `Boolean`.


.. method:: setCalloutLine(points)

    Takes an array of 2 or 3 points.

    :arg points: [ [x1, y1], [x2, y2], [x3, y3]? ].


.. method:: getCalloutLine()

    Returns the array of points.

    :return: `[ [x1, y1], [x2, y2], [x3, y3]? ]`.


.. method:: setCalloutPoint(point)

    Takes a point where the callout should point to.

    :arg points: `[x,y]`.


.. method:: getCalloutPoint()

    Returns the callout point.

    :return: `[x,y]`.


.. method:: setCalloutStyle(style)

    Sets the style of the callout line.

    :arg style: `String`. A :ref:`line ending style <mutool_pdf_annotation_line_ending_styles>`.


.. method:: getCalloutStyle()

    Returns the callout style.

    :return: `String`.


----


.. method:: hasIcon()

    Returns whether the annotation is capable of supporting an icon or not.


    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasIcon = annotation.hasIcon();




.. method:: getIcon()

    Gets the annotation icon name, either one of the standard :ref:`icon names<mutool_pdf_annotation_icon_names>`, or something custom.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var icon = annotation.getIcon();


.. method:: setIcon(name)

    Sets the annotation icon name, either one of the standard :ref:`icon names<mutool_pdf_annotation_icon_names>`, or something custom. Note that standard icon names can be used to resynthesize the annotation appearance, but custom names cannot.

    :arg name: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setIcon("Note");

.. _mutool_pdf_annotation_icon_names:

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



.. method:: hasPopup()

    |mutool_tag|

    Returns whether the annotation is capable of supporting a popup or not.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasPopup = annotation.hasPopup();


.. method:: getPopup()

    Get annotation popup rectangle.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        var popupRect = annotation.getPopup();

.. method:: setPopup(rect)

    Set annotation popup rectangle.

    :arg rect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setPopup([0,0,100,100]);


.. method:: hasOpen()

    Returns whether the annotation is capable of supporting an open state or not.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasOpen = annotation.hasOpen();


.. method:: getIsOpen()

    Get annotation open state.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isOpen = annotation.getIsOpen();

.. method:: setIsOpen(state)


    Set annotation open state.

    :arg state: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        annotation.setIsOpen(true);

.. note::

    "Open" refers to whether the annotation is display in an open state when the page is loaded. A Text Note annotation is considered "Open" if the user has clicked on it to view its contents.


.. method:: hasFileSpec()

    |mutool_tag|

    Returns whether the annotation is capable of supporting the annotation file specification.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasFileSpec = annotation.hasFileSpec();





.. method:: getFileSpec()


    Gets the file specification object.

    :return: `Object` :ref:`File Specification Object<mutool_run_js_api_file_spec_object>`.

    |example_tag|

    .. code-block:: javascript

        var fileSpec = annotation.getFileSpec(true);




.. _mutool_run_js_api_pdf_annotation_setFileSpec:


.. method:: setFileSpec(fileSpecObject)


    Sets the file specification object.

    :arg fileSpecObject: `Object` :ref:`File Specification object<mutool_run_js_api_file_spec_object>`.


    |example_tag|

    .. code-block:: javascript

        annotation.setFileSpec({filename:"my_file.pdf",
                                mimetype:"application/pdf",
                                size:1000,
                                creationDate:date,
                                modificationDate:date});





----


The border drawn around some annotations can be controlled by:

.. method:: hasBorder()

    Returns whether the annotation is capable of supporting border style.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasBorder = annotation.hasBorder();




.. method:: getBorderStyle()



    Get the annotation border style, either of "Solid" or "Dashed".

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var borderStyle = annotation.getBorderStyle();



.. method:: setBorderStyle(style)



    Set the annotation border style, either of "Solid" or "Dashed".

    :arg: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setBorderStyle("Dashed");



.. method:: getBorderWidth()



    Get the border width in points.

    :return: `Float`.

    |example_tag|

    .. code-block:: javascript

        var w = annotation.getBorderWidth();



.. method:: setBorderWidth(width)



    Set the border width in points. Retain any existing border effects.

    :arg width: `Float`.

    |example_tag|

    .. code-block:: javascript

        annotation.setBorderWidth(1.5);




.. method:: getBorderDashCount()



    Returns the number of items in the border dash pattern.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var dashCount = annotation.getBorderDashCount();


.. method:: getBorderDashItem(i)



    Returns the length of dash pattern item `i`.

    :arg i: `Integer` Item index.
    :return: `Float`.

    |example_tag|

    .. code-block:: javascript

        var length = annotation.getBorderDashItem(0);



.. method:: setBorderDashPattern(dashPattern)



    Set the annotation border dash pattern to the given array of dash item lengths. The supplied array represents the respective line stroke and gap lengths, e.g. `[1,1]` sets a small dash and small gap, `[2,1,4,1]` would set a medium dash, a small gap, a longer dash and then another small gap.

    :arg dashPattern: [Float, Float, ....].

    |example_tag|

    .. code-block:: javascript

        annotation.setBorderDashPattern([2.0, 1.0, 4.0, 1.0]);


.. method:: clearBorderDash()



    Clear the entire border dash pattern for an annotation.

    |example_tag|

    .. code-block:: javascript

        annotation.clearBorderDash();



.. method:: addBorderDashItem(length)



    Append an item (of the given length) to the end of the border dash pattern.

    :arg length: `Float`.

    |example_tag|

    .. code-block:: javascript

        annotation.addBorderDashItem(10.0);



Annotations that have a border effect allows the effect to be controlled by:

.. method:: hasBorderEffect()

    Returns whether the annotation is capable of supporting border effect.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasEffect = annotation.hasBorderEffect();





.. method:: getBorderEffect()



    Get the annotation border effect, either of "None" or "Cloudy".

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var effect = annotation.getBorderEffect();



.. method:: setBorderEffect(effect)



    Set the annotation border effect, either of "None" or "Cloudy".

    :arg: `String`.

    |example_tag|

    .. code-block:: javascript

        annotation.setBorderEffect("None");



.. method:: getBorderEffectIntensity()



    Get the annotation border effect intensity.

    :return: `Float`.

    |example_tag|

    .. code-block:: javascript

        var intensity = annotation.getBorderEffectIntensity();




.. method:: setBorderEffectIntensity(intensity)



    Set the annotation border effect intensity. Recommended values are between `0` and `2` inclusive.

    :arg: `Float`.

    |example_tag|

    .. code-block:: javascript

        annotation.setBorderEffectIntensity(1.5);



----

Ink annotations consist of a number of strokes, each consisting of a sequence of vertices between which a smooth line will be drawn. These can be controlled by:

.. method:: hasInkList()

    Returns whether the annotation is capable of supporting ink list.


    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasInkList = annotation.hasInkList();


.. method:: getInkList()

    Get the annotation ink list, represented as an array of strokes, each an array of points each an array of its X/Y coordinates.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var inkList = annotation.getInkList();

.. method:: setInkList(inkList)



    Set the annotation ink list, represented as an array of strokes, each an array of points each an array of its X/Y coordinates.

    :arg: `[...]`.

    |example_tag|

    .. code-block:: javascript

        annotation.setInkList([
                                  [
                                      [0,0]
                                  ],
                                  [
                                      [10,10], [20,20], [30,30]
                                  ]
                              ]);



.. method:: clearInkList()



    Clear the list of ink strokes for the annotation.

    |example_tag|

    .. code-block:: javascript

        annotation.clearInkList();



.. method:: addInkList(stroke)

    |mutool_tag|

    To the list of strokes, append a stroke, represented as an array of vertices each an array of its X/Y coordinates.

    :arg stroke: `[]`.

    |example_tag|

    .. code-block:: javascript

        annotation.addInkList(
                                [
                                    [0,0]
                                ],
                                [
                                    [10,10], [20,20], [30,30]
                                ]
                             );



.. method:: addInkListStroke()



    Add a new empty stroke to the ink annotation.

    |example_tag|

    .. code-block:: javascript

        annotation.addInkListStroke();



.. method:: addInkListStrokeVertex(vertex)



    Append a vertex to end of the last stroke in the ink annotation. The vertex is an array of its X/Y coordinates.

    :arg vertex: `[...]`.

    |example_tag|

    .. code-block:: javascript

        annotation.addInkListStrokeVertex([0,0]);



Text markup and redaction annotations consist of a set of quadadrilaterals controlled by:

.. method:: hasQuadPoints()

    Returns whether the annotation is capable of supporting quadpoints.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasQuadPoints = annotation.hasQuadPoints();



.. method:: getQuadPoints()

    Get the annotation quadpoints, describing the areas affected by text markup annotations and link annotations.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var quadPoints = annotation.getQuadPoints();

.. method:: setQuadPoints(quadPoints)

    Set the annotation quadpoints, describing the areas affected by text markup annotations and link annotations.

    :arg quadPoints: `[...]`.

    |example_tag|

    .. code-block:: javascript

        annotation.setQuadPoints([
                                    [1,2,3,4,5,6,7,8],
                                    [1,2,3,4,5,6,7,8],
                                    [1,2,3,4,5,6,7,8]
                                ]);

.. method:: clearQuadPoints()

    Clear the list of quad points for the annotation.

    |example_tag|

    .. code-block:: javascript

        annotation.clearQuadPoints();


.. method:: addQuadPoint(quadpoint)

    Append a single quad point as an array of 8 elements, where each pair are the X/Y coordinates of a corner of the quad.

    :arg quadpoint: `[]`.

    |example_tag|

    .. code-block:: javascript

        annotation.addQuadPoint([1,2,3,4,5,6,7,8]);


Polygon and polyline annotations consist of a sequence of vertices with a straight line between them. Those can be controlled by:

.. method:: hasVertices()

    Returns whether the annotation is capable of supporting annotation vertices.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasVertices = annotation.hasVertices();




.. method:: getVertices()

    Get the annotation vertices, represented as an array of vertices each an array of its X/Y coordinates.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var vertices = annotation.getVertices();


.. method:: setVertices(vertices)

    Set the annotation vertices, represented as an array of vertices each an array of its X/Y coordinates.

    :arg vertices: `[...]`.

    |example_tag|

    .. code-block:: javascript

        annotation.setVertices([
                                [0,0],
                                [10,10],
                                [20,20]
                              ]);

.. method:: clearVertices()

    Clear the list of vertices for the annotation.

    |example_tag|

    .. code-block:: javascript

        annotation.clearVertices();


.. method:: addVertex(vertex)



    Append a single vertex as an array of its X/Y coordinates.

    :arg vertex: `[...]`.

    |example_tag|

    .. code-block:: javascript

        annotation.addVertex([0,0]);


.. method:: applyRedaction(blackBoxes, imageMethod)

    Applies redaction to the annotation.

    :arg blackBoxes: `Boolean` Whether to use black boxes at each redaction or not.
    :arg imageMethod: `Integer`. `0` for no redactions, `1` to redact entire images, `2` for redacting just the covered pixels.

    .. note::

        Redactions are secure as they remove the affected content completely.

    |example_tag|

    .. code-block:: javascript

        annotation.applyRedaction(true, 1);

.. method:: hasRichContents()

    |mutool_tag|

    Returns whether the annotation is capable of supporting rich text contents.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasRichContents = annotation.hasRichContents();

.. method:: getRichContents()

    |mutool_tag|

    Obtain the annotation's rich-text contents, as opposed to the plain text contents obtained by :ref:`getContents()<mutool_object_pdf_annotation_get_contents>`.

    |example_tag|

    .. code-block:: javascript

        var richContents = annotation.getRichContents();

.. method:: setRichContents(plainText, richText)

    |mutool_tag|

    Set the annotation's rich-text contents, as opposed to the plain text contents set by :ref:`setContents()<mutool_object_pdf_annotation_set_contents>`.

    |example_tag|

    .. code-block:: javascript

        annotation.setRichContents("plain text", "<b><i>Rich-Text</i></b>");

.. method:: getRichDefaults()

    |mutool_tag|

    Get the default style used for the annotation's rich-text contents.

    |example_tag|

    .. code-block:: javascript

        var richDefaults = annotation.getRichDefaults();

.. method:: setRichDefaults(style)

    |mutool_tag|

    Set the default style used for the annotation's rich-text contents.

    |example_tag|

    .. code-block:: javascript

        annotation.setRichDefaults("font-size: 16pt");


.. method:: getStampImageObject()

    |mutool_tag|

    If the annotation is a stamp annotation and it consists of an image, return the `PDFObject` representing that image.

    :return: A `PDFObject` corresponding to the stamp annotation image, otherwise null.

    |example_tag|

    .. code-block:: javascript

        var pdfobj = annotation.getStampImageObject();


.. method:: setStampImageObject(imgobj)

    |mutool_tag|

    Create an appearance stream containing the image passed as argument and set that as the normal appearance of the annotation.

    :arg imgobj: A `PDFObject` corresponding to the desired image.

    |example_tag|

    .. code-block:: javascript

        annotation.setStampImageObject(imgobj);


.. method:: setStampImage(img)

    |mutool_tag|

    Add the image passed as argument to the document as a PDF object, and pass a reference to that object to when setting the normal appearance of the stamp annotation.

    :arg img: The `Image` desired to become the stamp annotations appearance.

    |example_tag|

    .. code-block:: javascript

        annotation.setStampImage(img);


.. method:: hasIntent()

    |mutool_tag|

    Returns whether the annotation is capable of supporting intent (justification).

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var hasIntent = annotation.hasIntent();


.. method:: getIntent()

    Get the annotation intent.

    :return: `String` :ref:`Annotation intent<mutool_pdf_annotation_intent_types>`.

    |example_tag|

    .. code-block:: javascript

        var intent = annotation.getIntent();


.. method:: setIntent(intent)

    Set the annotation intent.

    :arg intent: `String` Intent value.

    |example_tag|

    .. code-block:: javascript

        annotation.setIntent("LineArrow");

.. _mutool_pdf_annotation_intent_types:

**Intent values**

.. list-table::
   :header-rows: 1

   * - **Intent names**
   * - "FreeTextCallout"
   * - "FreeTextTypeWriter"
   * - "LineArrow"
   * - "LineDimension"
   * - "PolyLineDimension"
   * - "PolygonCloud"
   * - "PolygonDimension"
   * - "StampImage"
   * - "stampSnapshot"

Events
~~~~~~~~~~~~~~~

PDF annotations my have event handlers reacting to different interactive events happening to the annotation.

.. method:: eventEnter()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device enters an annotation's active area.

    |example_tag|

    .. code-block:: javascript

        annot.eventEnter();

    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventExit()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device exits an annotation's active area.

    |example_tag|

    .. code-block:: javascript

        annot.eventExit();

    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventDown()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device's button is depressed within an annotation's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventDown();


    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventUp()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device's button is released within an annotation's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventUp();


    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventFocus()

    |mutool_tag_wasm_soon|

    Trigger the event when an annotation gains input focus.

    |example_tag|

    .. code-block:: javascript

        widget.eventFocus();


    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventBlur()

    |mutool_tag_wasm_soon|

    Trigger the event when an annotation loses input focus.

    |example_tag|

    .. code-block:: javascript

        widget.eventBlur();


    .. TODO(tor): WASM, Even says "TODO" in the mupdf.js source file :)
