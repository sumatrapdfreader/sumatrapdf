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

/*global mupdf */

"use strict";

// Import the WASM module
importScripts("mupdf-wasm.js");
importScripts("lib/mupdf.js");

mupdf.ready.then(result => {
	postMessage(["READY", result.sharedBuffer]);
});

onmessage = async function (event) {
	let [ func, id, args ] = event.data;
	await mupdf.ready;

	try {
		if (func == "drawPageAsPNG") {
			drawPageAsPNG(id, ...args);
			return;
		}

		let result = workerMethods[func](...args);
		postMessage(["RESULT", id, result]);
	} catch (error) {
		if (error instanceof mupdf.MupdfTryLaterError) {
			trylaterQueue.push(event);
		} else {
			postMessage(["ERROR", id, {name: error.name, message: error.message, stack: error.stack}]);
		}
	}
};

let trylaterScheduled = false;
let trylaterQueue = [];
mupdf.onFetchCompleted = function (_id) {
	if (!trylaterScheduled) {
		trylaterScheduled = true;

		setTimeout(() => {
			trylaterScheduled = false;
			let currentQueue = trylaterQueue;
			trylaterQueue = [];
			currentQueue.forEach(onmessage);
		}, 0);
	}
};

const workerMethods = {};

let openStream = null;
let openDocument = null;

workerMethods.openStreamFromUrl = function (url, contentLength, progressive, prefetch) {
	openStream = mupdf.Stream.fromUrl(url, contentLength, Math.max(progressive << 10, 1 << 16), prefetch);
	// TODO - close stream?
};

workerMethods.openDocumentFromBuffer = function (buffer, magic) {
	openDocument = mupdf.Document.openFromJsBuffer(buffer, magic);
};

workerMethods.openDocumentFromStream = function (magic) {
	if (openStream == null) {
		throw new Error("openDocumentFromStream called but no stream has been open");
	}
	openDocument = mupdf.Document.openFromStream(openStream, magic);
};

workerMethods.freeDocument = function () {
	openDocument?.free();
	openDocument = null;
};

workerMethods.documentTitle = function () {
	return openDocument.title();
};

workerMethods.documentOutline = function () {
	const root = openDocument.loadOutline();

	if (root == null)
		return null;

	function makeOutline(node) {
		let list = [];
		while (node) {
			let entry = {
				title: node.title(),
				page: node.pageNumber(openDocument),
			};
			let down = node.down();
			if (down)
				entry.down = makeOutline(down);
			list.push(entry);
			node = node.next();
		}
		return list;
	}

	try {
		return makeOutline(root);
	} finally {
		root.free();
	}
};

workerMethods.countPages = function() {
	return openDocument.countPages();
};

// TODO - use hungarian notation for coord spaces
// TODO - currently this loads every single page. Not very efficient?
workerMethods.getPageSizes = function (dpi) {
	let list = [];
	let n = openDocument.countPages();
	for (let i = 0; i < n; ++i) {
		let page;
		try {
			page = openDocument.loadPage(i);
			let width = page.width() * dpi / 72;
			let height = page.height() * dpi / 72;
			list.push({width, height});
		}
		finally {
			page.free();
		}
	}
	return list;
};

// TODO - document the "- 1" better
// TODO - keep page loaded?
workerMethods.getPageWidth = function (pageNumber, dpi) {
	let page = openDocument.loadPage(pageNumber - 1);
	return page.width() * dpi / 72;
};

workerMethods.getPageHeight = function (pageNumber, dpi) {
	let page = openDocument.loadPage(pageNumber - 1);
	return page.height() * dpi / 72;
};

workerMethods.getPageLinks = function(pageNumber, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);
	let page;
	let links_ptr;

	try {
		page = openDocument.loadPage(pageNumber - 1);
		links_ptr = page.loadLinks();

		return links_ptr.links.map(link => {
			const { x0, y0, x1, y1 } = doc_to_screen.transformRect(link.rect());

			let href;
			if (link.isExternalLink()) {
				href = link.uri();
			} else {
				const linkPageNumber = link.resolve(openDocument).pageNumber(openDocument);
				// TODO - document the "+ 1" better
				href = `#page${linkPageNumber + 1}`;
			}

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
				href
			};
		});
	}
	finally {
		page?.free();
		links_ptr?.free();
	}
};

workerMethods.getPageText = function(pageNumber, dpi) {
	let page;
	let stextPage;

	let buffer;
	let output;

	try {
		page = openDocument.loadPage(pageNumber - 1);
		stextPage = page.toSTextPage();

		buffer = mupdf.Buffer.empty();
		output = mupdf.Output.withBuffer(buffer);

		stextPage.printAsJson(output, dpi / 72);
		output.close();

		let text = buffer.toJsString();
		return JSON.parse(text);
	}
	finally {
		output?.free();
		buffer?.free();
		stextPage?.free();
		page?.free();
	}
};

workerMethods.search = function(pageNumber, dpi, needle) {
	let page;

	try {
		page = openDocument.loadPage(pageNumber - 1);
		const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);
		const hits = page.search(needle);
		return hits.map(searchHit => {
			const  { x0, y0, x1, y1 } = doc_to_screen.transformRect(searchHit);

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
			};
		});
	}
	finally {
		page?.free();
	}
};

workerMethods.getPageAnnotations = function(pageNumber, dpi) {
	let pdfPage;

	try {
		pdfPage = openDocument.loadPage(pageNumber - 1);

		if (pdfPage == null) {
			return [];
		}

		const annotations = pdfPage.annotations();
		const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);

		return annotations.annotations.map(annotation => {
			const { x0, y0, x1, y1 } = doc_to_screen.transformRect(annotation.bounds());

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
				type: annotation.annotType(),
				ref: annotation.pointer,
			};
		});
	}
	finally {
		pdfPage?.free();
	}
};

let currentTool = null;
let currentSelection = null;

let jobCookies = {};

// TODO - Use mupdf instead
const lastPageRender = new Map();
function drawPageAsPNG(id, pageNumber, dpi) {
	if (lastPageRender.has(pageNumber) && lastPageRender.get(pageNumber).dpi === dpi) {
		postMessage(["RESULT", id, null]);
		return;
	}

	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);

	let page;
	let pixmap;
	let cookie;

	// TODO - draw annotations
	// TODO - use canvas?

	try {
		cookie = mupdf.JobCookie.create();
		jobCookies[cookie.pointer] = cookie;
		postMessage(["RESULT", id, cookie?.pointer]);
		// TODO - on error

		page = openDocument.loadPage(pageNumber - 1);
		pixmap = page.toPixmap(doc_to_screen, mupdf.DeviceRGB, false, cookie);

		if (cookie.aborted()) {
			pixmap = null;
		}

		if (pageNumber == currentTool.pageNumber)
			currentTool.drawOnPage(pixmap, dpi);

		let png = pixmap?.toPNG();

		postMessage(["RENDER", id, { pageNumber, png, cookiePointer: cookie?.pointer }]);
		lastPageRender.set(pageNumber, { dpi });
	}
	finally {
		pixmap?.free();
		page?.free();
	}
}


workerMethods.deleteCookie = function(cookiePointer) {
	delete jobCookies[cookiePointer];
};

workerMethods.resetPageCache = function() {
	lastPageRender.clear();
};

workerMethods.mouseDownOnPage = function(pageNumber, dpi, x, y) {
	// TODO - Do we want to do a load every time?
	let pdfPage = openDocument.loadPage(pageNumber - 1);

	if (pdfPage == null) {
		return;
	}

	if (pageNumber !== currentTool.pageNumber) {
		// TODO - schedule paint
		lastPageRender.delete(currentTool.pageNumber);
		currentTool.resetPage(pdfPage, pageNumber);
	}

	// transform mouse pos from screen coordinates to document coordinates.
	x = x / (dpi / 72);
	y = y / (dpi / 72);

	let pageChanged = currentTool.mouseDown(x, y);
	if (pageChanged) {
		lastPageRender.delete(pageNumber);
	}
	return pageChanged;

	// TODO - multi-selection
	// TODO - differentiate between hovered, selected, held

};

// TODO - handle crossing pages
workerMethods.mouseDragOnPage = function(pageNumber, dpi, x, y) {
	if (pageNumber !== currentTool.pageNumber)
		return false;

	// transform mouse pos from screen coordinates to document coordinates.
	x = x / (dpi / 72);
	y = y / (dpi / 72);

	let pageChanged = currentTool.mouseDrag(x, y);
	if (pageChanged) {
		lastPageRender.delete(pageNumber);
	}
	return pageChanged;
};

workerMethods.mouseMoveOnPage = function(pageNumber, dpi, x, y) {
	if (pageNumber !== currentTool.pageNumber)
		return false;

	let pdfPage = openDocument.loadPage(pageNumber - 1);
	if (pdfPage == null) {
		return;
	}

	// transform mouse pos from screen coordinates to document coordinates.
	x = x / (dpi / 72);
	y = y / (dpi / 72);

	let pageChanged = currentTool.mouseMove(x, y);
	if (pageChanged) {
		lastPageRender.delete(pageNumber);
	}
	return pageChanged;
};

workerMethods.mouseUpOnPage = function(pageNumber, dpi, x, y) {
	if (pageNumber !== currentTool.pageNumber)
		return false;

	// transform mouse pos from screen coordinates to document coordinates.
	x = x / (dpi / 72);
	y = y / (dpi / 72);

	let pageChanged = currentTool.mouseUp(x, y);
	if (pageChanged) {
		lastPageRender.delete(pageNumber);
	}
	return pageChanged;
};

workerMethods.deleteItem = function () {
	let pageChanged = currentTool.deleteItem();
	if (pageChanged) {
		lastPageRender.delete(currentTool.pageNumber);
	}
	return currentTool.pageNumber;
};

class SelectedAnnotation {
	constructor(annotation, mouse_x, mouse_y) {
		// Is this necessary?
		this.annotation = annotation;
		this.startRect = annotation.rect();
		this.currentRect = annotation.rect();
		this.initial_x = mouse_x;
		this.initial_y = mouse_y;
	}

	// TODO - remove
	mouseDrag(x, y) {
		this.currentRect = this.startRect.translated(x - this.initial_x, y - this.initial_y);
		this.annotation.rect();
		// TODO - setRect doesn't quite do what we want
		this.annotation.setRect(this.currentRect);
		return true;
	}

	// TODO - remove
	mouseUp(x, y) {
		this.currentRect = this.startRect.translated(x - this.initial_x, y - this.initial_y);
		// TODO - setRect doesn't quite do what we want
		this.annotation.setRect(this.currentRect);
		return true;
	}
}

function inSquare(squarePoint, x, y) {
	return (
		x >= squarePoint.x - 5 &&
		x < squarePoint.x + 5 &&
		y >= squarePoint.y - 5 &&
		y < squarePoint.y + 5
	);
}

function findAnnotationAtPos(pdfPage, x, y) {
	// using Array.findLast would be more elegant, but it isn't stable on
	// all major platforms
	let annotations = pdfPage.annotations().annotations;
	for (let i = annotations.length - 1; i >= 0; i--) {
		const annotation = annotations[i];
		const bbox = annotation.bound();
		if (x >= bbox.x0 && x <= bbox.x1 && y >= bbox.y0 && y <= bbox.y1) {
			return annotation;
		}
	}
	return null;
}

class SelectAnnot {
	constructor() {
		this.initial_x = null;
		this.initial_y = null;
		this.hovered = null;
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		currentSelection = null;
	}

	mouseDown(x, y) {
		const clickedAnnotation = findAnnotationAtPos(this.pdfPage, x, y);
		let selectionChanged = (currentSelection?.annotation !== clickedAnnotation);

		if (clickedAnnotation != null) {
			currentSelection = new SelectedAnnotation(
				clickedAnnotation,
				x, y
			);
			this.initial_x = x;
			this.initial_y = y;
		}
		else {
			currentSelection = null;
		}

		return selectionChanged;
	}

	mouseDrag(x, y) {
		if (currentSelection == null)
			return false;

		return currentSelection?.mouseDrag(x, y);
	}

	mouseMove(x, y) {
		let prevHovered = this.hovered;
		this.hovered = findAnnotationAtPos(this.pdfPage, x, y);
		return prevHovered?.pointer !== this.hovered?.pointer;
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		if (this.hovered != null) {
			let rect = this.hovered.rect();
			pixmap.drawGrabHandle(rect.x0 * dpi / 72, rect.y0 * dpi / 72);
			pixmap.drawGrabHandle(rect.x0 * dpi / 72, rect.y1 * dpi / 72);
			pixmap.drawGrabHandle(rect.x1 * dpi / 72, rect.y0 * dpi / 72);
			pixmap.drawGrabHandle(rect.x1 * dpi / 72, rect.y1 * dpi / 72);
		}
		if (currentSelection != null) {
			let rect = currentSelection.annotation.rect();
			pixmap.drawGrabHandle(rect.x0 * dpi / 72, rect.y0 * dpi / 72);
			pixmap.drawGrabHandle(rect.x0 * dpi / 72, rect.y1 * dpi / 72);
			pixmap.drawGrabHandle(rect.x1 * dpi / 72, rect.y0 * dpi / 72);
			pixmap.drawGrabHandle(rect.x1 * dpi / 72, rect.y1 * dpi / 72);
		}
	}

	deleteItem() {
		if (currentSelection?.annotation) {
			this.pdfPage.removeAnnot(currentSelection?.annotation);
			return true;
		}
	}
}

// TODO - DragSelection
// TODO - SelectionRect

class CreateText {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_TEXT);
		annot.setRect(new mupdf.Rect(x, y, x + 20, y + 20));
		//pdf_annot_icon_name
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

// TODO - CreateLink

class CreateFreeText {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_FREE_TEXT);
		annot.setRect(new mupdf.Rect(x, y, x + 200, y + 100));
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateLine {
	constructor() {
		this.points = [];
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		this.points = [];
	}

	mouseDown(x, y) {
		this.points.push(new mupdf.Point(x, y));

		if (this.points.length == 2) {
			let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_LINE);
			annot.setLine(this.points[0], this.points[1]);
			// pdf_set_annot_interior_color
			// pdf_set_annot_line_ending_styles
			this.pdfPage.update();
			return true;
		}

		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateSquare {
	constructor() {
		this.points = [];
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		this.points = [];
	}

	mouseDown(x, y) {
		this.points.push(new mupdf.Point(x, y));

		if (this.points.length == 2) {
			let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_SQUARE);
			annot.setRect(new mupdf.Rect(this.points[0].x, this.points[0].y, this.points[1].x, this.points[1].y));
			// pdf_set_annot_interior_color
			this.pdfPage.update();
			return true;
		}

		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateCircle {
	constructor() {
		this.points = [];
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		this.points = [];
	}

	mouseDown(x, y) {
		this.points.push(new mupdf.Point(x, y));

		if (this.points.length == 2) {
			let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_CIRCLE);
			annot.setRect(new mupdf.Rect(this.points[0].x, this.points[0].y, this.points[1].x, this.points[1].y));
			// pdf_set_annot_interior_color
			this.pdfPage.update();
			return true;
		}

		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreatePolygon {
	constructor() {
		this.points = [];
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		this.points = [];
	}

	mouseDown(x, y) {
		if (this.points[0] != null && inSquare(this.points[0], x, y)) {
			let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_POLYGON);
			for (const point of this.points) {
				annot.addVertex(point);
			}
			this.pdfPage.update();
			//pdf_annot_interior_color
			//pdf_annot_line_ending_styles
			return true;
		}

		this.points.push(new mupdf.Point(x, y));
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreatePolyLine {
	constructor() {
		this.points = [];
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
		this.points = [];
	}

	mouseDown(x, y) {
		if (this.points[0] != null && inSquare(this.points[0], x, y)) {
			let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_POLYLINE);
			for (const point of this.points) {
				annot.addVertex(point);
			}
			this.pdfPage.update();
			//pdf_annot_interior_color
			//pdf_annot_line_ending_styles
			return true;
		}

		this.points.push(new mupdf.Point(x, y));
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateStamp {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_STAMP);
		annot.setRect(new mupdf.Rect(x, y, x + 190, y + 50));
		//pdf_annot_icon_name
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateCaret {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_CARET);
		annot.setRect(new mupdf.Rect(x, y, x + 18, y + 15));
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}


class CreateFileAttachment {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_FILE_ATTACHMENT);
		annot.setRect(new mupdf.Rect(x, y, x + 20, y + 20));
		//pdf_annot_icon_name
		//pdf_annot_filespec
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

class CreateSound {
	constructor() {
		this.pdfPage = null;
		this.pageNumber = null;
	}

	resetPage(pdfPage, pageNumber) {
		this.pdfPage = pdfPage;
		this.pageNumber = pageNumber;
	}

	mouseDown(x, y) {
		let annot = this.pdfPage.createAnnot(mupdf.PDF_ANNOT_SOUND);
		annot.setRect(new mupdf.Rect(x, y, x + 20, y + 20));
		//pdf_annot_icon_name
		this.pdfPage.update();
		return true;
	}

	mouseDrag(_x, _y) {
		// move last point
	}

	mouseMove(_pdfPage, _x, _y) {
		// update hovered
	}

	mouseUp(_x, _y) {
		// do nothing
	}

	drawOnPage(pixmap, dpi) {
		// TODO - draw points on hover/select
		let points = this.points ?? [];
		for (let point of points) {
			pixmap.drawGrabHandle(point.x * dpi / 72, point.y * dpi / 72);
		}
	}

	deleteItem() {
		// TODO
	}
}

currentTool = new SelectAnnot();

const editionTools = {
	CreateText,
	CreateFreeText,
	CreateLine,
	CreateSquare,
	CreateCircle,
	CreatePolygon,
	CreatePolyLine,
	CreateStamp,
	CreateCaret,
	CreateFileAttachment,
	CreateSound,
};

workerMethods.setEditionTool = function(toolName) {
	if (toolName in editionTools) {
		currentTool = new (editionTools[toolName]);
		console.log("new tool:", toolName, " - ", currentTool);
	}
	else {
		console.warn("cannot find tool", toolName);
	}
};
