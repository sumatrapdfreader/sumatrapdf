.. default-domain:: js

.. highlight:: javascript

Archive
==============

|only_mutool|

Constructors
------------

.. class:: Archive(path)

	Create a new archive based either on a tar- or zip-file or the contents of a directory.

	:param string path: Path string to the archive file or directory.

	.. code-block::

		var archive1 = new mupdf.Archive("example1.zip")
		var archive2 = new mupdf.Archive("example2.tar")
		var archive3 = new mupdf.Archive("images/")

.. class:: Archive(buffer)

	Create a new archive based either on a tar- or zip-file contained in a buffer.

	:param Buffer | ArrayBuffer | Uint8Array | string buffer: Buffer containing a archive.

	.. code-block::

		var archive1 = new mupdf.Archive(fs.readFileSync("example1.zip"))
		var archive2 = new mupdf.Archive(fs.readFileSync("example1.tar"))

Instance methods
-----------------

.. method:: Archive.prototype.getFormat()

	Returns a string describing the archive format.

	:returns: string

	.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		print(archive.getFormat())

.. method:: Archive.prototype.countEntries()

	Returns the number of entries in the archive.

	:returns: number

	.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		var numEntries = archive.countEntries()

.. method:: Archive.prototype.listEntry(idx)

	Returns the name of entry number idx in the archive, or null if idx is
	out of range.

	:param number idx: Entry index.

	:returns: string | null

	.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		var entry = archive.listEntry(0)

.. method:: Archive.prototype.hasEntry(name)

	Returns true if an entry of the given name exists in the archive.

	:param string name: Entry name to look for.

	:returns: boolean

	.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		var hasEntry = archive.hasEntry("file1.txt")

.. method:: Archive.prototype.readEntry(name)

	Returns the contents of the entry of the given name.

	:param string name: Name of entry to look for.

	:returns: `Buffer`

	.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		var contents = archive.readEntry("file1.txt")

Examples
--------

.. code-block::

		var archive = new mupdf.Archive("example1.zip")
		var n = archive.countEntries()
		for (var i = 0; i < n; ++i) {
			var entry = archive.listEntry(i)
			var contents = archive.readEntry(entry)
			console.log("entry", entry, contents.length)
		}
