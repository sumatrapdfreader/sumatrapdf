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

"use strict";

// Import the WASM module
importScripts("libmupdf.js");

let fetcher = new Worker("mupdf-fetcher.js");

let mupdf = {};
let ready = false;

let wasm_onFetchData = null;
let wasm_openDocumentFromBuffer = null;

Module.onRuntimeInitialized = function () {
	Module.ccall('initContext');

	wasm_onFetchData = Module.cwrap('onFetchData', 'null', ['number', 'number', 'number', 'number']);
	wasm_openDocumentFromBuffer = Module.cwrap('openDocumentFromBuffer', 'number', ['number', 'number', 'string']);

	mupdf.openURL = Module.cwrap('openURL', 'number', ['string', 'number', 'number']);
	mupdf.openDocumentFromStream = Module.cwrap('openDocumentFromStream', 'number', ['number', 'string']);
	mupdf.freeDocument = Module.cwrap('freeDocument', 'null', ['number']);
	mupdf.documentTitle = Module.cwrap('documentTitle', 'string', ['number']);
	mupdf.countPages = Module.cwrap('countPages', 'number', ['number']);
	mupdf.pageWidth = Module.cwrap('pageWidth', 'number', ['number', 'number', 'number']);
	mupdf.pageHeight = Module.cwrap('pageHeight', 'number', ['number', 'number', 'number']);
	mupdf.pageLinksJSON = Module.cwrap('pageLinks', 'string', ['number', 'number', 'number']);
	mupdf.doDrawPageAsPNG = Module.cwrap('doDrawPageAsPNG', 'null', ['number', 'number', 'number']);
	mupdf.getLastDrawData = Module.cwrap('getLastDrawData', 'number', []);
	mupdf.getLastDrawSize = Module.cwrap('getLastDrawSize', 'number', []);
	mupdf.pageTextJSON = Module.cwrap('pageText', 'string', ['number', 'number', 'number']);
	mupdf.searchJSON = Module.cwrap('search', 'string', ['number', 'number', 'number', 'string']);
	mupdf.loadOutline = Module.cwrap('loadOutline', 'number', ['number']);
	mupdf.freeOutline = Module.cwrap('freeOutline', null, ['number']);
	mupdf.outlineTitle = Module.cwrap('outlineTitle', 'string', ['number']);
	mupdf.outlinePage = Module.cwrap('outlinePage', 'number', ['number', 'number']);
	mupdf.outlineDown = Module.cwrap('outlineDown', 'number', ['number']);
	mupdf.outlineNext = Module.cwrap('outlineNext', 'number', ['number']);
	postMessage("READY");
	ready = true;
};

mupdf.openDocumentFromBuffer = function (data, magic) {
	let n = data.byteLength;
	let ptr = Module._malloc(n);
	let src = new Uint8Array(data);
	Module.HEAPU8.set(src, ptr);
	return wasm_openDocumentFromBuffer(ptr, n, magic);
}

function onFetchData(id, block, data) {
	let n = data.byteLength;
	let p = Module._malloc(n);
	Module.HEAPU8.set(new Uint8Array(data), p);
	wasm_onFetchData(id, block, p, n);
	Module._free(p);
}

mupdf.drawPageAsPNG = function (doc, page, dpi) {
	mupdf.doDrawPageAsPNG(doc, page, dpi);
	let n = mupdf.getLastDrawSize();
	let p = mupdf.getLastDrawData();
	return Module.HEAPU8.buffer.slice(p, p+n);
}

mupdf.documentOutline = function (doc) {
	function makeOutline(node) {
		let list = [];
		while (node) {
			let entry = {
				title: mupdf.outlineTitle(node),
				page: mupdf.outlinePage(doc, node),
			}
			let down = mupdf.outlineDown(node);
			if (down)
				entry.down = makeOutline(down);
			list.push(entry);
			node = mupdf.outlineNext(node);
		}
		return list;
	}
	let root = mupdf.loadOutline(doc);
	if (root) {
		let list = null;
		try {
			list = makeOutline(root);
		} finally {
			mupdf.freeOutline(root);
		}
		return list;
	}
	return null;
}

mupdf.pageSizes = function (doc, dpi) {
	let list = [];
	let n = mupdf.countPages(doc);
	for (let i = 1; i <= n; ++i) {
		let w = mupdf.pageWidth(doc, i, dpi);
		let h = mupdf.pageHeight(doc, i, dpi);
		list[i] = [w, h];
	}
	return list;
}

mupdf.pageLinks = function (doc, page, dpi) {
	return JSON.parse(mupdf.pageLinksJSON(doc, page, dpi));
}

mupdf.pageText = function (doc, page, dpi) {
	return JSON.parse(mupdf.pageTextJSON(doc, page, dpi));
}

mupdf.search = function (doc, page, dpi, needle) {
	return JSON.parse(mupdf.searchJSON(doc, page, dpi, needle));
}

let trylater_timer = 0;
let trylater_queue = [];

function trylater_progress() {
	trylater_timer = 0;
	let current_queue = trylater_queue;
	trylater_queue = [];
	current_queue.forEach(onmessage);
}

fetcher.onmessage = function (event) {
	let [ cmd, id, block, data ] = event.data;
	if (cmd === 'DATA') {
		onFetchData(id, block, data);
		if (!trylater_timer)
			trylater_timer = setTimeout(trylater_progress, 0);
	}
	if (cmd === 'ERROR') {
		console.log("FETCH ERROR", data);
	}
}

onmessage = function (event) {
	let [ func, args, id ] = event.data;
	if (!ready) {
		postMessage(["ERROR", id, {name: "NotReadyError", message: "WASM module is not ready yet"}]);
		return;
	}
	try {
		let result = mupdf[func](...args);
		if (result instanceof ArrayBuffer)
			postMessage(["RESULT", id, result], [result]);
		else
			postMessage(["RESULT", id, result]);
	} catch (error) {
		if (error === "trylater") {
			trylater_queue.push(event);
		} else {
			postMessage(["ERROR", id, {name: error.name, message: error.message}]);
		}
	}
}
