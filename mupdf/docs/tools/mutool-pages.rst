mutool pages
============

The ``pages`` command lists information about each page: MediaBox, CropBox, Rotation, UserUnit, etc.

.. code-block:: bash

	mutool pages [options] input.pdf [pages]

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.

``input.pdf``
	Input file name. Must be a PDF file.

``[pages]``
	Comma separated list of page numbers and ranges. If no pages are
	supplied then all document pages will be considered for the output
	file.
