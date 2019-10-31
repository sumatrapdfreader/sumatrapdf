// Script to create a PDF from JPEG2000 images.
// Each image be put on its own page.
// This script can be used to create files to test JPEG2000 support in PDF viewers.

var doc = new PDFDocument();

function addJPXImage(filename, w, h) {
	return doc.addRawStream(
		readFile(filename),
		{
			Type: "XObject",
			Subtype: "Image",
			Width: w,
			Height: h,
			Filter: "JPXDecode"
		}
	);
}

function addJPXPage(filename) {
	var image = new Image(filename);
	var w = image.getWidth();
	var h = image.getHeight();
	var mediabox = [0, 0, w, h];
	var resources = { XObject: { I: addJPXImage(filename, w, h) } };
	var contents = "q " + w + " 0 0 " + h + " 0 0 cm /I Do Q";
	doc.insertPage(-1, doc.addPage(mediabox, 0, resources, contents));
}

var i, n = scriptArgs.length;
if (n < 1) {
	print("usage: mutool run jpx-to-pdf.js file.jpx ...");
	quit();
}
for (i = 0; i < n; ++i) {
	addJPXPage(scriptArgs[i]);
}

doc.save("out.pdf", "ascii,pretty");
