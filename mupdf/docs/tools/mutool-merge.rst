mutool merge
============

The ``merge`` command is used to pick out pages from two or more files and merge them into a new output file.

.. code-block:: bash

	mutool merge [-o output.pdf] [-O options] input1.pdf [pages1] [input2.pdf] [pages2] ...

``[-o output.pdf]``
	The output filename. Defaults to "out.pdf" if not supplied.

``[-O options]``
	See :doc:`/reference/common/pdf-write-options`.

``input1.pdf``
	The first document.

``[pages1]``
	Comma separated list of page ranges for the first document (``input.pdf``). The first page is "1", and the last page is "N". The default is "1-N".

``[input2.pdf]``
	The second document.

``[pages2]``
	Comma separated list of page ranges for the second document (``input2.pdf``). The first page is "1", and the last page is "N". The default is "1-N".

``...``
	Indicates that we add as many additional ``[input]`` & ``[pages]`` pairs as required to merge multiple documents.
