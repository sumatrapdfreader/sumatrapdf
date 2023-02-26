.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_widget:



.. _mutool_run_js_api_object_pdf_widget:



`PDFWidget`
------------------------


.. method:: getFieldType()

    Return `String` indicating type of widget: "button", "checkbox", "combobox", "listbox", "radiobutton", "signature" or "text".

    :return: `String`.

.. method:: getFieldFlags()

    Return the field flags. Refer to the :title:`PDF` specification for their meanings.

    :return: `Integer` which determines the bit-field value.

.. method:: getRect()

    Get the widget bounding box.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: setRect(rect)

    Set the widget bounding box.

    :arg rect: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.


.. method:: getMaxLen()

    Get maximum allowed length of the string value.

    :return: `Integer`.

.. method:: getValue()

    Get the widget value.

    :return: `String`

.. method:: setTextValue(value)

    Set the widget string value.

    :arg value: `String`.

.. method:: setChoiceValue(value)

    Sets the value against the widget.

    :arg value: `String`.

.. method:: toggle()

    Toggle the state of the widget, returns `1` if the state changed.

    :return: `Integer`.

.. method:: getOptions()

    Returns an array of strings which represents the value for each corresponding radio button or checkbox field.

    :return: `[]`.

.. method:: layoutTextWidget()

    Layout the value of a text widget. Returns a :ref:`Text Layout Object<mutool_run_js_api_pdf_widget_text_layout_object>`.

    :return: `Object`.

.. method:: isReadOnly()

    If the value is read only and the widget cannot be interacted with.

    :return: `Boolean`.

.. method:: getLabel()

    Get the field name as a string.

    :return: `String`.

.. method:: getEditingState()

    Gets whether the widget is in editing state.

    :return: `Boolean`.


.. method:: setEditingState(state)

    Set whether the widget is in editing state.

    :arg state: `Boolean`.

.. note::

    When in editing state any changes to the widget value will not cause any side-effects such as changing other widgets or running :title:`JavaScript`. This is intended for, e.g. when a text widget is interactively having characters typed into it. Once editing is finished the state should reverted back, before updating the widget value again.

.. method:: update()

    Update the appearance stream to account for changes to the widget.


.. method:: isSigned()

    Returns :title:`true` if the signature is signed.

    :return: `Boolean`.

.. method:: validateSignature()

    Returns number of updates ago when signature became invalid. Returns `0` is signature is still valid, `1` if it became invalid during the last save, etc.

    :return: `Integer`.

.. method:: checkCertificate()

    Returns "OK" if signature checked out OK, otherwise a text string containing an error message, e.g. "Self-signed certificate." or "Signature invalidated by change to document.", etc.

    :return: `String`.


.. method:: getSignatory()

    Returns a text string with the distinguished name from a signed signature, or a text string with an error message.

    :return: `String`.

.. method:: previewSignature(signer, signatureConfig, image, reason, location)

    Return a :ref:`Pixmap<mutool_object_pixmap>` preview of what the signature would look like if signed with the given configuration. Reason and location may be `undefined`, in which case they are not shown.

    :arg signer: :ref:`PDFPKCS7Signer<mutool_object_pdf_widget_signer>`.
    :arg signatureConfig: :ref:`Signature Configuration Object<mutool_object_pdf_widget_signature_configuration>`.
    :arg image: :ref:`Image<mutool_object_image>`.
    :arg reason: `String`.
    :arg location: `String`.

    :return: `Pixmap`.


.. _mutool_object_pdf_widget_sign:

.. method:: sign(signer, signatureConfig, image, reason, location)

    Sign the signature with the given configuration. Reason and location may be `undefined`, in which case they are not shown.

    :arg signer: :ref:`PDFPKCS7Signer<mutool_object_pdf_widget_signer>`.
    :arg signatureConfig: :ref:`Signature Configuration Object<mutool_object_pdf_widget_signature_configuration>`.
    :arg image: :ref:`Image<mutool_object_image>`.
    :arg reason: `String`.
    :arg location: `String`.

.. method:: clearSignature()

    Clear a signed signature, making it unsigned again.



.. method:: eventEnter()

    Trigger the event when the pointing device enters a widget's active area.

.. method:: eventExit()

    Trigger the event when the pointing device exits a widget's active area.

.. method:: eventDown()

    Trigger the event when the pointing device's button is depressed within a widget's active area.

.. method:: eventUp()

    Trigger the event when the pointing device's button is released within a widget's active area.

.. method:: eventFocus()

    Trigger the event when the a widget gains input focus.

.. method:: eventBlur()

    Trigger the event when the a widget loses input focus.



.. _mutool_object_pdf_widget_signer:

`PDFPKCS7Signer`
------------------------

**Creating a Signer**

To create a signer object an instance of `PDFPKCS7Signer` is required.

.. method:: new (filename, password)

    Read a certificate and private key from a :title:`pfx` file and create a :title:`signer` to hold this information. Used with :ref:`PDFWidget.sign()<mutool_object_pdf_widget_sign>`.

    :arg filename: `String`.
    :arg password: `String`.

    :return: `PDFPKCS7Signer`.


    **Example**

    .. code-block:: javascript

        var signer = new PDFPKCS7Signer(<file_name>,<password>);
