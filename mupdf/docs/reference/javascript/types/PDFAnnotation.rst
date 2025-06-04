.. default-domain:: js

.. highlight:: javascript

PDFAnnotation
#############

PDF Annotations belong to a specific `PDFPage` and may be
created/changed/removed. Because annotation appearances may change (for several
reasons) it is possible to scan through the annotations on a page and query
them to see whether a re-render is necessary.

Additionally redaction annotations can be applied to a `PDFPage`,
destructively removing content from the page.

Annotation Types
================

These are the annotation types, and which attributes they have.

Text
	An icon with a popup of text.

	Set the appearance with the Icon attribute.

	Attributes: `Rect`_, :ref:`color-attribute`, :ref:`icon-attribute`.

FreeText
	Text in a rectangle on the page.

	The text font and color is defined by DefaultAppearance.

	Attributes: :ref:`border-attribute`, :ref:`rect-attribute`, :ref:`default-appearance-attribute`.

Line
	A line with optional arrow heads.

	The line width is determined by the border attribute.

	The end points are defined by the Line attribute.

	Attributes: :ref:`border-attribute`, :ref:`color-attribute`, :ref:`line-attribute`, :ref:`line-ending-styles-attribute`.

Square
	A rectangle.

	Attributes: :ref:`rect-attribute`, :ref:`rect-attribute`, :ref:`color-attribute`, :ref:`interior-color-attribute`.

Circle
	An ellipse.

	Attributes: :ref:`rect-attribute`, :ref:`border-attribute`, :ref:`color-attribute`, :ref:`interior-color-attribute`.

Polygon, PolyLine
	A polygon shape (closed and open).

	The shape is defined by the Vertices attribute.

	The line width is defined by the Border attribute.

	Attributes: :ref:`vertices-attribute`, :ref:`border-attribute`, :ref:`color-attribute`, :ref:`interior-color-attribute`, LineEndingStyles.

Highlight, Underline, Squiggly, StrikeOut
	Text markups.

	The shape is defined by the :ref:`quadpoints-attribute`.

Stamp
	A rubber stamp.

	The appearance is either a stock name, or a :ref:`custom image <stamp-image-attribute>`.

Ink
	A free-hand line.

	The shape is defined by the :ref:`inklist-attribute` attribute.

FileAttachment
	A file attachment.

	The appearance is an icon on the page.

	Set the attached file contents with the :ref:`filespec-attribute` attribute,
	and the appearance with the :ref:`icon-attribute` attribute.

Redaction
	A black box.

	Redaction annotations are used to mark areas of the page that
	can be redacted. They do NOT redact any content by themselves,
	you MUST apply them using `PDFAnnotation.prototype.applyRedaction` or
	`PDFPage.prototype.applyRedactions`.

These annotation types are special and handled with other APIs:

- `Link`
- Popup -- see `PDFAnnotation.prototype.setPopup()`
- Widget -- see `PDFWidget`

Constructors
============

.. class:: PDFAnnotation

	|no_new|

To get the annotations on a page use `PDFPage.prototype.getAnnotations()`.

To create a new annotation call `PDFPage.prototype.createAnnotation()`.

Instance methods
================

.. method:: PDFAnnotation.prototype.getBounds()

	Returns a rectangle containing the location and dimension of the annotation.

	:returns: `Rect`

	.. code-block::

		var bounds = annotation.getBounds()

.. method:: PDFAnnotation.prototype.run(device, matrix)

	Calls the device functions to draw the annotation.

	:param Device device: The device to make device calls to while rendering the annotation.
	:param Matrix matrix: The transformation matrix.

	.. code-block::

		annotation.run(device, mupdf.Matrix.identity)

.. method:: PDFAnnotation.prototype.toPixmap(matrix, colorspace, alpha)

	Render the annotation into a `Pixmap`, using the
	``transform``, ``colorspace`` and ``alpha`` parameters.

	:param Matrix matrix: Transformation matrix.
	:param ColorSpace colorspace: The desired colorspace of the returned pixmap.
	:param boolean alpha: Whether the returned pixmap has transparency or not. If the pixmap handles transparency, it starts out transparent (otherwise it is filled white), before the contents of the display list are rendered onto the pixmap.

	:returns: `Pixmap`

	.. code-block::

		var pixmap = annotation.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true)

.. method:: PDFAnnotation.prototype.toDisplayList()

	Record the contents of the annotation into a `DisplayList`.

	:returns: `DisplayList`

	.. code-block::

		var displayList = annotation.toDisplayList()

.. method:: PDFAnnotation.prototype.getObject()

	Get the underlying `PDFObject` for an annotation.

	:returns: `PDFObject`

	.. code-block::

		var obj = annotation.getObject()

.. method:: PDFAnnotation.prototype.setAppearance(appearance, state, transform, bbox, resources, contents)

	Set the annotation appearance stream for the given appearance. The
	desired appearance is given as a transform along with a bounding box, a
	PDF dictionary of resources and a content stream.

	:param string | null appearance: Appearance stream ("N" for normal, "R" for roll-over or "D" for down). Defaults to "N".
	:param string | null state: The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
	:param Matrix transform: The transformation matrix.
	:param Rect bbox: The bounding box.,
	:param PDFObject resources: Resources object.
	:param Buffer | ArrayBuffer | Uint8Array | string contents: Contents string.

	.. code-block::

		annotation.setAppearance(
			"N",
			null,
			mupdf.Matrix.identity,
			[0, 0, 100, 100],
			resources,
			contents
		)

.. method:: PDFAnnotation.prototype.update()

	Update the appearance stream to account for changes in the annotation.

	Returns true if the annotation appearance changed during the call.

	:returns: boolean

	.. code-block::

		annotation.update()

.. method:: PDFAnnotation.prototype.setAppearanceFromDisplayList(appearance, state, transform, list)

	Set the annotation appearance stream for the given appearance. The
	desired appearance is given as a transform along with a display list.

	:param string appearance: Appearance stream ("N", "R" or "D").
	:param string state: The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
	:param Matrix transform: The transformation matrix.
	:param DisplayList list: The display list.

	.. code-block::

		annotation.setAppearanceFromDisplayList(
			"N",
			null,
			mupdf.Matrix.identity,
			displayList
		)

.. method:: PDFAnnotation.prototype.getHiddenForEditing()

	Get a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

	:returns: boolean

	.. code-block::

		var hidden = annotation.getHiddenForEditing()

.. method:: PDFAnnotation.prototype.setHiddenForEditing(hidden)

	Set a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

	:param boolean hidden:

	.. code-block::

		annotation.setHiddenForEditing(true)

.. method:: PDFAnnotation.prototype.getHot()

	|only_mutool|

	Check if the annotation is hot, i.e. that the pointing device's cursor
	is hovering over the annotation.

	:returns: boolean

	.. code-block::

		annotation.getHot()

.. method:: PDFAnnotation.prototype.setHot(hot)

	|only_mutool|

	Set the annotation as being hot, i.e. that the pointing device's cursor
	is hovering over the annotation.

	:param boolean hot:

	.. code-block::

		annotation.setHot(true)

.. method:: PDFAnnotation.prototype.requestSynthesis()

	|only_mutool|

	Request that if an annotation does not have an appearance stream, flag
	the annotation to have one generated. The appearance stream
	will be created during future calls to
	`PDFAnnotation.prototype.update()` on or
	`PDFPage.prototype.update()`.

	.. code-block::

		annotation.requestSynthesis()

.. method:: PDFAnnotation.prototype.requestResynthesis()

	|only_mutool|

	Request that an appearance stream shall be re-generated for an
	annotation next time update() is called on
	`PDFAnnotation.prototype.update()` or
	`PDFPage.prototype.update()`.

	This is a side-effect of setting annotation attributes through
	the PDFAnnotation interface, so normally this call does not
	need to be done explicitly.

	.. code-block::

		annotation.requestResynthesis()

.. method:: PDFAnnotation.prototype.process(processor)

	|only_mutool|

	Run through the annotation appearance stream and call methods
	on the supplied `PDFProcessor`.

	:param PDFProcessor processor: User defined function.

	.. code-block::

		annotation.process(processor)

.. method:: PDFAnnotation.prototype.applyRedaction(blackBoxes, imageMethod, lineArtMethod, textMethod)

	Applies a single Redaction annotation.

	See `PDFPage.prototype.applyRedactions` for details.

Annotation attributes
=====================

PDF Annotations have many attributes. Some of these are common to all
annotations, and some only exist on specific annotation types.

Common
-------------

.. method:: PDFAnnotation.prototype.getType()

	Return the :term:`annotation type` for this annotation.

	:returns: string

	.. code-block::

		var type = annotation.getType()

.. method:: PDFAnnotation.prototype.getFlags()

	Get the annotation flags.

	See `PDFAnnotation.prototype.setFlags`.

	:returns: number

	.. code-block::

		var flags = annotation.getFlags()

.. method:: PDFAnnotation.prototype.setFlags(flags)

	Set the annotation flags.

	:param number flags: A bit mask with the flags (see below).

	.. table::
		:align: left

		=======	====================
		Bit	Name
		=======	====================
		1	Invisible
		2	Hidden
		3	Print
		4	NoZoom
		5	NoRotate
		6	NoView
		7	ReadOnly
		8	Locked
		9	ToggleNoView
		10	LockedContents
		=======	====================

	.. code-block::

		annotation.setFlags(4) // Clears all other flags and sets "NoZoom".

.. method:: PDFAnnotation.prototype.getContents()

	Get the annotation contents.

	:returns: string

	.. code-block::

		var contents = annotation.getContents()

.. method:: PDFAnnotation.prototype.setContents(text)

	Set the annotation contents.

	:param string text:

	.. code-block::

		annotation.setContents("Hello World")

.. method:: PDFAnnotation.prototype.getCreationDate()

	Get the annotation creation date as a Date object.

	:returns: Date

	.. code-block::

		var date = annotation.getCreationDate()

.. method:: PDFAnnotation.prototype.setCreationDate(date)

	Set the creation date.

	:param Date date: A Date object.

	.. code-block::

		annotation.setCreationDate(new Date())

.. method:: PDFAnnotation.prototype.getModificationDate()

	Get the annotation modification date as a Date object.

	:returns: Date

	.. code-block::

		var date = annotation.getModificationDate()

.. method:: PDFAnnotation.prototype.setModificationDate(date)

	Set the modification date.

	:param Date date:

	.. code-block::

		annotation.setModificationDate(new Date())

.. method:: PDFAnnotation.prototype.getLanguage()

	Get the annotation :term:`language code` (or get the one
	inherited from the document).

	:returns: string | null

	.. code-block::

		var language = annotation.getLanguage()

.. method:: PDFAnnotation.prototype.setLanguage(language)

	Set the annotation :term:`language code`.

	:param string language: The desired language code.

	.. code-block::

		annotation.setLanguage("en")

.. _rect-attribute:

Rect
----

For annotations that can be resized by setting its bounding box rectangle
(e.g. Square and FreeText), `PDFAnnotation.prototype.hasRect()` returns ``true``.

Other annotation types, (e.g. Line, Polygon, and InkList)
change size by adding/removing vertices.
Yet other annotations (e.g. Highlight and StrikeOut)
change size by adding/removing QuadPoints.

The underlying Rect attribute on the PDF object is automatically updated as needed
for these other annotation types.

.. method:: PDFAnnotation.prototype.hasRect()

	Checks whether the annotation can be resized by setting its
	bounding box.

	:returns: boolean

	.. code-block::

		var hasRect = annotation.hasRect()

.. method:: PDFAnnotation.prototype.getRect()

	Get the annotation bounding box.

	:returns: `Rect`

	.. code-block::

		var rect = annotation.getRect()

.. method:: PDFAnnotation.prototype.setRect(rect)

	Set the annotation bounding box.

	:param Rect rect: The new desired bounding box.

	.. code-block::

		annotation.setRect([0, 0, 100, 100])

.. _rich-contents-attribute:

Rich contents
-------------

.. method:: PDFAnnotation.prototype.hasRichContents()

	Returns whether the annotation is capable of supporting rich text
	contents.

	:returns: boolean

	.. code-block::

		var hasRichContents = annotation.hasRichContents()

.. method:: PDFAnnotation.prototype.getRichContents()

	Obtain the annotation's rich-text contents, as opposed to the plain
	text contents obtained by `getContents()`.

	:returns: string

	.. code-block::

		var richContents = annotation.getRichContents()

.. method:: PDFAnnotation.prototype.setRichContents(plainText, richText)

	Set the annotation's rich-text contents, as opposed to the plain
	text contents set by `setContents()`.

	:param string plainText:
	:param string richText:

	.. code-block::

		annotation.setRichContents("plain text", "<b><i>Rich-Text</i></b>")

.. method:: PDFAnnotation.prototype.getRichDefaults()

	Get the default style used for the annotation's rich-text contents.

	:returns: string

	.. code-block::

		var richDefaults = annotation.getRichDefaults()

.. method:: PDFAnnotation.prototype.setRichDefaults(style)

	Set the default style used for the annotation's rich-text contents.

	:param string style:

	.. code-block::

		annotation.setRichDefaults("font-size: 16pt")

.. _color-attribute:

Color
-----

The meaning of the color attribute depends on the annotation type. For some it is the color
of the border.

.. method:: PDFAnnotation.prototype.getColor()

	Get the annotation color, represented as an array of 0, 1, 3, or 4 component values.

	:returns: `Color`

	.. code-block::

		var color = annotation.getColor()

.. method:: PDFAnnotation.prototype.setColor(color)

	Set the annotation color, represented as an array of 0, 1, 3, or 4 component values.

	:param Color color: The new color.

	:throws: TypeError if number of components is not 0, 1, 3, or 4.

	.. code-block::

		annotation.setColor([0, 1, 0])

.. _opacity-attribute:

Opacity
-------

.. method:: PDFAnnotation.prototype.getOpacity()

	Get the annotation :term:`opacity`.

	:returns: number

	.. code-block::

		var opacity = annotation.getOpacity()

.. method:: PDFAnnotation.prototype.setOpacity(opacity)

	Set the annotation :term:`opacity`.

	:param number opacity: The desired opacity.

	.. code-block::

		annotation.setOpacity(0.5)

.. _quadding-attribute:

Quadding
--------

.. method:: PDFAnnotation.prototype.hasQuadding()

	|only_mutool|

	Returns whether the annotation is capable of supporting
	quadding (justification).

	:returns: boolean

	.. code-block::

		var hasQuadding = annotation.hasQuadding()

.. method:: PDFAnnotation.prototype.getQuadding()

	Get the annotation quadding (justification). Quadding value, 0
	for left-justified, 1 for centered, 2 for right-justified

	:returns: number

	.. code-block::

		var quadding = annotation.getQuadding()

.. method:: PDFAnnotation.prototype.setQuadding(value)

	Set the annotation quadding (justification). Quadding value, 0
	for left-justified, 1 for centered, 2 for right-justified.

	:param number value: The desired quadding.

	.. code-block::

		annotation.setQuadding(1)

.. _author-attribute:

Author
------

.. method:: PDFAnnotation.prototype.hasAuthor()

	Returns whether the annotation is capable of supporting an author.

	:returns: boolean

	.. code-block::

		var hasAuthor = annotation.hasAuthor()

.. method:: PDFAnnotation.prototype.getAuthor()

	Gets the annotation author.

	:returns: string

	.. code-block::

		var author = annotation.getAuthor()

.. method:: PDFAnnotation.prototype.setAuthor(author)

	Sets the annotation author.

	:param string author:

	.. code-block::

		annotation.setAuthor("Jane Doe")

.. _border-attribute:

Border
------

.. method:: PDFAnnotation.prototype.hasBorder()

	Returns whether the annotation is capable of supporting a border.

	:returns: boolean

	.. code-block::

		var hasBorder = annotation.hasBorder()

.. method:: PDFAnnotation.prototype.getBorderStyle()

	Get the annotation :term:`border style`.

	:returns: string

	.. code-block::

		var borderStyle = annotation.getBorderStyle()

.. method:: PDFAnnotation.prototype.setBorderStyle(style)

	Set the annotation :term:`border style`.

	:param string style: The annotation style.

	.. code-block::

		annotation.setBorderStyle("Dashed")

.. method:: PDFAnnotation.prototype.getBorderWidth()

	Get the border width in points.

	:returns: number

	.. code-block::

		var w = annotation.getBorderWidth()

.. method:: PDFAnnotation.prototype.setBorderWidth(width)

	Set the border width in points. Retains any existing border effects.

	:param number width:

	.. code-block::

		annotation.setBorderWidth(1.5)

.. method:: PDFAnnotation.prototype.getBorderDashCount()

	Returns the number of items in the border dash pattern.

	:returns: number

	.. code-block::

		var dashCount = annotation.getBorderDashCount()

.. method:: PDFAnnotation.prototype.getBorderDashItem(idx)

	Returns the length of dash pattern item idx.

	:param number idx:

	:returns: number

	.. code-block::

		var length = annotation.getBorderDashItem(0)

.. method:: PDFAnnotation.prototype.setBorderDashPattern(list)

	Set the annotation border dash pattern to the given array of dash item lengths. The supplied array represents the respective line stroke and gap lengths, e.g. [1, 1] sets a small dash and small gap, [2, 1, 4, 1] would set a medium dash, a small gap, a longer dash and then another small gap.

	:param Array of number dashPattern:

	.. code-block::

		annotation.setBorderDashPattern([2.0, 1.0, 4.0, 1.0])

.. method:: PDFAnnotation.prototype.clearBorderDash()

	Clear the entire border dash pattern for an annotation.

	.. code-block::

		annotation.clearBorderDash()

.. method:: PDFAnnotation.prototype.addBorderDashItem(length)

	Append an item (of the given length) to the end of the border dash pattern.

	:param number length:

	.. code-block::

		annotation.addBorderDashItem(10.0)

.. method:: PDFAnnotation.prototype.hasBorderEffect()

	Returns whether the annotation is capable of supporting a border
	effect.

	:returns: boolean

	.. code-block::

		var hasEffect = annotation.hasBorderEffect()

.. method:: PDFAnnotation.prototype.getBorderEffect()

	Get the :term:`border effect`.

	:returns: string

	.. code-block::

		var effect = annotation.getBorderEffect()

.. method:: PDFAnnotation.prototype.setBorderEffect(effect)

	Set the :term:`border effect`.

	:param string effect: The border effect.

	.. code-block::

		annotation.setBorderEffect("None")

.. method:: PDFAnnotation.prototype.getBorderEffectIntensity()

	Get the annotation border effect intensity.

	:returns: number

	.. code-block::

		var intensity = annotation.getBorderEffectIntensity()

.. method:: PDFAnnotation.prototype.setBorderEffectIntensity(intensity)

	Set the annotation border effect intensity. Recommended values are between 0 and 2 inclusive.

	:param number intensity: Border effect intensity.

	.. code-block::

		annotation.setBorderEffectIntensity(1.5)

.. _callout-attribute:

Callout
-------

Callouts are used with FreeText annotations and
allow for a graphical line to point to an area on a page.

.. image:: /images/callout-annot.png
		  :alt: Callout annotation
		  :width: 100%

.. method:: PDFAnnotation.prototype.hasCallout()

	Returns whether the annotation is capable of supporting a callout.

	:returns: boolean

.. method:: PDFAnnotation.prototype.setCalloutLine(line)

	Takes an array of 2 or 3 `points <Point>`. Supply an empty array to
	remove the callout line.

	:param Array of Point points:

.. method:: PDFAnnotation.prototype.getCalloutLine()

	Returns the array of points.

	:returns: Array of `Point` | null

.. method:: PDFAnnotation.prototype.setCalloutPoint(p)

	Takes a point where the callout should point to.

	:param Point p:

.. method:: PDFAnnotation.prototype.getCalloutPoint()

	Returns the callout point.

	:returns: `Point`

.. method:: PDFAnnotation.prototype.setCalloutStyle(style)

	Sets the :term:`line ending style` of the callout line.

	:param string style:

.. method:: PDFAnnotation.prototype.getCalloutStyle()

	Returns the callout style.

	:returns: string

.. _default-appearance-attribute:

Default Appearance
------------------

.. method:: PDFAnnotation.prototype.hasDefaultAppearance()

	|only_mutool|

	Returns whether the annotation is capable of supporting a default
	appearance.

	:returns: boolean

	.. code-block::

		var hasRect = annotation.hasDefaultAppearance()

.. method:: PDFAnnotation.prototype.getDefaultAppearance()

	Get the default text appearance used for free text annotations
	as an object containing the font, size, and color.

	:returns: ``{ font: string, size: number, color: Color }``

	.. code-block::

		var appearance = annotation.getDefaultAppearance()
		console.log("DA font:", appearance.font, appearance.size)
		console.log("DA color:", appearance.color)

.. method:: PDFAnnotation.prototype.setDefaultAppearance(font, size, color)

	Set the default text appearance used for free text annotations.

	:param string font: The desired default font: ``"Helv" | "TiRo" | "Cour"`` for Helvetica, Times Roman, and Courier respectively.
	:param number size: The desired default font size.
	:param Color color: The desired default font color.

	.. code-block::

		annotation.setDefaultAppearance("Helv", 16, [0, 0, 0])

.. _filespec-attribute:

Filespec
--------

.. method:: PDFAnnotation.prototype.hasFilespec()

	Returns whether the annotation is capable of supporting a
	:term:`file specification`.

	:returns: boolean

	.. code-block::

		var hasFilespec = annotation.hasFilespec()

.. method:: PDFAnnotation.prototype.getFilespec()

	Get the :term:`file specification` PDF object for the file attachment, or null if none set.

	:returns: `PDFObject` | null

	.. code-block::

		var fs = annotation.getFilespec()

.. method:: PDFAnnotation.prototype.setFilespec(fs)

	Set the :term:`file specification` PDF object for the file attachment.

	:param `PDFObject` fs:

	.. code-block::

		annotation.setFilespec(fs)

.. _icon-attribute:

Icon
----

.. method:: PDFAnnotation.prototype.hasIcon()

	Returns whether the annotation is capable of supporting an icon.

	:returns: boolean

	.. code-block::

		var hasIcon = annotation.hasIcon()

.. method:: PDFAnnotation.prototype.getIcon()

	Get the annotation :term:`icon name`, either a standard or custom name.

	:returns: string

	.. code-block::

		var icon = annotation.getIcon()

.. method:: PDFAnnotation.prototype.setIcon(name)

	Set the annotation :term:`icon name`.

	Note that standard icon names can be used to resynthesize the annotation appearance, but custom names cannot.

	:param string name: An :term:`icon name`.

	.. code-block::

		annotation.setIcon("Note")

.. _inklist-attribute:

Ink List
--------

Ink annotations consist of a number of strokes, each consisting of a sequence of vertices between which a smooth line will be drawn.

.. method:: PDFAnnotation.prototype.hasInkList()

	Returns whether the annotation is capable of supporting an ink list.

	:returns: boolean

	.. code-block::

		var hasInkList = annotation.hasInkList()

.. method:: PDFAnnotation.prototype.getInkList()

	Get the annotation ink list, represented as an array of strokes.
	Each stroke consists of an array of points.

	:returns: Array of Array of `Point`

	.. code-block::

		var inkList = annotation.getInkList()

.. method:: PDFAnnotation.prototype.setInkList(inkList)

	Set the annotation ink list, represented as an array of strokes.
	Each stroke consists of an array of points.

	:param Array of Array of Point inkList:

	.. code-block::

		// this draws a box with a cross in three strokes:
		annotation.setInkList([
			[
				[0, 0], [10, 0], [10, 10], [0, 10], [0, 0]
			],
			[
				[10, 0], [0, 10]
			],
			[
				[0, 0], [10, 10]
			]
		])

.. method:: PDFAnnotation.prototype.clearInkList()

	Clear the list of ink strokes for the annotation.

	.. code-block::

		annotation.clearInkList()

.. method:: PDFAnnotation.prototype.addInkListStroke()

	Add a new empty stroke to the ink annotation.

	.. code-block::

		annotation.addInkListStroke()

.. method:: PDFAnnotation.prototype.addInkListStrokeVertex(v)

	Append a vertex to end of the last stroke in the ink annotation.

	:param Point v:

	.. code-block::

		annotation.addInkListStrokeVertex([0, 0])

.. _interior-color-attribute:

Interior Color
--------------

.. method:: PDFAnnotation.prototype.hasInteriorColor()

	Returns whether the annotation is capable of supporting an interior
	color.

	:returns: boolean

	.. code-block::

		var hasInteriorColor = annotation.hasInteriorColor()

.. method:: PDFAnnotation.prototype.getInteriorColor()

	Get the annotation interior color, represented as an array of 0, 1, 3, or 4 component values.

	:returns: `Color`

	.. code-block::

		var interiorColor = annotation.getInteriorColor()

.. method:: PDFAnnotation.prototype.setInteriorColor(color)

	Sets the annotation interior color.

	:param Color color: The new desired interior color.

	:throws: TypeError if number of components is not 0, 1, 3, or 4.

	.. code-block::

		annotation.setInteriorColor([0, 1, 1])

.. _line-attribute:

Line
----

.. method:: PDFAnnotation.prototype.hasLine()

	Returns whether the annotation is capable of supporting a line.

	:returns: boolean

	.. code-block::

		var hasLine = annotation.hasLine()

.. method:: PDFAnnotation.prototype.getLine()

	Get line end points, represented by an array of two points, each represented as an [x, y] array.

	:returns: Array of `Point`

	.. code-block::

		var line = annotation.getLine()

.. method:: PDFAnnotation.prototype.setLine(a, b)

	Set the two line end points, each represented as an [x, y] array.

	:param Point a: The new point a.
	:param Point b: The new point b.

	.. code-block::

		annotation.setLine([100, 100], [150, 175])

.. _line-ending-styles-attribute:

Line Ending Styles
------------------

.. method:: PDFAnnotation.prototype.hasLineEndingStyles()

	Returns whether the annotation is capable of supporting
	:term:`line ending style`.

	:returns: boolean

	.. code-block::

		var hasLineEndingStyles = annotation.hasLineEndingStyles()

.. method:: PDFAnnotation.prototype.getLineEndingStyles()

	Get the start and end :term:`line ending style` values for each end of the line annotation.

	:returns: ``{ start: string, end: string }`` Returns an object with the key/value pairs

	.. code-block::

		var lineEndingStyles = annotation.getLineEndingStyles()

.. method:: PDFAnnotation.prototype.setLineEndingStyles(start, end)

	Sets the :term:`line ending style` values for each end of the line annotation.

	:param string start:
	:param string end:

	.. code-block::

		annotation.setLineEndingStyles("Square", "OpenArrow")

.. _line-leaders-attribute:

Line Leaders
------------

In a PDF line leaders refer to two lines at the ends of the line annotation,
oriented perpendicular to the line itself. These are common in technical
drawings when illustrating distances.

.. image:: /images/leader-lines.png
		  :alt: Leader lines explained
		  :width: 100%

.. method:: PDFAnnotation.prototype.setLineLeader(v)

	Sets the line leader length.

	:param number v:
		The length of leader lines that extend from each endpoint of
		the line perpendicular to the line itself. A positive value
		means that the leader lines appear in the direction that is
		clockwise when traversing the line from its starting point to
		its ending point a negative value indicates the opposite
		direction.

	Setting a value of 0 effectively removes the line leader.

.. method:: PDFAnnotation.prototype.getLineLeader()

	Gets the line leader length.

	:returns: number

.. method:: PDFAnnotation.prototype.setLineLeaderExtension(v)

	Sets the line leader extension.

	:param number v:
		A non-negative number representing the length of leader line
		extensions that extend from the line proper 180 degrees from
		the leader lines.

	Setting a value of 0 effectively removes the line leader extension.

.. method:: PDFAnnotation.prototype.getLineLeaderExtension()

	Gets the line leader extension.

	:returns: number

.. method:: PDFAnnotation.prototype.setLineLeaderOffset(v)

	Sets the line leader offset.

	:param number v:
		A non-negative number representing the length of the leader
		line offset, which is the amount of empty space between the
		endpoints of the annotation and the beginning of the leader
		lines.

	Setting a value of 0 effectively removes the line leader offset.

.. method:: PDFAnnotation.prototype.getLineLeaderOffset()

	Gets the line leader offset.

	:returns: number

.. method:: PDFAnnotation.prototype.setLineCaption(on)

	Sets whether line caption is enabled or not.

	When line captions are enabled then calling the
	`PDFAnnotation.prototype.setContents` on the line annotation will
	render the contents onto the line as the caption text.

	:param boolean on:

.. method:: PDFAnnotation.prototype.getLineCaption()

	Returns whether the line caption is enabled or not.

	:returns: boolean

.. method:: PDFAnnotation.prototype.setLineCaptionOffset(point)

	Sets the line caption offset.

	The x value of the offset point is the horizontal offset along the
	annotation line from its midpoint, with a positive value indicating
	offset to the right and a negative value indicating offset to the
	left. The y value of the offset point is the vertical offset
	perpendicular to the annotation line, with a positive value
	indicating a shift up and a negative value indicating a shift down.

	Setting a point of [0, 0] removes the caption offset.

	.. image:: /images/offset-caption.png
		  :alt: Offset caption explained
		  :width: 100%

	:param Point point: A point specifying the offset of the caption text from its normal position.

.. method:: PDFAnnotation.prototype.getLineCaptionOffset()

	Returns the line caption offset as a point, [x, y].

	:returns: `Point`

.. _open-attribute:

Open
----

Open refers to whether the annotation is display in an open state when the
page is loaded. A Text Note annotation is considered open if the user has
clicked on it to view its contents.

.. method:: PDFAnnotation.prototype.hasOpen()

	Returns whether the annotation is capable of supporting annotation
	open state.

	:returns: boolean

	.. code-block::

		var hasOpen = annotation.hasOpen()

.. method:: PDFAnnotation.prototype.getIsOpen()

	Get annotation open state.

	:returns: boolean

	.. code-block::

		var isOpen = annotation.getIsOpen()

.. method:: PDFAnnotation.prototype.setIsOpen(state)

	Set annotation open state.

	:param boolean state:

	.. code-block::

		annotation.setIsOpen(true)

.. _popup-attribute:

Popup
-----

.. method:: PDFAnnotation.prototype.hasPopup()

	|only_mutool|

	Returns whether the annotation is capable of supporting a popup.

	:returns: boolean

	.. code-block::

		var hasPopup = annotation.hasPopup()

.. method:: PDFAnnotation.prototype.getPopup()

	Get annotation popup rectangle.

	:returns: `Rect`

	.. code-block::

		var popupRect = annotation.getPopup()

.. method:: PDFAnnotation.prototype.setPopup(rect)

	Set annotation popup rectangle.

	:param Rect rect: The desired area where the popup should appear.

	.. code-block::

		annotation.setPopup([0, 0, 100, 100])

.. _quadpoints-attribute:

QuadPoints
----------

Text markup and redaction annotations consist of a set of
quadadrilaterals, or :term:`QuadPoints <QuadPoint>`.
These are used in e.g. Highlight
annotations to mark up several disjoint spans of text.

In Javascript QuadPoints are represented with `Quad` objects.

.. method:: PDFAnnotation.prototype.hasQuadPoints()

	Returns whether the annotation is capable of supporting quadpoints.

	:returns: boolean

	.. code-block::

		var hasQuadPoints = annotation.hasQuadPoints()

.. method:: PDFAnnotation.prototype.getQuadPoints()

	Get the annotation's quadpoints, describing the areas affected by
	text markup annotations and link annotations.

	:returns: Array of `Quad`

	.. code-block::

		var quadPoints = annotation.getQuadPoints()

.. method:: PDFAnnotation.prototype.setQuadPoints(quadList)

	Set the annotation quadpoints describing the areas affected by
	text markup annotations and link annotations.

	:param Array of Quad quadList: The quadpoints to set.

	.. code-block::

		// two quads, the first one wider than the second one
		annotation.setQuadPoints([
			[ 100, 100, 200, 100, 200, 150, 100, 150 ],
			[ 125, 150, 175, 150, 175, 200, 125, 200 ]
		])

.. method:: PDFAnnotation.prototype.clearQuadPoints()

	Clear the list of quadpoints for the annotation.

	.. code-block::

		annotation.clearQuadPoints()

.. method:: PDFAnnotation.prototype.addQuadPoint(quad)

	Append a single quadpoint to the annotation.

	:param Quad quad: The quadpoint to add.

	.. code-block::

		annotation.addQuadPoint([1, 2, 3, 4, 5, 6, 7, 8])

.. _vertices-attribute:

Vertices
--------

Polygon and polyline annotations consist of a sequence of vertices with a straight line between them. Those can be controlled by:

.. method:: PDFAnnotation.prototype.hasVertices()

	Returns whether the annotation is capable of supporting vertices.

	:returns: boolean

	.. code-block::

		var hasVertices = annotation.hasVertices()

.. method:: PDFAnnotation.prototype.getVertices()

	Get the annotation vertices, represented as an array of points.

	:returns: Array of `Point`

	.. code-block::

		var vertices = annotation.getVertices()

.. method:: PDFAnnotation.prototype.setVertices(vertices)

	Set the annotation vertices, represented as an array of points.

	:param Array of Point vertices:

	.. code-block::

		annotation.setVertices([
			[0, 0],
			[10, 10],
			[20, 20]
		])

.. method:: PDFAnnotation.prototype.clearVertices()

	Clear the list of vertices for the annotation.

	.. code-block::

		annotation.clearVertices()

.. method:: PDFAnnotation.prototype.addVertex(vertex)

	Append a single vertex point to the annotation.

	:param Point vertex:

	.. code-block::

		annotation.addVertex([0, 0])

.. _stamp-image-attribute:

Stamp image
-----------

.. method:: PDFAnnotation.prototype.getStampImageObject()

	|only_mutool|

	If the annotation is a stamp annotation and it consists of an
	image, return the `PDFObject` representing that image.

	:returns: `PDFObject` | null

	.. code-block::

		var pdfobj = annotation.getStampImageObject()

.. method:: PDFAnnotation.prototype.setStampImageObject(imgobj)

	|only_mutool|

	Create an appearance stream containing the image passed as
	argument and set that as the normal appearance of the
	annotation.

	:param PDFObject imgobj: PDFObject corresponding to the desired image.

	.. code-block::

		annotation.setStampImageObject(imgobj)

.. method:: PDFAnnotation.prototype.setStampImage(img)

	|only_mutool|

	Add the image passed as argument to the document as a PDF
	object, and pass a reference to that object to when setting the
	normal appearance of the stamp annotation.

	:param Image img: The image to become the stamp annotations appearance.

	.. code-block::

		annotation.setStampImage(img)

.. _intent-attribute:

Intent
------

.. method:: PDFAnnotation.prototype.hasIntent()

	|only_mutool|

	Returns whether the annotation is capable of supporting an intent.

	:returns: boolean

	.. code-block::

		var hasIntent = annotation.hasIntent()

.. method:: PDFAnnotation.prototype.getIntent()

	Get the annotation intent, one of the values below:

	* "FreeTextCallout"
	* "FreeTextTypeWriter"
	* "LineArrow"
	* "LineDimension"
	* "PolyLineDimension"
	* "PolygonCloud"
	* "PolygonDimension"
	* "StampImage"
	* "StampSnapshot"

	:returns: string

	.. code-block::

		var intent = annotation.getIntent()

.. method:: PDFAnnotation.prototype.setIntent(intent)

	Set the annotation intent.

	:param string intent: Intent value, see `getIntent()` for permissible values.

	.. code-block::

		annotation.setIntent("LineArrow")

Events
------

PDF annotations can have different appearances depending on whether
the pointing device's cursor is hovering over an annotation, or if the
pointing device's button is pressed.

PDF widgets, which is a type of annotation, may also have associated
Javascript functions that are executed when certain events occur.

Therefore it is important to tell an PDFAnnotation when the pointing
device's cursor enters/exits an annotation, when it's button is
clicked, or when an annotation gains/loses input focus.

.. method:: PDFAnnotation.prototype.eventEnter()

	|only_mutool|

	Trigger appearance changes and event handlers for
	when the pointing device's cursor enters an
	annotation's active area.

	.. code-block::

		annot.eventEnter()

.. method:: PDFAnnotation.prototype.eventExit()

	|only_mutool|

	Trigger appearance changes and event handlers for
	when the pointing device's cursor exits an
	annotation's active area.

	.. code-block::

		annot.eventExit()

.. method:: PDFAnnotation.prototype.eventDown()

	|only_mutool|

	Trigger appearance changes and event handlers for
	when the pointing device's button is depressed within
	an annotation's active area.

	.. code-block::

		widget.eventDown()

.. method:: PDFAnnotation.prototype.eventUp()

	|only_mutool|

	Trigger appearance changes and event handlers for
	when the pointing device's button is released within
	an annotation's active area.

	.. code-block::

		widget.eventUp()

.. method:: PDFAnnotation.prototype.eventFocus()

	|only_mutool|

	Trigger event handlers for when an annotation gains
	input focus.

	.. code-block::

		widget.eventFocus()

.. method:: PDFAnnotation.prototype.eventBlur()

	|only_mutool|

	Trigger event handlers for when an annotation loses
	input focus.

	.. code-block::

		widget.eventBlur()
