mutool draw
===========

The ``draw`` command will render a document to image files, convert to another vector format, or extract the text content.

- The supported input document formats are: ``pdf``, ``xps``, ``cbz``, and ``epub``.

- The supported output image formats are: ``pbm``, ``pgm``, ``ppm``, ``pam``, ``png``, ``pwg``, ``pcl`` and ``ps``.

- The supported output vector formats are: ``svg``, ``pdf``, and ``debug trace`` (as ``xml``).

- The supported output text formats are: ``plain text``, ``html``, and structured text (as ``xml`` or ``json``).

.. code-block:: bash

	mutool draw [options] file [pages]

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.
	``-o`` output
		The output file name. The output format is inferred from the output filename. Embed ``%d`` in the name to indicate the page number (for example: "page%d.png"). Printf modifiers are supported, for example "%03d". If no output is specified, the output will go to ``stdout`` for text output formats, for image output formats nothing is outputted.
	``-F`` format
		Enforce a specific output format. Only necessary when outputting to ``stdout`` since normally the output filename is used to infer the output format.
	``-q``
		Be quiet, do not print progress messages.
	``-R`` angle
		Rotate clockwise by given number of degrees.
	``-r`` resolution
		Render the page at the specified resolution. The default resolution is 72 dpi.
	``-w`` width
		Render the page at the specified width (or, if the ``-r`` flag is used, render with a maximum width).
	``-h`` height
		Render the page at the specified height (or, if the ``-r`` flag is used, render with a maximum height).
	``-f``
		Fit exactly; ignore the aspect ratio when matching specified width/heights.
	``-b`` box
		Use named page box (``MediaBox``, ``CropBox``, ``BleedBox``, ``TrimBox``, or ``ArtBox``).
	``-B`` bandheight
		Render in banded mode with each band no taller than the given height. This uses less memory during rendering. Only compatible with ``pam``, ``pgm``, ``ppm``, ``pnm`` and ``png`` output formats. Banded rendering and md5 checksumming may not be used at the same time.
	``-T`` threads
		Number of threads to use for rendering (banded mode only).
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
	``-a``
		Disable usage of accelerator file.
	``-c`` colorspace
		Render in the specified colorspace. Supported colorspaces are: ``mono``, ``gray``, ``grayalpha``, ``rgb``, ``rgbalpha``, ``cmyk``, ``cmykalpha``. Some abbreviations are allowed: ``m``, ``g``, ``ga``, ``rgba``, ``cmyka``. The default is chosen based on the output format.
	``-e`` filename
		Proof ICC profile filename for rendering.
	``-G`` gamma
		Apply gamma correction. Some typical values are 0.7 or 1.4 to thin or darken text rendering.
	``-I``
		Invert colors.
	``-s`` [mft5]
		Show various bits of information: ``m`` for glyph cache and total memory usage, ``f`` for page features such as whether the page is grayscale or color, ``t`` for per page rendering times as well statistics, and ``5`` for md5 checksums of rendered images that can be used to check if rendering has changed.
	``-A`` bits
		Specify how many bits of anti-aliasing to use. The default is ``8``. ``0`` means no anti-aliasing, ``9`` means no anti-aliasing, centre-of-pixel rule, ``10`` means no anti-aliasing, any-part-of-a-pixel rule.
	``-A`` graphics-bits/text-bits
		Specify separate numbers of bits for anti-aliasing for graphics and for text, use a slash ``/`` as separator.
	``-l`` width
		Minimum stroke line width (in pixels).
	``-K``
		Do not draw text.
	``-KK``
		Only draw text.
	``-D``
		Disable use of display lists. May cause slowdowns, but should reduce the amount of memory used.
	``-i``
		Ignore errors.
	``-m`` limit
		Limit memory usage in bytes.
	``-L``
		Low memory mode (avoid caching objects by clearing cache after each page).
	``-P``
		Run interpretation and rendering at the same time.
	``-N``
		Disable ICC workflow.
	``-O`` overprint
		Control spot/overprint rendering: ``0`` for no spot rendering, ``1`` for Overprint simulation (default), or ``2`` for full spot rendering.
	``-t`` language
		Specify language/script for OCR (default: eng)
	``-d`` ocr-file-path
		Specify path for OCR files (default: rely on ``TESSDATA_PREFIX`` environment variable.
	``-k`` correction
		Set the skew correction, either one of ``auto``, ``0`` for increase size, ``1`` for maintain size, or ``2`` for decrease size.
	``-k`` correction,angle
		Set the skew correction as well as the angle.
	``-y l``
		Print the layer configs to stderr.
	``-y`` layer-number
		Select layer config (by number from ``-y l``).
	``-y`` layer-number,item1,item2,...
		Select layer config (by number from ``-y l``) and toggle the listed items.
	``-Y``
		Print the individual layers to stderr.
	``-z`` layer-number
		Hide individual layer.
	``-Z`` layer-number
		Show individual layer.

``file``
	Input file name.

``[pages]``
	Comma separated list of page ranges. The first page is "1", and the last page is "N". The default is "1-N".
