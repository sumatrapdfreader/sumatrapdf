// usage: mutool run pdf-bake.js input.pdf output.pdf
//
// This tool will rewrite all Widgets and Annotations as plain XObjects
// drawn on the page and remove them as interactive objects.
//
// Links will be preserved. All other Annotations will be removed.
// The AcroForm object will be deleted if present.
//

"use strict"

function request_synthesis(list) {
	if (list)
		for (var i = 0; i < list.length; ++i)
			list[i].requestSynthesis()
}

function bake_document(doc, do_bake_annots, do_bake_widgets) {
	var i, n, page, list
	n = doc.countPages()
	for (i = 0; i < n; ++i) {
		page = doc.loadPage(i)
		if (do_bake_annots)
			request_synthesis(page.getAnnotations())
		if (do_bake_widgets)
			request_synthesis(page.getWidgets())
		page.update()
		bake_page(doc, page.getObject(), do_bake_annots, do_bake_widgets)
	}
	if (do_bake_widgets)
		delete doc.getTrailer().Root.AcroForm
}

function bake_page(doc, page, do_bake_annots, do_bake_widgets) {
	var i, n, list, keep, buf
	list = page.Annots
	if (list) {
		if (!page.Resources)
			page.Resources = {}
		if (!page.Resources.XObject)
			page.Resources.XObject = {}
		if (!page.Contents.isArray())
			page.Contents = [ page.Contents ]

		keep = []
		buf = ""
		for (i = 0; i < list.length; ++i) {
			if (list[i].Subtype == "Link") {
				keep.push(list[i])
			} else if (list[i].Subtype == "Widget") {
				if (do_bake_widgets)
					buf += bake_annot(doc, page, list[i])
				else
					keep.push(list[i])
			} else {
				if (do_bake_annots)
					buf += bake_annot(doc, page, list[i])
				else
					keep.push(list[i])
			}
		}

		if (keep.length > 0)
			page.Annots = keep
		else
			delete page.Annots

		page.Contents.push(doc.addStream(buf))
	}
}

function get_annot_ap(annot) {
	var ap = annot.AP
	if (ap) {
		ap = ap.N
		if (ap.isStream())
			return ap
		ap = ap[annot.AS]
		if (ap.isStream())
			return ap
	}
	return null
}

function get_annot_transform(rect, bbox, transform) {
	var w, h, x, y
	if (!transform)
		transform = [ 1, 0, 0, 1, 0, 0 ]
	bbox = mupdf.Rect.transform(bbox, transform)
	w = (rect[2] - rect[0]) / (bbox[2] - bbox[0])
	h = (rect[3] - rect[1]) / (bbox[3] - bbox[1])
	x = rect[0] - bbox[0] * w
	y = rect[1] - bbox[1] * h
	return [ w, 0, 0, h, x, y ]
}

function bake_annot(doc, page, annot) {
	var name = "Annot" + annot.asIndirect()
	var ap = get_annot_ap(annot)
	if (ap) {
		var transform = get_annot_transform(annot.Rect, ap.BBox, ap.Matrix)
		page.Resources.XObject[name] = ap
		ap.Type = "XObject"
		ap.Subtype = "Form"
		return "q\n" + transform.join(" ") + " cm\n/" + name + " Do\nQ\n"
	}
	return ""
}

var doc = mupdf.Document.openDocument(scriptArgs[0])
doc.enableJournal()
doc.beginOperation("bake")
bake_document(doc, true, true)
doc.endOperation()
doc.save(scriptArgs[1], "garbage,compress")
