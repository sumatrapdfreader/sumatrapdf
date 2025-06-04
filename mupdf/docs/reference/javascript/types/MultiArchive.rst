.. default-domain:: js

.. highlight:: javascript

MultiArchive
============

|only_mutool|

Constructors
------------

.. class:: MultiArchive()

	Create a new empty multi archive.

	.. code-block::

		var multiArchive = new mupdf.MultiArchive()

Instance methods
----------------

.. method:: MultiArchive.prototype.mountArchive(subArchive, path)

	Add an archive to the set of archives handled by a multi archive.
	If ``path`` is ``null``, the ``subArchive`` contents appear at the
	top-level, otherwise they will appear prefixed by the string
	``path``.

	:param Archive subArchive: An archive that will be a child archive of this one.
	:param string path: The path at which the archive will be inserted.

	.. code-block::
		:caption:
			Assuming that ``example1.zip`` contains a ``file1.txt``
			and ``example2.tar`` contains ``file2.txt``, the
			multiarchive now allows access to "file1.txt" and
			"subpath/file2.txt".

		var archive = new mupdf.MultiArchive()
		archive.mountArchive(new mupdf.Archive("example1.zip"), null)
		archive.mountArchive(new mupdf.Archive("example2.tar"), "subpath")
		print(archive.hasEntry("file1.txt"))
		print(archive.hasEntry("subpath/file2.txt"))
