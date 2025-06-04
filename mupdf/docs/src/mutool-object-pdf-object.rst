.. _mutool_object_pdf_object:



.. _mutool_run_js_api_pdf_object:



`PDFObject`
--------------


All functions that take `PDFObjects`, do automatic translation between
:title:`JavaScript` objects and `PDFObjects` using a few basic rules:


- Null, booleans, and numbers are translated directly.
- :title:`JavaScript` strings are translated to :title:`PDF` names, unless
  they are surrounded by parentheses: "Foo" becomes the :title:`PDF` name /Foo
  and "(Foo)" becomes the :title:`PDF` string (Foo).
- Arrays and dictionaries are recursively translated to :title:`PDF` arrays
  and dictionaries. Be aware of cycles though! The translation does NOT cope
  with cyclic references!
- The translation goes both ways: :title:`PDF` dictionaries and arrays can
  be accessed similarly to :title:`JavaScript` objects and arrays by getting
  and setting their properties.


|instance_props|

`length`

    Length of the array.




|instance_methods|

.. method:: get(ref)

    Access dictionaries and arrays in the `PDFObject`.

    :arg ref: Key or index.
    :return: The value for the key or index.

    |example_tag|

    .. code-block:: javascript

        var dict = pdfDocument.newDictionary();
        var value = dict.get("my_key");
        var arr = pdfDocument.newArray();
        var value = arr.get(1);


.. method:: put(ref, value)

    Put information into dictionaries and arrays in the `PDFObject`.
    Dictionaries and arrays can also be accessed using normal property syntax: `obj.Foo = 42; delete obj.Foo; x = obj[5]`.

    :arg ref: Key or index.
    :arg value: The value for the key or index.

    |example_tag|

    .. code-block:: javascript

        var dict = pdfDocument.newDictionary();
        dict.put("my_key", "my_value");
        var arr = pdfDocument.newArray();
        arr.put(0, 42);


.. method:: delete(ref)

    Delete a reference from a `PDFObject`.

    :arg ref: Key or index.

    |example_tag|

    .. code-block:: javascript

        pdfObj.delete("my_key");
        var dict = pdfDocument.newDictionary();
        dict.put("my_key", "my_value");
        dict.delete("my_key");
        var arr = pdfDocument.newArray();
        arr.put(1, 42);
        arr.delete(1);


.. method:: resolve()

    If the object is an indirect reference, return the object it points to; otherwise return the object itself.

    :return: Object.

    |example_tag|

    .. code-block:: javascript

        var resolvedObj = pdfObj.resolve();


.. method:: compare(other_obj)

    |mutool_tag|

    Compare the object to another one. Returns 0 on match, non-zero on mismatch. Streams always mismatch.

    :arg other: `PDFObject`.
    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var match = pdfObj.compare(other_obj);


.. method:: isArray()

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var result = pdfObj.isArray();

.. method:: isDictionary()

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var result = pdfObj.isDictionary();

.. method:: forEach(fun)

    Iterate over all the entries in a dictionary or array and call a function for each value-key pair.

    :arg fun: Function in the format `function(value,key){...}`.

    |example_tag|

    .. code-block:: javascript

        pdfObj.forEach(function(value,key){console.log("value="+value+",key="+key)});


.. method:: push(item)

    Append `item` to the end of an array.

    :arg item: Item to add.

    |example_tag|

    .. code-block:: javascript

        pdfObj.push("item");


.. method:: toString()

    Returns the object as a pretty-printed string.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var str = pdfObj.toString();


.. method:: valueOf()

    Convert primitive :title:`PDF` objects to a corresponding primitive `Null`,
    `Boolean`, `Number` or `String` :title:`JavaScript` objects.
    Indirect :title:`PDF` objects get converted to the string "R"
    while :title:`PDF` names are converted to plain strings.
    :title:`PDF` arrays or dictionaries are returned unchanged.

    :return: `Null` \| `Boolean` \| `Number` \| `String`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.valueOf();


.. method:: isIndirect()

    Is the object an indirect reference.

    :return: `Boolean`.


    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isIndirect();


.. method:: asIndirect()

    Return the object number the indirect reference points to.

    :return: `Integer`.


    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asIndirect();


.. method:: isFileSpec()

    Is the object a file specification (or a reference to a file specification).

    :return: `Boolean`.


    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isFileSpec();




:title:`PDF` streams
~~~~~~~~~~~~~~~~~~~~~~~~~

The only way to access a stream is via an indirect object, since all streams are numbered objects.


.. method:: isStream()

    *True* if the object is an indirect reference pointing to a stream.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isStream();


.. method:: readStream()

    Read the contents of the stream object into a `Buffer`.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pdfObj.readStream();

.. method:: readRawStream()

    Read the raw, uncompressed, contents of the stream object into a `Buffer`.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pdfObj.readRawStream();

.. method:: writeObject(obj)

    Update the object the indirect reference points to.

    :arg obj: Object to update.

    |example_tag|

    .. code-block:: javascript

        pdfObj.writeObject(obj);

.. method:: writeStream(buffer)

    Update the contents of the stream the indirect reference points to.
    This will update the "Length", "Filter" and "DecodeParms" automatically.

    :arg buffer: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        pdfObj.writeStream(buffer);

.. method:: writeRawStream(buffer)

    Update the contents of the stream the indirect reference points to.
    The buffer must contain already compressed data that matches
    the "Filter" and "DecodeParms". This will update the "Length"
    automatically, but leave the "Filter" and "DecodeParms" untouched.


    :arg buffer: `Buffer`.


    |example_tag|

    .. code-block:: javascript

        pdfObj.writeRawStream(buffer);


Primitive Objects
~~~~~~~~~~~~~~~~~~~~~~~~~


Primitive :title:`PDF` objects such as booleans, names, and numbers can usually be treated like :title:`JavaScript` values. When that is not sufficient use these functions:


.. method:: isNull()

    Returns *true* if the object is a `null` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isNull();

.. method:: isBoolean()

    Returns *true* if the object is a `Boolean` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isBoolean();

.. method:: asBoolean()

    Get the boolean primitive value.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asBoolean();

.. method:: isInteger()

    Returns *true* if the object is an `Integer` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isInteger();

.. method:: isNumber()

    Returns *true* if the object is a `Number` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isNumber();

.. method:: asNumber()

    Get the number primitive value.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asNumber();

.. method:: isName()

    Returns *true* if the object is a `Name` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isName();

.. method:: asName()

    Get the name as a string.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asName();

.. method:: isReal()

    |mutool_tag|

    Returns *true* if the object is a `Real` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isReal();

.. method:: isString()

    Returns *true* if the object is a `String` object.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.isString();

.. method:: asString()

    Convert a "text string" to a :title:`JavaScript` unicode string.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asString();

.. method:: asByteString()

    Convert a string to an array of byte values.

    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var val = pdfObj.asByteString();

.. method:: getNumber()

    Convert a primitive :title:`PDF` integer or real object to a `Number` :title:`JavaScript` object.

    :return: `Number`.

    |example_tag|

    .. code-block:: javascript

        var nbr = pdfObj.getNumber();

.. method:: getName()

    Convert a primitive :title:`PDF` name object to a `String` :title:`JavaScript` object.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var name = pdfObj.getName();

.. method:: getString()

    Convert a primitive :title:`PDF` string object to a `String` :title:`JavaScript` object.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var str = pdfObj.getString();

.. method:: getInheritable()

    Access dictionaries and array in the `PDFObject`.

    For a dictionary, if the requested key does not exist, getInheritable()
    will walk Parent references to parent dictionaries and lookup the same
    key there. If no key can be found in any parent or grand-parent or
    grand-grand-parent, all the way up, null is returned.

    :arg ref: Key or index.
    :return: The value for the key or index.

    |example_tag|

    .. code-block:: javascript

        var dict = pdfDocument.newDictionary();
        var grandParent = pdfDocument.newDictionary();
        var grandgrandParent = pdfDocument.newDictionary();
        grandgrandParent.put("my_key", "my_value");
        grandParent.put("Parent", grandgrandParent);
        dict.put("Parent", grandParent);
        var value = dict.getInheritable("my_key");
        var arr = pdfDocument.newArray();
        var value = arr.get(0);
