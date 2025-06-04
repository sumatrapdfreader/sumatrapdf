mutool barcode
==============

The ``barcode`` command is used to either decode a barcode from an image or
document, or to create a new barcode image.

	This command may not be available!

	It is only present if MuPDF was compiled with the ZXing barcode library.

Decoding barcodes
~~~~~~~~~~~~~~~~~

.. code-block:: bash

	mutool barcode -d [options] file1.pdf [pages1] [file2.pdf [pages2] ...]

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.

	``-o`` output
		The output file name (e.g. "output.txt"). If this option is not
		present, any text from decoded barcodes will be printed to
		standard out.

	``-r`` rotation
		How much to rotate the input pages in degrees (0-360), before trying to decode any barcodes.

``file1`` and ``file2``, etc.
	Input file name. The input can be any of the supported document formats.

``[pages1]`` and ``pages2``, etc.
	Comma separated list of page ranges. The first page is "1", and the last page is "N". The default is "1-N".

Encoding barcodes
~~~~~~~~~~~~~~~~~

.. code-block:: bash

	mutool barcode -c [options] text

``[options]``
	Options are as follows:

	``-o`` output
		The output file name. PNG or PDF format is chosen depending on
		the file extension. If none is given, the default is
		``out.png``.

	``-F`` format
		The desired output barcode format:

		- ``aztec``
		- ``codabar``
		- ``code39``
		- ``code93``
		- ``code128``
		- ``databar``
		- ``databarexpanded``
		- ``datamatrix``
		- ``ean8``
		- ``ean13``
		- ``itf``
		- ``maxicode``
		- ``pdf417``
		- ``qrcode``
		- ``upca``
		- ``upce``
		- ``microqrcode``
		- ``rmqrcode``
		- ``dxfilmedge``
		- ``databarlimited``

	``-s`` size
		Set size in pixels for the output barcode. If not specified,
		the smallest size that can be decoded is chosen.

	``-q``
		Add quiet zones around the barcode. This puts an empty margin
		around created barcodes.

	``-t``
		Add human-readable text, when available. Some barcodes, e.g.
		EAN-13, may have the barcode contents printed in human-readable
		text next to the barcode, which is enabled by this flag.

	``-e`` level
		Set error correction level (0-8). Some barcodes, e.g. QR-codes,
		support several levels of error correction, which can be
		customized through this option.

``text``
	 The text to be encoded into a barcode.
