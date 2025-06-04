.. default-domain:: js

.. highlight:: javascript

PDFWidget
===================

:term:`Widgets <Widget Type>` are annotations that represent form
components such as buttons, text inputs and signature fields.

Because PDFWidget inherits `PDFAnnotation`, they also provide the
same interface as other annotation types.

Many widgets, e.g. text inputs or checkboxes, are the visual representation of
an associated form field. As the widget changes state, so does its
corresponding field value; for example when the text is edited in a text input
or a checkbox is checked. Note that widgets may be changed by Javascript event
handlers triggered by edits on other widgets.

The PDF specification has sections on `Widget annotations
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1951506>`_
and
`Interactive forms
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1951635>`_
with further details.

Constructors
------------

.. class:: PDFWidget

	|no_new|

To get the widgets on a page, see `PDFPage.prototype.getWidgets()`.

Instance methods
----------------

.. method:: PDFWidget.prototype.getFieldType()

	Return the widget type, one of the following:

	``"button" | "checkbox" | "combobox" | "listbox" | "radiobutton" | "signature" | "text"``

	:returns: string

	.. code-block::

		var type = widget.getFieldType()

.. method:: PDFWidget.prototype.getFieldFlags()

	Return the field flags which indicate attributes for the
	field. There are convenience functions to check some of these:
	:js:meth:`~PDFWidget.prototype.isReadOnly()`,
	:js:meth:`~PDFWidget.prototype.isMultiline()`,
	:js:meth:`~PDFWidget.prototype.isPassword()`,
	:js:meth:`~PDFWidget.prototype.isComb()`,
	:js:meth:`~PDFWidget.prototype.isButton()`,
	:js:meth:`~PDFWidget.prototype.isPushButton()`,
	:js:meth:`~PDFWidget.prototype.isCheckbox()`,
	:js:meth:`~PDFWidget.prototype.isRadioButton()`,
	:js:meth:`~PDFWidget.prototype.isText()`,
	:js:meth:`~PDFWidget.prototype.isChoice()`,
	:js:meth:`~PDFWidget.prototype.isListBox()`,
	and
	:js:meth:`~PDFWidget.prototype.isComboBox()`.

	For details refer to the PDF specification's sections
	on flags
	`common to all field types
	<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1697681>`_,
	`for button fields
	<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1697832>`_,
	`for text fields
	<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1967484>`_,
	and
	`for choice fields
	<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1873701>`_.

	:returns: number

	.. code-block::

		var flags = widget.getFieldFlags()

.. method:: PDFWidget.prototype.getName()

	Retrieve the name of the field.

	:returns: string

	.. code-block::

		var fieldName = widget.getName()

.. method:: PDFWidget.prototype.getMaxLen()

	Get maximum allowed length of the string value.

	:returns: number

	.. code-block::

		var length = widget.getMaxLen()

.. method:: PDFWidget.prototype.getOptions()

	Returns an array of strings which represent the value for each corresponding radio button or checkbox field.

	:returns: Array of string

	.. code-block::

		var options = widget.getOptions()

.. method:: PDFWidget.prototype.getLabel()

	Get the field name as a string.

	:returns: string

	.. code-block::

		var label = widget.getLabel()

Editing
-------

.. method:: PDFWidget.prototype.getValue()

	Get the widget value.

	:returns: string

	.. code-block::

		var value = widget.getValue()

.. method:: PDFWidget.prototype.setTextValue(value)

	Set the widget string value.

	:param string value: New text value.

	.. code-block::

		widget.setTextValue("Hello World!")

.. method:: PDFWidget.prototype.setChoiceValue(value)

	Sets the choice value against the widget.

	:param string value: New choice value.

	.. code-block::

		widget.setChoiceValue("Yes")

.. method:: PDFWidget.prototype.toggle()

	Toggle the state of the widget, returns true if the state changed.

	:returns: boolean

	.. code-block::

		var state = widget.toggle()

.. method:: PDFWidget.prototype.getEditingState()

	|only_mutool|

	Get whether the widget is in editing state.

	:returns: boolean

	.. code-block::

		var state = widget.getEditingState()

.. method:: PDFWidget.prototype.setEditingState(state)

	|only_mutool|

	Set whether the widget is in editing state.

	When in editing state any changes to the widget value will not
	cause any side-effects such as changing other widgets or
	running Javascript event handlers. This is intended for, e.g.
	when a text widget is interactively having characters typed
	into it. Once editing is finished the state should reverted
	back, before updating the widget value again.

	:param boolean state:

	.. code-block::

		widget.getEditingState(false)

.. TODO The text layout object needs to be described.

.. method:: PDFWidget.prototype.layoutTextWidget()

	|only_mutool|

	Layout the value of a text widget. Returns a text layout object.

	:returns: Object

	.. code-block::

		var layout = widget.layoutTextWidget()

Flags
-----

.. method:: PDFWidget.prototype.isReadOnly()

	If the value is read only and the widget cannot be interacted with.

	:returns: boolean

	.. code-block::

		var isReadOnly = widget.isReadOnly()

.. method:: PDFWidget.prototype.isMultiline()

	|only_mupdfjs|

	Return whether the widget is multi-line.

	:returns: boolean

.. method:: PDFWidget.prototype.isPassword()

	|only_mupdfjs|

	Return whether the widget is a password input.

	:returns: boolean

.. method:: PDFWidget.prototype.isComb()

	|only_mupdfjs|

	Return whether the widget is a text field laid out in "comb" style (forms where you write one character per square).

	:returns: boolean

.. method:: PDFWidget.prototype.isButton()

	|only_mupdfjs|

	Return whether the widget is of "button", "checkbox" or "radiobutton" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isPushButton()

	|only_mupdfjs|

	Return whether the widget is of "button" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isCheckbox()

	|only_mupdfjs|

	Return whether the widget is of "checkbox" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isRadioButton()

	Return whether the widget is of "radiobutton" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isText()

	|only_mupdfjs|

	Return whether the widget is of "text" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isChoice()

	|only_mupdfjs|

	Return whether the widget is of "combobox" or "listbox" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isListBox()

	|only_mupdfjs|

	Return whether the widget is of "listbox" type.

	:returns: boolean

.. method:: PDFWidget.prototype.isComboBox()

	|only_mupdfjs|

	Return whether the widget is of "combobox" type.

	:returns: boolean

Signatures
----------

.. method:: PDFWidget.prototype.isSigned()

	|only_mutool|

	Returns true if the signature is signed.

	:returns: boolean

	.. code-block::

		var isSigned = widget.isSigned()

.. method:: PDFWidget.prototype.validateSignature()

	|only_mutool|

	Returns number of updates ago when signature became invalid.
	Returns 0 is signature is still valid, 1 if it became
	invalid during the last save, etc.

	:returns: number

	.. code-block::

		var validNum = widget.validateSignature()

.. method:: PDFWidget.prototype.checkCertificate()

	|only_mutool|

	Returns "OK" if signature checked out OK, otherwise a text
	string containing an error message, e.g. "Self-signed
	certificate." or "Signature invalidated by change to
	document.", etc.

	:returns: string

	.. code-block::

		var result = widget.checkCertificate()

.. method:: PDFWidget.prototype.checkDigest()

	|only_mutool|

	Returns "OK" if digest checked out OK, otherwise a text string
	containing an error message.

	:returns: string

	.. code-block::

		var result = widget.checkDigest()

.. method:: PDFWidget.prototype.getSignatory()

	|only_mutool|

	Returns a text string with the distinguished name from a signed
	signature, or a text string with an error message.

	The returned string follows the format:

	``"cn=Name, o=Organization, ou=Organizational Unit,
	email=jane.doe@example.com, c=US"``

	:returns: string

	.. code-block::

		var signatory = widget.getSignatory()

.. TODO document what properties exist in the signatureConfig
.. TODO PDFPKCS7Signer... what do we do with this? mutool run has it, so better document it. maybe mupdf.js will gain PDF PKCS 7 digests in the future?

.. method:: PDFWidget.prototype.previewSignature(signer, signatureConfig, image, reason, location)

	|only_mutool|

	Return a `Pixmap` preview of what the signature would look like
	if signed with the given configuration. Reason and location may
	be ``undefined``, in which case they are not shown.

	:param PDFPKCS7Signer signer:
	:param Object signatureConfig:
	:param Image image:
	:param string reason:
	:param string location:

	:returns: Pixmap

	.. code-block::

		var pixmap = widget.previewSignature(
			signer,
			{
				showLabels: true,
				showDate: true
			},
			image,
			"",
			""
		)

.. method:: PDFWidget.prototype.sign(signer, signatureConfig, image, reason, location)

	|only_mutool|

	Sign the signature with the given configuration. Reason and
	location may be ``undefined``, in which case they are not shown.

	:param PDFPKCS7Signer signer:
	:param Object signatureConfig:
	:param Image image:
	:param string reason:
	:param string location:

	.. code-block::

		widget.sign(
			signer,
			{
				showLabels: true,
				showDate: true
			},
			image,
			"",
			""
		)

.. method:: PDFWidget.prototype.clearSignature()

	|only_mutool|

	Clear a signed signature, making it unsigned again.

	.. code-block::

		widget.clearSignature()

.. method:: PDFWidget.prototype.incrementalChangesSinceSigning()

	|only_mutool|

	Returns true if there have been incremental changes since the
	signature widget was signed.

	:returns: boolean

	.. code-block::

		var changed = widget.incrementalChangesSinceSigning()
