Coordinate Systems
=================================

In PDF, the origin (0, 0) of a page is located at its bottom-left corner. In
MuPDF, the origin of a page is located at its top-left corner.

.. image:: /images/img-coordinate-space.webp
	:width: 75%

The exact coordinate of the page's corner may not always be (0, 0) but is
defined by its CropBox in relation to the MediaBox.

The rotation of the page also affects the PDF to MuPDF coordinate space
relationship; the rotation automatically adjusted for and hidden in the MuPDF
space.

Coordinates are real numbers and measured in points (1/72 of an inch).

The PDF page may override this convention by setting a UserUnit (must be larger than 1/72").

Example
-------

In MuPDF interfaces we use the MuPDF coordinate space.

.. code-block:: javascript

    var rectangle = [0,0,200,200]

Results in:

.. image:: /images/200x200-rect.webp
	:width: 200px

Working at the PDF object level
-------------------------------

Sometimes you may want to work directly with the PDF objects without using the MuPDF high level functions.
When doing this, you need to write the PDF coordinates as the PDF file expects them.

.. code-block:: javascript

	var contents = document.addStream(`
		q			% Save graphics state
		200 0 0 200 10 10 cm	% Scale and translate to position image
		/Image1 Do		% Fill image
		Q			% Restore graphics state
	`)

Then in this case we are working in the PDF coordinate space!

Use the `PDFPage.prototype.getTransform` to get a matrix that can be used to transform between the coordinate spaces.
