mutool extract
==============

The ``extract`` command can be used to extract images and font files from a PDF file. The image and font files will be saved out to the same folder which the file originates from.

.. code-block:: bash

	mutool extract [options] input.pdf [object numbers]

``[options]``
	Options are as follows:

	``-p`` password
		Use the specified password if the file is encrypted.

	``-r``
		Convert images to RGB when extracting them.

	``-a``
		Embed SMasks as alpha channel.

	``-N``
		Do not use ICC color conversions.

``input.pdf``
	Input file name. Must be a PDF file.

``[object numbers]``
	An array of object ids to extract from. If no object numbers are given
	on the command line, all images and fonts will be extracted from the
	file.
