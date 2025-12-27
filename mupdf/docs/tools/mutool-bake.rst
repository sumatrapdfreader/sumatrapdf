mutool bake
==========================================

The ``bake`` command bakes PDF forms into static content.

.. code-block:: bash

	mutool bake [options] input.pdf [output.pdf]

``[options]``
	Options are as follows:

	``-A``
		Do not bake annotations into page contents.

	``-F``
		Do not bake form fields into page contents.

	``-O`` options
		See :doc:`/reference/common/pdf-write-options`.
		If none are given ``garbage`` is used by default.

``input.pdf``
	Name of input PDF file.

``output.pdf``
	Name of output PDF file. If none is given ``out.pdf`` is used by default.
