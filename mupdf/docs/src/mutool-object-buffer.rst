.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_buffer:

.. _mutool_run_js_api_buffer:


`Buffer`
--------------

`Buffer` objects are used for working with binary data. They can be used much like arrays, but are much more efficient since they only store bytes.



.. method:: new Buffer()

    *Constructor method*.

    Create a new empty buffer.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = new mupdf.Buffer();


.. method:: new Buffer(original)

    *Constructor method*.

    Create a new buffer with a copy of the data from the original buffer.

    :arg original: `Buffer`.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = new mupdf.Buffer(buffer);


.. method:: readFile(fileName)

    |mutool_tag|

    *Constructor method*.

    Create a new buffer with the contents of a file.

    :arg fileName: The path to the file to read.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = mupdf.readFile("my_file.pdf");



|instance_props|



`length`

    |mutool_tag|

    The number of bytes in the buffer. `Read-only`.


`[n]`

    |mutool_tag|

    Read/write the byte at index 'n'. Will throw exceptions on out of bounds accesses.


    |example_tag|

    .. code-block:: javascript

        var byte = buffer[0];




|instance_methods|

.. method:: getLength()

    |wasm_tag|

    Returns the number of bytes in the buffer. `Read-only`.

    :return: `Integer`.

    |example_tag|

    .. code-block:: javascript

        var length = buffer.getLength();


.. method:: writeByte(b)

    Append a single byte to the end of the buffer.

    :arg b: The byte value. Only the least significant 8 bits of the value are appended to the buffer.

    |example_tag|

    .. code-block:: javascript

        buffer.writeByte(0x2a);


.. method:: readByte(at)

    |wasm_tag|

    Read the byte at the supplied index.

    :arg at: `Integer`.

    |example_tag|

    .. code-block:: javascript

        buffer.readByte(0);


.. method:: writeRune(c)

    |mutool_tag|

    Encode a unicode character as UTF-8 and append to the end of the buffer.

    :arg c: The character unicode codepoint.

    |example_tag|

    .. code-block:: javascript

        buffer.writeRune(0x4f60); // To append U+4f60
        buffer.writeRune(0x597d); // To append U+597d
        buffer.writeRune(0xff01); // To append U+ff01

.. method:: writeLine(...)

    Append arguments to the end of the buffer, separated by spaces, ending with a newline.

    :arg ...: List of arguments.

    |example_tag|

    .. code-block:: javascript

        buffer.writeLine("a line");


.. method:: write(...)

    Append arguments to the end of the buffer, separated by spaces.

    :arg ...: List of arguments.

    |example_tag|

    .. code-block:: javascript

        buffer.write("hello", "world");


.. method:: writeBuffer(data)

    Append the contents of the `data` buffer to the end of the buffer.

    :arg data: Data buffer.

    |example_tag|

    .. code-block:: javascript

        buffer.writeBuffer(anotherBuffer);


.. method:: slice(start end)

    |mutool_tag|

    Create a new buffer containing a (subset of) the data in this buffer. Start and end are offsets from the beginning of this buffer, and if negative from the end of this buffer.

    :arg start: Start index.
    :arg end: End index.

    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = new Buffer();
        buffer.write("hello", "world"); // buffer contains "hello world"
        var newBuffer = buffer.slice(1, -1); // newBuffer contains "ello worl"


.. method:: save(fileName)

    |mutool_tag|

    Write the contents of the buffer to a file.

    :arg fileName: Filename to save to.

    |example_tag|

    .. code-block:: javascript

        buffer.save("my_buffer_filename");


.. method:: asUint8Array()

    |wasm_tag|

    Returns the buffer as a `Uint8Array`.

    :return: `Uint8Array`.

    |example_tag|

    .. code-block:: javascript

        var arr = buffer.asUint8Array();


.. method:: asString()

    |wasm_tag|

    Returns the buffer as a `String`.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var str = buffer.asString();
