.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_pdf_object:



.. _mutool_run_js_api_pdf_object:



`PDFObject`
--------------


All functions that take `PDFObjects`, do automatic translation between :title:`JavaScript` objects and `PDFObjects` using a few basic rules:


- Null, booleans, and numbers are translated directly.
- :title:`JavaScript` strings are translated to :title:`PDF` names, unless they are surrounded by parentheses: "Foo" becomes the :title:`PDF` name /Foo and "(Foo)" becomes the :title:`PDF` string (Foo).
- Arrays and dictionaries are recursively translated to :title:`PDF` arrays and dictionaries. Be aware of cycles though! The translation does NOT cope with cyclic references!
- The translation goes both ways: :title:`PDF` dictionaries and arrays can be accessed similarly to :title:`JavaScript` objects and arrays by getting and setting their properties.


----

**Instance properties**

`length`

    Length of the array.


----

**Instance methods**

.. method:: get(ref)

    Access dictionaries and arrays in the `PDFObject`.

    :arg ref: Key or index.
    :return: The value for the key or index.


.. method:: put(ref, value)

    Put information into dictionaries and arrays in the `PDFObject`. Dictionaries and arrays can also be accessed using normal property syntax: `obj.Foo = 42; delete obj.Foo; x = obj[5]`.

    :arg ref: Key or index.
    :arg value: The value for the key or index.


.. method:: delete(ref)

    Delete a reference from a `PDFObject`.

    :arg ref: Key or index.

.. method:: resolve()

    If the object is an indirect reference, return the object it points to; otherwise return the object itself.

    :return: Object.

.. method:: isArray()

    :return: `Boolean`.

.. method:: isDictionary()

    :return: `Boolean`.

.. method:: forEach(fun)

    Iterate over all the entries in a dictionary or array and call `fun` for each key-value pair.

    :arg fun: Function in the format `function(key,value){...}`.


.. method:: push(item)

    Append `item` to the end of the array.

    :arg item: Item to add.


.. method:: toString()

    Returns the object as a pretty-printed string.

    :return: `String`.


.. method:: valueOf()

    Convert primitive :title:`PDF` objects to a corresponding primitive `Null`, `Boolean`, `Number` or `String` :title:`JavaScript` objects. Indirect :title:`PDF` objects get converted to the string "R" while :title:`PDF` names are converted to plain strings. :title:`PDF` arrays or dictionaries are returned unchanged.

    :return: `Null` \| `Boolean` \| `Number` \| `String`.


----

**PDF streams**

The only way to access a stream is via an indirect object, since all streams are numbered objects.


.. method:: isIndirect()

    Is the object an indirect reference.

    :return: `Boolean`.

.. method:: asIndirect()

    Return the object number the indirect reference points to.

    :return: `Boolean`.


.. method:: isStream()

    *True* if the object is an indirect reference pointing to a stream.

    :return: `Boolean`.


.. method:: readStream()

    Read the contents of the stream object into a `Buffer`.

    :return: `Buffer`.

.. method:: readRawStream()

    Read the raw, uncompressed, contents of the stream object into a `Buffer`.

    :return: `Buffer`.

.. method:: writeObject(obj)

    Update the object the indirect reference points to.

    :arg obj: Object to update.

.. method:: writeStream(buffer)

    Update the contents of the stream the indirect reference points to. This will update the "Length", "Filter" and "DecodeParms" automatically.

    :arg buffer: `Buffer`.

.. method:: writeRawStream(buffer)

    Update the contents of the stream the indirect reference points to. The buffer must contain already compressed data that matches the "Filter" and "DecodeParms". This will update the "Length" automatically, but leave the "Filter" and "DecodeParms" untouched.


    :arg buffer: `Buffer`.


----


Primitive :title:`PDF` objects such as booleans, names, and numbers can usually be treated like :title:`JavaScript` values. When that is not sufficient use these functions:


.. method:: isNull()

    Returns *true* if the object is a `null` object.

    :return: `Boolean`.

.. method:: isBoolean()

    Returns *true* if the object is a `Boolean` object.

    :return: `Boolean`.

.. method:: asBoolean()

    Get the boolean primitive value.

    :return: `Boolean`.

.. method:: isNumber()

    Returns *true* if the object is a `Number` object.

    :return: `Boolean`.

.. method:: asNumber()

    Get the number primitive value.

    :return: `Integer`.

.. method:: isName()

    Returns *true* if the object is a `Name` object.

    :return: `Boolean`.

.. method:: asName()

    Get the name as a string.

    :return: `String`.

.. method:: isString()

    Returns *true* if the object is a `String` object.

    :return: `Boolean`.

.. method:: asString()

    Convert a "text string" to a :title:`JavaScript` unicode string.

    :return: `String`.

.. method:: asByteString()

    Convert a string to an array of byte values.

    :return: `[...]`.
