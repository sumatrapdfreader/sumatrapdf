// usage: mutool run import-fdf.js input.pdf data.fdf output.pdf
//
// Import annotations from FDF into PDF file.
//
// TODO: import form data!

"use strict"

if (scriptArgs.length != 3) {
	print("usage: mutool run import-fdf.js file.pdf data.fdf output.pdf")
	quit()
}

var doc = mupdf.Document.openDocument(scriptArgs[0])
var data = mupdf.Document.openDocument(scriptArgs[1])
var map = doc.newGraftMap()

var annots = data.getTrailer().Root.FDF.Annots
if (annots) {
	annots.forEach(
		function (annot) {
			var ref = map.graftObject(annot)
			var page = doc.findPage(ref.Page)
			if (!page.Annots)
				page.Annots = []
			page.Annots.push(ref)
			ref.P = page
			ref.delete("Page")
		}
	)
}

doc.save(scriptArgs[2], "incremental")
