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
		Do not bake form fields widgets into page contents.

	``-O`` options
		See :doc:`/reference/common/pdf-write-options`.
		If none are given ``garbage`` is used by default.

``input.pdf``
	Name of input PDF file.

``output.pdf``
	Name of output PDF file. If none is given ``out.pdf`` is used by default.

Normally annotations and form field widgets are separate content streams
that get drawn on top of the page contents. This command bakes annotations
and/or widgets into the page contents, making them inseparable.
After baking in annotations and widgets they are just visual representaitons
of what the original annotation/widget looked like, their properties can no
longer be edited since they no longer exist as proper annotation/widgets.
