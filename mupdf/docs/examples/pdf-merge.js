// A re-implementation of "mutool merge" in JavaScript.

function graftObject(dstDoc, srcDoc, srcObj, map) {
	var srcNum, dstRef, dstObj
	if (!map)
		map = []
	if (srcObj.isIndirect()) {
		srcNum = srcObj.toIndirect()
		if (map[srcNum])
			return map[srcNum]
		map[srcNum] = dstRef = dstDoc.createObject()
		dstRef.writeObject(graftObject(dstDoc, srcDoc, srcObj.resolve(), map))
		if (srcObj.isStream())
			dstRef.writeRawStream(srcObj.readRawStream())
		return dstRef
	}
	if (srcObj.isArray()) {
		dstObj = dstDoc.newArray()
		srcObj.forEach(function (key, val) {
			dstObj[key] = graftObject(dstDoc, srcDoc, val, map)
		})
		return dstObj
	}
	if (srcObj.isDictionary()) {
		dstObj = dstDoc.newDictionary()
		srcObj.forEach(function (key, val) {
			dstObj[key] = graftObject(dstDoc, srcDoc, val, map)
		})
		return dstObj
	}
	return srcObj /* primitive objects are not bound to a document */
}

function copyPage(dstDoc, srcDoc, pageNumber, map) {
	var srcPage, dstPage
	srcPage = srcDoc.findPage(pageNumber)
	dstPage = dstDoc.newDictionary()
	dstPage.Type = dstDoc.newName("Page")
	if (srcPage.MediaBox) dstPage.MediaBox = graftObject(dstDoc, srcDoc, srcPage.MediaBox, map)
	if (srcPage.Rotate) dstPage.Rotate = graftObject(dstDoc, srcDoc, srcPage.Rotate, map)
	if (srcPage.Resources) dstPage.Resources = graftObject(dstDoc, srcDoc, srcPage.Resources, map)
	if (srcPage.Contents) dstPage.Contents = graftObject(dstDoc, srcDoc, srcPage.Contents, map)
	dstDoc.insertPage(-1, dstDoc.addObject(dstPage))
}

function copyAllPages(dstDoc, srcDoc) {
	var k, n = srcDoc.countPages()
	var srcMap = []
	for (k = 0; k < n; ++k)
		copyPage(dstDoc, srcDoc, k, srcMap)
}

function pdfmerge() {
	var srcDoc, dstDoc, i

	dstDoc = new PDFDocument()
	for (i = 1; i < scriptArgs.length; ++i) {
		srcDoc = new PDFDocument(scriptArgs[i])
		copyAllPages(dstDoc, srcDoc)
	}
	dstDoc.save(scriptArgs[0], "compress")
}

if (scriptArgs.length < 2)
	print("usage: mutool run pdf-merge.js output.pdf input1.pdf input2.pdf ...")
else
	pdfmerge()
