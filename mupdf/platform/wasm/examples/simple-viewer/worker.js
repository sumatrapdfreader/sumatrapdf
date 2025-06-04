// Copyright (C) 2022, 2024 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

"use strict"

import * as mupdf from "../../dist/mupdf.js"

const methods = {}

onmessage = async function (event) {
	let [ func, id, args ] = event.data
	try {
		let result = methods[func](...args)
		postMessage([ "RESULT", id, result ])
	} catch (error) {
		postMessage([ "ERROR", id, { name: error.name, message: error.message, stack: error.stack } ])
	}
}

var document_next_id = 1
var document_map = {} // open mupdf.Document handles

class WorkerBlobStream {
	constructor(blob) {
		this.reader = new FileReaderSync()
		this.blob = blob
	}
	fileSize() {
		return this.blob.size
	}
	read(memory, offset, size, position) {
		let data = this.reader.readAsArrayBuffer(this.blob.slice(position, position + size))
		memory.set(new Uint8Array(data), offset)
		return data.byteLength
	}
	close() {
		this.reader = null
		this.blob = null
	}
}

methods.openDocumentFromBlob = function (blob, magic) {
	let stm = new mupdf.Stream(new WorkerBlobStream(blob))
	let doc_id = document_next_id++
	document_map[doc_id] = mupdf.Document.openDocument(stm, magic)
	return doc_id
}

methods.openDocumentFromBuffer = function (buffer, magic) {
	let doc_id = document_next_id++
	document_map[doc_id] = mupdf.Document.openDocument(buffer, magic)
	return doc_id
}

methods.closeDocument = function (doc_id) {
	let doc = document_map[doc_id]
	doc.destroy()
	delete document_map[doc_id]
}

methods.documentTitle = function (doc_id) {
	let doc = document_map[doc_id]
	return doc.getMetaData(mupdf.Document.META_INFO_TITLE)
}

methods.documentOutline = function (doc_id) {
	let doc = document_map[doc_id]
	return doc.loadOutline()
}

methods.countPages = function (doc_id) {
	let doc = document_map[doc_id]
	return doc.countPages()
}

methods.getPageSize = function (doc_id, page_number) {
	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)
	let bounds = page.getBounds()
	return { width: bounds[2] - bounds[0], height: bounds[3] - bounds[1] }
}

methods.getPageLinks = function (doc_id, page_number) {
	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)
	let links = page.getLinks()

	return links.map((link) => {
		const [ x0, y0, x1, y1 ] = link.getBounds()

		let href
		if (link.isExternal())
			href = link.getURI()
		else
			href = `#page${doc.resolveLink(link) + 1}`

		return {
			x: x0,
			y: y0,
			w: x1 - x0,
			h: y1 - y0,
			href,
		}
	})
}

methods.getPageText = function (doc_id, page_number) {
	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)
	let text = page.toStructuredText().asJSON()
	return JSON.parse(text)
}

methods.search = function (doc_id, page_number, needle) {
	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)
	const hits = page.search(needle)
	let result = []
	for (let hit of hits) {
		for (let quad of hit) {
			const [ ulx, uly, urx, ury, llx, lly, lrx, lry ] = quad
			result.push({
				x: ulx,
				y: uly,
				w: urx - ulx,
				h: lly - uly,
			})
		}
	}
	return result
}

methods.getPageAnnotations = function (doc_id, page_number, dpi) {
	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)

	if (page == null) {
		return []
	}

	const annotations = page.getAnnotations()
	const doc_to_screen = [ dpi = 72, 0, 0, dpi / 72, 0, 0 ]

	return annotations.map((annotation) => {
		const [ x0, y0, x1, y1 ] = mupdf.Matrix.transformRect(annotation.getBounds())
		return {
			x: x0,
			y: y0,
			w: x1 - x0,
			h: y1 - y0,
			type: annotation.getType(),
			ref: annotation.pointer,
		}
	})
}

methods.drawPageAsPixmap = function (doc_id, page_number, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72)

	let doc = document_map[doc_id]
	let page = doc.loadPage(page_number)
	let bbox = mupdf.Rect.transform(page.getBounds(), doc_to_screen)

	let pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, bbox, true)
	pixmap.clear(255)

	let device = new mupdf.DrawDevice(doc_to_screen, pixmap)
	page.run(device, mupdf.Matrix.identity)
	device.close()

	// TODO: do we need to make a copy with slice() ?
	let imageData = new ImageData(pixmap.getPixels().slice(), pixmap.getWidth(), pixmap.getHeight())

	pixmap.destroy()

	// TODO: do we need to pass image data as transferable to avoid copying?
	return imageData
}

postMessage([ "INIT", 0, Object.keys(methods) ])
