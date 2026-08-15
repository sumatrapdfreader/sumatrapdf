.. default-domain:: js

.. highlight:: javascript

Buffer
======

Buffer objects are used for working with binary data, e.g. returned from
:func:`asJPEG`. They can be used much like arrays, but are much more
efficient since they only store bytes.

Constructors
------------

.. class:: Buffer(data)

	Creates a new Buffer initialized with contents from data.

	:param Buffer | ArrayBuffer | Uint8Array | string | null data: Contents used to populate buffer, or leave it empty if ``null``.

	.. code-block::

		var buffer = new mupdf.Buffer("hello world")

Instance properties
-------------------

.. attribute:: Buffer.prototype.length

	The number of bytes in this buffer (read-only).

	:throws: TypeError on attempted writes.

	:returns: number

.. attribute:: Buffer.prototype.[n]

	|only_mutool|

	Get or set the byte at index ``n``.

	See `readByte()` and `writeByte()` for the equivalent in mupdf.js.

	:throws: RangeError on out of bounds accesses.

	.. code-block::

		var byte = buffer[0]

Instance methods
----------------

.. method:: Buffer.prototype.asString()

	Returns the contents of this buffer as a string.

	:returns: string

	.. code-block::

		var str = buffer.asString()

.. method:: Buffer.prototype.asUint8Array()

	|only_mupdfjs|

	Returns the contents of this buffer as a Uint8Array.

	:returns: Uint8Array

	.. code-block::

		var arr = buffer.asUint8Array()

.. method:: Buffer.prototype.getLength()

	Returns the number of bytes in this buffer.

	:returns: number

	.. code-block::

		var length = buffer.getLength()

.. method:: Buffer.prototype.readByte(at)

	Returns the byte at the specified index ``at`` if within ``0 <= at < getLength()``. Otherwise returns ``undefined``.

	:param number at: Index to read byte at.

	:returns: number

	.. code-block::

		buffer.readByte(0)

.. method:: Buffer.prototype.slice(start, end)

	Create a new buffer containing a (subset of) the data in this buffer.
	Start and end are offsets from the beginning of this buffer, and if negative from the end of this buffer.
	If ``start`` points to the end of this buffer, or if ``end`` point to at or before ``start``, then an empty buffer will be returned.

	:param number start: Start index.
	:param number end: End index (optional).

	:returns: `Buffer`

	.. code-block::

		var buffer = new mupdf.Buffer()
		buffer.write("hello world") // buffer contains "hello world"
		var newBuffer = buffer.slice(1, -1) // newBuffer contains "ello worl"

.. method:: Buffer.prototype.write(str)

	Append the string as UTF-8 to the end of this buffer.

	:param string str: String to append.

	.. code-block::

		buffer.write("hello world")

.. method:: Buffer.prototype.writeBuffer(data)

	Append the contents of the ``data`` buffer to the end of this buffer.

	:param Buffer | ArrayBuffer | Uint8Array | string data: Data buffer to append.

	.. code-block::

		buffer.writeBuffer(anotherBuffer)

.. method:: Buffer.prototype.writeByte(byte)

	Append a single byte to the end of this buffer.
	Only the least significant 8 bits of the value are appended.

	:param number byte: The byte value to append.

	.. code-block::

		buffer.writeByte(0x2a)

.. method:: Buffer.prototype.writeLine(str)

	Append string to the end of this buffer ending with a newline.

	:param string str: String to append.

	.. code-block::

		buffer.writeLine("a line")

.. method:: Buffer.prototype.writeRune(c)

	|only_mutool|

	Encode a unicode character as UTF-8 and append to the end of
	the buffer.

	:param number c: The character unicode codepoint.

	.. code-block:: javascript

		buffer.writeRune(0x4f60) // To append U+4f60, 你
		buffer.writeRune(0x597d) // To append U+597d, 好
		buffer.writeRune(0xff01) // To append U+ff01, ！

.. method:: Buffer.prototype.save(filename)

	Write the contents of the buffer to a file.

	:param string filename: Filename to save to.

	.. code-block:: javascript

		buffer.save("buffer.dat")
