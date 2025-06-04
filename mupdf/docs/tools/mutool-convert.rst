.. _mutool_convert:

mutool convert
==========================================

The ``convert`` command converts an input file into another format.

.. code-block::

	mutool convert [options] file [pages]

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.
	``-o`` output
		The output file name. The output format is inferred from the output filename. Embed ``%d`` in the name to indicate the page number (for example: "page%d.png"). Printf modifiers are supported, for example "%03d". If no output is specified, the output will go to ``stdout``.
	``-F`` output format (default inferred from output file name)
		- raster: ``cbz``, ``png``, ``pnm``, ``pgm``, ``ppm``, ``pam``, ``pbm``, ``pkm``.
		- print-raster: ``pcl``, ``pclm``, ``ps``, ``pwg``.
		- vector: ``pdf``, ``svg``.
		- text: ``html``, ``xhtml``, ``text``, ``stext``.
	``-b`` box
		Use named page box (``MediaBox``, ``CropBox``, ``BleedBox``, ``TrimBox``, or ``ArtBox``).
	``-A`` bits
		Specify how many bits of anti-aliasing to use. The default is ``8``.
	``-W`` width
		Page width in points for EPUB layout.
	``-H`` height
		Page height in points for EPUB layout.
	``-S`` size
		Font size in points for EPUB layout.

	``-U`` filename
		User CSS stylesheet for EPUB layout.

	``-X``
		Disable document styles for EPUB layout.

	``-O`` comma separated list of options for output format.
		See
		:doc:`/reference/common/pdf-write-options`
		and
		:doc:`/reference/common/document-writer-options`.

``file``
	Input file name. The file can be any of the supported input formats.

``[pages]``
	Comma separated list of page ranges. The first page is "1", and the
	last page is "N". The default is "1-N".
