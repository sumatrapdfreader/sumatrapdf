mutool show
===========

The ``show`` command will print the specified objects and streams to ``stdout``.
Streams are decoded and non-printable characters are represented with a period
by default.

.. code-block::

	mutool show [options] input.pdf ...

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.
	``-o`` output
		The output file name instead of using ``stdout``. Should be a plain text file format.
	``-e``
		Leave stream contents in their original form.
	``-b``
		Print only stream contents, as raw binary data.
	``-g``
		Print only object, one line per object, suitable for grep.
	``-r``
		Force repair before showing any objects.
	``-L``
		Show object labels (how to reach an object from the root)

``input.pdf``
	Input file name. Must be a PDF file.

``...``
	Specify what to show by using one of the following keywords, or a path to an object:

	``trailer``
		Print the trailer dictionary.

	``xref``
		Print the cross reference table.

	``pages``
		List the object numbers for every page.

	``grep``
		Print all the objects in the file in a compact one-line format
		suitable for piping to grep.

	``outline``
		Print the outline (also known as "table of contents" or
		"bookmarks").

	``js``
		Print document level Javascript.

	``form``
		Print form objects.

	``<path>``
		A path starts with either an object number, a property in the
		trailer dictionary, or the keyword "trailer" or "pages".
		Separate elements with a period '.' or slash '/'. Select a page
		object by using pages/N where N is the page number. The first
		page is number 1.

	``*``
		You can use ``*`` as an element to iterate over all array
		indices or dictionary properties in an object. Thus you can
		have multiple keywords with for your ``mutool show`` query.

Examples
--------

Find the number of pages in a document:

.. code-block:: bash

	mutool show input.pdf trailer/Root/Pages/Count

Print the content stream data of the first page:

.. code-block:: bash

	mutool show -b input.pdf pages/1/Contents

Print the names of all the font resources on all the pages:

.. code-block:: bash

	mutool show input.pdf pages/*/Resources/Font/*/BaseFont

Show all JPEG compressed stream objects:

.. code-block:: bash

	mutool show input.pdf grep | grep '/Filter/DCTDecode'
