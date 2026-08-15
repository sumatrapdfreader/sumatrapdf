# Basics

## Loading a PDF

The following example demonstrates how to load a document and then print out the page count.
Ensure you have a PDF file named "test.pdf" alongside this example before running it.

	import * as mupdf from "mupdf"

	var doc = mupdf.Document.openDocument("test.pdf")
	console.log(doc.countPages())

## Creating a PDF

How to create a new PDF file with a single blank page:

	import * as mupdf from "mupdf"

	var doc = new mupdf.PDFDocument()
	doc.insertPage(-1, doc.addPage([0, 0, 595, 842], 0, null, ""))
	doc.save("blank.pdf")

## Adding an annotation

How to add a simple annotation to a PDF file:

	import * as mupdf from "mupdf"

	var doc = mupdf.Document.openDocument("blank.pdf")

	var page = doc.loadPage(0)

	var annot = page.createAnnotation("FreeText")
	annot.setRect([10, 10, 200, 100])
	annot.setContents("Hello, world!")

	page.update()

	doc.save("annotation.pdf")

## Converting a PDF to plain text

	import * as mupdf from "mupdf"

	var doc = mupdf.Document.openDocument("test.pdf")
	for (var i = 0; i < doc.countpages(); ++i) {
		var page = doc.loadPage(i)
		var text = page.toStructuredText().asText()
		console.log(text)
	}

## Converting a PDF to image files

	import * as fs from "fs"
	import * as mupdf from "mupdf"

	var doc = mupdf.Document.openDocument("test.pdf")
	for (var i = 0; i < doc.countpages(); ++i) {
		var page = doc.loadPage(i)
		var pixmap = page.toPixmap(
			mupdf.Matrix.scale(96 / 72, 96 / 72),
			mupdf.ColorSpace.DeviceRGB
		)
		var buffer = pixmap.asPNG()
		fs.writeFileSync("page" + (i+1) + ".png", buffer)
	}
