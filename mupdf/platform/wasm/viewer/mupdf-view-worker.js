// Copyright (C) 2004-2022 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

/* global mupdf */

"use strict"

// Import the WASM module.
globalThis.__filename = "../lib/mupdf-wasm.js"
importScripts("../lib/mupdf-wasm.js")

// Import the MuPDF bindings.
importScripts("../lib/mupdf.js")

const workerMethods = {}

onmessage = async function (event) {
	let [ func, id, args ] = event.data
	try {
		let result = workerMethods[func](...args)
		postMessage([ "RESULT", id, result ])
	} catch (error) {
		if (error instanceof mupdf.TryLaterError) {
			trylaterQueue.push(event)
		} else {
			postMessage([ "ERROR", id, { name: error.name, message: error.message, stack: error.stack } ])
		}
	}
}

let trylaterScheduled = false
let trylaterQueue = []
mupdf.onFetchCompleted = function (_id) {
	if (!trylaterScheduled) {
		trylaterScheduled = true
		setTimeout(() => {
			trylaterScheduled = false
			let currentQueue = trylaterQueue
			trylaterQueue = []
			currentQueue.forEach(onmessage)
		}, 0)
	}
}

let openStream = null
let openDocument = null

workerMethods.setLogFilters = function (filters) {
	logFilters = filters
}

workerMethods.openStreamFromUrl = function (url, contentLength, progressive, prefetch) {
	openStream = new mupdf.Stream(url, contentLength, Math.max(progressive << 10, 1 << 16), prefetch)
}

workerMethods.openDocumentFromBuffer = function (buffer, magic) {
	openDocument = mupdf.Document.openDocument(buffer, magic)
}

workerMethods.openDocumentFromStream = function (magic) {
	if (openStream == null) {
		throw new Error("openDocumentFromStream called but no stream has been open")
	}
	openDocument = mupdf.Document.openDocument(openStream, magic)
}

workerMethods.freeDocument = function () {
	openDocument?.destroy()
	openDocument = null
}

workerMethods.documentTitle = function () {
	return openDocument.getMetaData(Document.META_INFO_TITLE)
}

workerMethods.documentOutline = function () {
	return openDocument.loadOutline()
}

workerMethods.countPages = function () {
	return openDocument.countPages()
}

// TODO - use hungarian notation for coord spaces
// TODO - document the "- 1" better
// TODO - keep page loaded?
workerMethods.getPageSize = function (pageNumber) {
	let page = openDocument.loadPage(pageNumber - 1)
	let bounds = page.getBounds()
	return { width: bounds[2] - bounds[0], height: bounds[3] - bounds[1] }
}

workerMethods.getPageLinks = function (pageNumber) {
	let page = openDocument.loadPage(pageNumber - 1)
	let links = page.getLinks()

	return links.map((link) => {
		const [ x0, y0, x1, y1 ] = link.getBounds()

		let href
		if (link.isExternal()) {
			href = link.getURI()
		} else {
			const linkPageNumber = openDocument.resolveLink(link)
			// TODO - move to front-end
			// TODO - document the "+ 1" better
			href = `#page${linkPageNumber + 1}`
		}

		return {
			x: x0,
			y: y0,
			w: x1 - x0,
			h: y1 - y0,
			href,
		}
	})
}

workerMethods.getPageText = function (pageNumber) {
	let page = openDocument.loadPage(pageNumber - 1)
	let text = page.toStructuredText(1).asJSON()
	return JSON.parse(text)
}

workerMethods.search = function (pageNumber, needle) {
	let page = openDocument.loadPage(pageNumber - 1)
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

workerMethods.getPageAnnotations = function (pageNumber, dpi) {
	let page = openDocument.loadPage(pageNumber - 1)

	if (page == null) {
		return []
	}

	const annotations = page.getAnnotations()
	const doc_to_screen = [ dpi = 72, 0, 0, dpi / 72, 0, 0 ]

	return annotations.map((annotation) => {
		const [ x0, y0, x1, y1 ] = Matrix.transformRect(annotation.getBounds())
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

// TODO - Move this to mupdf-view
const lastPageRender = new Map()

workerMethods.drawPageAsPNG = function (pageNumber, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72)

	// TODO - use canvas?

	let page = openDocument.loadPage(pageNumber - 1)
	let pixmap = page.toPixmap(doc_to_screen, mupdf.DeviceRGB, false)

	let png = pixmap?.saveAsPNG()

	pixmap?.destroy()

	return png
}

workerMethods.drawPageAsPixmap = function (pageNumber, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72)

	let page = openDocument.loadPage(pageNumber - 1)
	let bbox = Rect.transform(page.getBounds(), doc_to_screen)
	let pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, bbox, true)
	pixmap.clear(255)

	let device = new mupdf.DrawDevice(doc_to_screen, pixmap)
	page.run(device, Matrix.identity)
	device.close()

	let pixArray = pixmap.getPixels()
	let pixW = pixmap.getWidth()
	let pixH = pixmap.getHeight()

	let imageData = new ImageData(pixArray.slice(), pixW, pixH)

	pixmap.destroy()

	return imageData
}

postMessage([ "READY", Object.keys(workerMethods) ])
