.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_pdf_widget:



.. _mutool_run_js_api_object_pdf_widget:



`PDFWidget`
------------------------

Widgets refer to components which make up form items such as buttons, text inputs and signature fields.


To get the widgets on a page see: :ref:`PDFPage getWidgets()<mutool_run_js_api_pdf_page_getWidgets>`.




|instance_methods|


.. method:: getFieldType()

    |mutool_tag_wasm_soon|

    Return `String` indicating type of widget: "button", "checkbox", "combobox", "listbox", "radiobutton", "signature" or "text".

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var type = widget.getFieldType();

    .. |tor_todo| WASM: TypeError: PDFWidget.TYPES is not a function


.. method:: getFieldFlags()

    Return the field flags. Refer to the :title:`PDF` specification for their meanings.

    :return: `Integer` which determines the bit-field value.

    |example_tag|

    .. code-block:: javascript

        var flags = widget.getFieldFlags();


.. method:: getRect()

    Get the widget bounding box.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        var rect = widget.getRect();


.. method:: setRect(rect)

    Set the widget bounding box.

    :arg rect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        widget.setRect([0,0,100,100]);


.. method:: getMaxLen()

    Get maximum allowed length of the string value.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var length = widget.getMaxLen();


.. method:: getValue()

    Get the widget value.

    :return: `String`.


    |example_tag|

    .. code-block:: javascript

        var value = widget.getValue();


.. method:: setTextValue(value)

    Set the widget string value.

    :arg value: `String`.

    |example_tag|

    .. code-block:: javascript

        widget.setTextValue("Hello World!");

.. method:: setChoiceValue(value)

    Sets the value against the widget.

    :arg value: `String`.

    |example_tag|

    .. code-block:: javascript

        widget.setChoiceValue("Yes");


.. method:: toggle()

    Toggle the state of the widget, returns `1` if the state changed.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var state = widget.toggle();

.. method:: getOptions()

    Returns an array of strings which represents the value for each corresponding radio button or checkbox field.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var options = widget.getOptions();

    .. |tor_todo| In both WASM & mutool this always returned undefined?, I tried checkboxes & radio buttons


.. method:: layoutTextWidget()

    |mutool_tag_wasm_soon|

    Layout the value of a text widget. Returns a :ref:`Text Layout Object<mutool_run_js_api_pdf_widget_text_layout_object>`.

    :return: `Object`.

    |example_tag|

    .. code-block:: javascript

        var layout = widget.layoutTextWidget();

    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: isReadOnly()

    If the value is read only and the widget cannot be interacted with.

    :return: `Boolean`.


    |example_tag|

    .. code-block:: javascript

        var isReadOnly = widget.isReadOnly();


.. method:: getLabel()

    Get the field name as a string.

    :return: `String`.


    |example_tag|

    .. code-block:: javascript

        var label = widget.getLabel();


.. method:: getEditingState()

    |mutool_tag_wasm_soon|

    Gets whether the widget is in editing state.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var state = widget.getEditingState();

    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: setEditingState(state)

    |mutool_tag_wasm_soon|

    Set whether the widget is in editing state.

    :arg state: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        widget.getEditingState(false);


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. note::

    When in editing state any changes to the widget value will not cause any side-effects such as changing other widgets or running :title:`JavaScript`. This is intended for, e.g. when a text widget is interactively having characters typed into it. Once editing is finished the state should reverted back, before updating the widget value again.

.. method:: update()

    Update the appearance stream to account for changes to the widget.

    |example_tag|

    .. code-block:: javascript

        widget.update();


Signature Methods
~~~~~~~~~~~~~~~~~~~~~~

.. method:: isSigned()

    |mutool_tag_wasm_soon|

    Returns :title:`true` if the signature is signed.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isSigned = widget.isSigned();

    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: validateSignature()

    |mutool_tag_wasm_soon|

    Returns number of updates ago when signature became invalid. Returns `0` is signature is still valid, `1` if it became invalid during the last save, etc.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var validNum = widget.validateSignature();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: checkCertificate()

    |mutool_tag_wasm_soon|

    Returns "OK" if signature checked out OK, otherwise a text string containing an error message, e.g. "Self-signed certificate." or "Signature invalidated by change to document.", etc.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var result = widget.checkCertificate();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: getSignatory()

    |mutool_tag_wasm_soon|

    Returns a text string with the distinguished name from a signed signature, or a text string with an error message.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var signatory = widget.getSignatory();

    .. |tor_todo| Source file has a todo for "getSignature", should this be getSignatory ?

.. method:: previewSignature(signer, signatureConfig, image, reason, location)

    |mutool_tag_wasm_soon|

    Return a :ref:`Pixmap<mutool_object_pixmap>` preview of what the signature would look like if signed with the given configuration. Reason and location may be `undefined`, in which case they are not shown.

    :arg signer: :ref:`PDFPKCS7Signer<mutool_object_pdf_widget_signer>`.
    :arg signatureConfig: :ref:`Signature Configuration Object<mutool_object_pdf_widget_signature_configuration>`.
    :arg image: :ref:`Image<mutool_object_image>`.
    :arg reason: `String`.
    :arg location: `String`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pixmap = widget.previewSignature(signer, {showLabels:true, showDate:true}, image, "", "");


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)



.. _mutool_object_pdf_widget_sign:

.. method:: sign(signer, signatureConfig, image, reason, location)

    |mutool_tag_wasm_soon|

    Sign the signature with the given configuration. Reason and location may be `undefined`, in which case they are not shown.

    :arg signer: :ref:`PDFPKCS7Signer<mutool_object_pdf_widget_signer>`.
    :arg signatureConfig: :ref:`Signature Configuration Object<mutool_object_pdf_widget_signature_configuration>`.
    :arg image: :ref:`Image<mutool_object_image>`.
    :arg reason: `String`.
    :arg location: `String`.


    |example_tag|

    .. code-block:: javascript

        widget.sign(signer, {showLabels:true, showDate:true}, image, "", "");


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: clearSignature()

    |mutool_tag_wasm_soon|

    Clear a signed signature, making it unsigned again.

    |example_tag|

    .. code-block:: javascript

        widget.clearSignature();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


Widget Events
~~~~~~~~~~~~~~~~~~~~~


.. method:: eventEnter()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device enters a widget's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventEnter();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)


.. method:: eventExit()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device exits a widget's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventExit();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventDown()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device's button is depressed within a widget's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventDown();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventUp()

    |mutool_tag_wasm_soon|

    Trigger the event when the pointing device's button is released within a widget's active area.

    |example_tag|

    .. code-block:: javascript

        widget.eventUp();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventFocus()

    |mutool_tag_wasm_soon|

    Trigger the event when the a widget gains input focus.

    |example_tag|

    .. code-block:: javascript

        widget.eventFocus();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)

.. method:: eventBlur()

    |mutool_tag_wasm_soon|

    Trigger the event when the a widget loses input focus.

    |example_tag|

    .. code-block:: javascript

        widget.eventBlur();


    .. |tor_todo| WASM, Even says "TODO" in the mupdf.js source file :)



.. _mutool_object_pdf_widget_signer:

`PDFPKCS7Signer`
------------------------

**Creating a Signer**

To create a signer object an instance of `PDFPKCS7Signer` is required.

.. method:: new (filename, password)

    |mutool_tag_wasm_soon|

    Read a certificate and private key from a :title:`pfx` file and create a :title:`signer` to hold this information. Used with :ref:`PDFWidget.sign()<mutool_object_pdf_widget_sign>`.

    :arg filename: `String`.
    :arg password: `String`.

    :return: `PDFPKCS7Signer`.


    |example_tag|

    .. code-block:: javascript

        var signer = new PDFPKCS7Signer(<file_name>, <password>);


    .. |tor_todo| WASM - no such class.
