.. default-domain:: js

.. highlight:: javascript

PDFPage
+++++++

A PDFPage inherits `Page`, so its interface is also available for
PDFPage objects.

Constructors
------------

.. class:: PDFPage

	|no_new|

Instances of this class are returned by :js:meth:`~Document.prototype.loadPage` called on a `PDFDocument`.

Static properties
-----------------

|only_mupdfjs|

.. data:: PDFPage.REDACT_IMAGE_NONE

	for no image redactions

.. data:: PDFPage.REDACT_IMAGE_REMOVE

	to redact entire images

.. data:: PDFPage.REDACT_IMAGE_PIXELS

	for redacting just the covered pixels

.. data:: PDFPage.REDACT_IMAGE_UNLESS_INVISIBLE

	only redact visible images

.. data:: PDFPage.REDACT_LINE_ART_NONE

	for no line art redactions

.. data:: PDFPage.REDACT_LINE_ART_REMOVE_IF_COVERED

	redacts line art if covered

.. data:: PDFPage.REDACT_LINE_ART_REMOVE_IF_TOUCHED

	redacts line art if touched

.. data:: PDFPage.REDACT_TEXT_REMOVE

	to redact text

.. data:: PDFPage.REDACT_TEXT_NONE

	for no text redaction

Instance methods
----------------

.. method:: PDFPage.prototype.getObject()

	Get the underlying `PDFObject` for a `PDFPage`.

	:returns: `PDFObject`

	.. code-block::

		var obj = page.getObject()

.. method:: PDFPage.prototype.getAnnotations()

	Returns an array of all annotations on the page.

	:returns: Array of `PDFAnnotation`

	.. code-block::

		var annots = page.getAnnotations()

.. method:: PDFPage.prototype.getWidgets()

	Returns an array of all widgets on the page.

	:returns: Array of `PDFWidget`

	.. code-block::

		var widgets = page.getWidgets()

.. method:: PDFPage.prototype.setPageBox(box, rect)

	Sets the type of box required for the page.

	:param string box: The :term:`page box` type to change.
	:param Rect rect: The new desired dimensions.

	.. code-block::

		page.setPageBox("ArtBox", [10, 10, 585, 832])

.. method:: PDFPage.prototype.createAnnotation(type)

	Create a new blank annotation of a given annotation type.

	:param string type: The :term:`annotation type` to create.

	:returns: `PDFAnnotation`.

	.. code-block::

		var annot = page.createAnnotation("Text")

.. method:: PDFPage.prototype.deleteAnnotation(annot)

	Delete a `PDFAnnotation` from the page.

	:param PDFAnnotation annot:

	.. code-block::

		var annots = getAnnotations()
		page.delete(annots[0])

.. method:: PDFPage.prototype.update()

	Loop through all annotations of the page and update them.

	Returns true if re-rendering is needed because at least one
	annotation was changed (due to either events or Javascript
	actions or annotation editing).

	:returns: boolean

	.. code-block::

		// edit an annotation
		var updated = page.update()

.. method:: PDFPage.prototype.toPixmap(matrix, colorspace, alpha, showExtras, usage, box)

	Render the page into a `Pixmap` with the PDF "usage" and :term:`page box`.

	:param Matrix matrix: The transformation matrix.
	:param ColorSpace colorspace: The desired colorspace.
	:param boolean alpha: Whether or not the returned pixmap will use alpha.
	:param boolean renderExtra: Whether annotations and widgets should be rendered. Defaults to true.
	:param string usage: If the output is destined for viewing or printing. Defaults to "View".
	:param string box: Default is "CropBox".

	:returns: `Pixmap`

	.. code-block::

		var pixmap = page.toPixmap(
			mupdf.Matrix.identity,
			mupdf.ColorSpace.DeviceRGB,
			true,
			false,
			"View",
			"CropBox"
		)

.. method:: PDFPage.prototype.applyRedactions(blackBoxes, imageMethod, lineArtMethod, textMethod)

	Applies all the Redaction annotations on the page.

	Redactions are a special type of annotation used to permanently remove (or
	"redact") content from a PDF.

	:param boolean blackBoxes: Whether to use black boxes at each redaction or not.
	:param number imageMethod: Default is `PDFPage.REDACT_IMAGE_PIXELS`.
	:param number lineArtMethod: Default is `PDFPage.REDACT_LINE_ART_REMOVE_IF_COVERED`.
	:param number textMethod: Default is `PDFPage.REDACT_TEXT_REMOVE`.

	Once redactions are added to a page you can *apply* them, which is an
	irreversible action, thus it is a two step process as follows:

	.. code-block::

		var annot1 = page.createAnnotation("Redaction")
		annot1.setRect([0, 0, 500, 100])
		var annot2 = page.createAnnotation("Redaction")
		annot2.setRect([0, 600, 500, 700])
		page.applyRedactions(true, mupdf.PDFPage.REDACT_IMAGE_NONE)

.. method:: PDFPage.prototype.process(processor)

	|only_mutool|

	Run through the page contents stream and call methods on the
	supplied `PDFProcessor`.

	:param PDFProcessor processor: User defined function callback object.

	.. code-block:: javascript

		pdfPage.process(processor)

.. method:: PDFPage.prototype.getTransform()

	Return the transform from MuPDF page space (upper left page origin,
	y descending, 72 dpi) to PDF user space (arbitrary page origin, y
	ascending, UserUnit dpi).

	:returns: `Rect`

	.. code-block:: javascript

		var transform = pdfPage.getTransform()

.. method:: PDFPage.prototype.createSignature(name)

	|only_mutool|

	Create a new signature widget with the given name as field
	label.

	:param string name: The desired field label.

	:returns: `PDFWidget`

	.. code-block:: javascript

		var signatureWidget = pdfPage.createSignature("test")

.. method:: PDFPage.prototype.countAssociatedFiles()

	|only_mutool|

	Return the number of :term:`associated files <associated file>`
	associated with this page. Note that this is the number of
	files associated with this page, not necessarily the
	total number of files associated with elements throughout the
	entire document.

	:returns: number

	.. code-block:: javascript

		var count = pdfPage.countAssociatedFiles()

.. method:: PDFPage.prototype.associatedFile(n)

	|only_mutool|

	Return the :term:`file specification` object that represents the nth
	Associated File on this page.

	``n`` should be in the range ``0 <= n < countAssociatedFiles()``.

	Returns null if no associated file exists or index is out of range.

	:returns: `PDFObject` | null

	.. code-block:: javascript

		var obj = pdfPage.associatedFile(0)

.. method:: PDFPage.prototype.clip(rect)

	|only_mutool|

	Clip the page to the given rectangle.

	:param Rect rect: The new desired dimensions.

	.. code-block:: javascript

		pdfPage.clip([0, 0, 200, 300])
