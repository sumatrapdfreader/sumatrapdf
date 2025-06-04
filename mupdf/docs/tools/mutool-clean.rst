mutool clean
==========================================

The ``clean`` command pretty prints and rewrites the syntax of a PDF file. It can be used to repair broken files, expand compressed streams, filter out a range of pages, etc.

.. code-block:: bash

	mutool clean [options] input.pdf [output.pdf] [pages]

``[options]``
	Options are as follows:

	``-p`` password
		 Use the specified password if the file is encrypted.

	``-g``
		 Garbage collect unused objects.

	``-gg``
		 In addition to ``-g`` compact xref table.

	``-ggg``
		 In addition to ``-gg`` merge duplicate objects.

	``-gggg``
		 In addition to ``-ggg`` check streams for duplication.

	``-l``
		 Linearize PDF (no longer supported!).

	``-D``
		 Save file without encryption.

	``-E`` encryption
		 Save file with new encryption (``rc4-40``, ``rc4-128``, ``aes-128``, or ``aes-256``).

	``-O`` owner_password
		 Owner password (only if encrypting).

	``-U`` user_password
		 User password (only if encrypting).

	``-P`` permission
		 Permission flags (only if encrypting).

	``-a``
		 ASCII hex encode binary streams.

	``-d``
		 Decompress streams.

	``-z``
		 Deflate uncompressed streams.

	``-f``
		 Compress font streams.

	``-i``
		 Compress image streams.

	``-c``
		 Pretty-print graphics commands in content streams.

	``-s``
		 Sanitize graphics commands in content streams.

	``-t``
		 Compact object syntax.

	``-tt``
		 Use indented object syntax to make PDF objects more readable.

	``-L``
		 Print comments containing labels showing how each object can be reached from the Root.

	``-A``
		 Create appearance streams for annotations that are missing appearance streams.

	``-AA``
		 Recreate appearance streams for all annotations.

	``-m``
		 Preserve metadata.

	``-S``
		 Subset fonts if possible. (EXPERIMENTAL!)

	``-Z``
		 Use object streams cross reference streams for extra compression.

	``--(color|gray|bitonal)-(|lossy-|lossless-)image-subsample-method method``
		 Set the subsampling method (``average``, or ``bicubic``) for the desired image types, for example color-lossy and bitonal-lossless.

	``--(color|gray|bitonal)-(|lossy-|lossless-)image-subsample-dpi dpi``
		 Set the resolution at which to subsample.

	``--(color|gray|bitonal)-(|lossy-|lossless-)image-recompress-method quality``
		 Set the recompression quality to either of ``never``, ``same``, ``lossless``, ``jpeg``, ``j2k``, ``fax``, or ``jbig2``.

	``--structure=keep|drop``
		 Keep or drop the structure tree.

``input.pdf``
	Input file name. Must be a PDF file.

``[output.pdf]``
	The output file. Must be a PDF file.

	If no output file is specified, it will write the cleaned PDF to "out.pdf" in the current directory.

``[pages]``
	Comma separated list of page numbers and ranges (for example:
	1,5,10-15,20-N), where the character N denotes the last page. If no
	pages are specified, then all pages will be included.
