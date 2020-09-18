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
