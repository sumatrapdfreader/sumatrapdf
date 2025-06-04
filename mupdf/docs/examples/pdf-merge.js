// A re-implementation of "mutool merge" in JavaScript.

import * as mupdf from "mupdf"

function copyPage(dstDoc, srcDoc, pageNumber, dstFromSrc) {
	var srcPage, dstPage
	srcPage = srcDoc.findPage(pageNumber)
	dstPage = dstDoc.newDictionary()
	dstPage.Type = dstDoc.newName("Page")
	if (srcPage.MediaBox) dstPage.MediaBox = dstFromSrc.graftObject(srcPage.MediaBox)
	if (srcPage.Rotate) dstPage.Rotate = dstFromSrc.graftObject(srcPage.Rotate)
	if (srcPage.Resources) dstPage.Resources = dstFromSrc.graftObject(srcPage.Resources)
	if (srcPage.Contents) dstPage.Contents = dstFromSrc.graftObject(srcPage.Contents)
	dstDoc.insertPage(-1, dstDoc.addObject(dstPage))
}

function copyAllPages(dstDoc, srcDoc) {
	var dstFromSrc = dstDoc.newGraftMap()
	var k, n = srcDoc.countPages()
	for (k = 0; k < n; ++k)
		copyPage(dstDoc, srcDoc, k, dstFromSrc)
}

function pdfmerge() {
	var srcDoc, dstDoc, i

	dstDoc = new mupdf.PDFDocument()
	for (i = 3; i < process.argv.length; ++i) {
		srcDoc = mupdf.Document.openDocument(process.argv[i])
		copyAllPages(dstDoc, srcDoc)
	}
	dstDoc.save(process.argv[2], "compress")
}

if (process.argv.length < 4)
	print("usage: mutool run pdf-merge.js output.pdf input1.pdf input2.pdf ...")
else
	pdfmerge()
