// Copyright (C) 2004-2021 Artifex Software, Inc.
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

let mupdf = {};

(function () {
	let worker = new Worker("mupdf-worker.js");

	worker.onmessage = function (event) {
		worker.promises = {};
		worker.promiseId = 0;
		worker.onmessage = function (event) {
			let [ type, id, result ] = event.data;
			if (type === "RESULT")
				worker.promises[id].resolve(result);
			else
				worker.promises[id].reject(result);
			delete worker.promises[id];
		}
		mupdf.oninit();
	}

	function wrap(func) {
		return function(...args) {
			return new Promise(function (resolve, reject) {
				let id = worker.promiseId++;
				worker.promises[id] = { resolve: resolve, reject: reject };
				if (args[0] instanceof ArrayBuffer)
					worker.postMessage([func, args, id], [args[0]]);
				else
					worker.postMessage([func, args, id]);
			});
		}
	}

	mupdf.openDocument = wrap("openDocument");
	mupdf.freeDocument = wrap("freeDocument");
	mupdf.documentTitle = wrap("documentTitle");
	mupdf.documentOutline = wrap("documentOutline");
	mupdf.countPages = wrap("countPages");
	mupdf.pageSizes = wrap("pageSizes");
	mupdf.pageWidth = wrap("pageWidth");
	mupdf.pageHeight = wrap("pageHeight");
	mupdf.pageLinks = wrap("pageLinks");
	mupdf.pageText = wrap("pageText");
	mupdf.search = wrap("search");
	mupdf.drawPageAsPNG = wrap("drawPageAsPNG");
	mupdf.terminate = function () { worker.terminate(); }
})();
