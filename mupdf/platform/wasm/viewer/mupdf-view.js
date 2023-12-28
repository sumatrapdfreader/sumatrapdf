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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

"use strict"

var mupdfView = {}

const worker = new Worker("mupdf-view-worker.js")
const messagePromises = new Map()
let lastPromiseId = 0

mupdfView.ready = new Promise((resolve, reject) => {
	worker.onmessage = function (event) {
		let type = event.data[0]
		if (type === "READY") {
			let methodNames = event.data[1]
			for (let method of methodNames)
				mupdfView[method] = wrap(method)
			worker.onmessage = onWorkerMessage
			resolve()
		} else if (type === "ERROR") {
			let error = event.data[1]
			reject(new Error(error))
		} else {
			reject(new Error(`Unexpected first message: ${event.data}`))
		}
	}
})

function onWorkerMessage(event) {
	let [ type, id, result ] = event.data
	if (type === "RESULT")
		messagePromises.get(id).resolve(result)
	else if (type === "READY")
		messagePromises.get(id).reject(new Error("Unexpected READY message"))
	else if (type === "ERROR") {
		let error = new Error(result.message)
		error.name = result.name
		error.stack = result.stack
		messagePromises.get(id).reject(error)
	} else
		messagePromises.get(id).reject(new Error(`Unexpected result type '${type}'`))

	messagePromises.delete(id)
}

// TODO - Add cancelation for trylater queues
function wrap(func) {
	return function (...args) {
		return new Promise(function (resolve, reject) {
			let id = lastPromiseId++
			messagePromises.set(id, { resolve, reject })
			if (args[0] instanceof ArrayBuffer)
				worker.postMessage([ func, id, args ], [ args[0] ])
			else
				worker.postMessage([ func, id, args ])
		})
	}
}

mupdfView.setLogFilters = wrap("setLogFilters")

const wrap_openStreamFromUrl = wrap("openStreamFromUrl")
const wrap_openDocumentFromStream = wrap("openDocumentFromStream")

mupdfView.openDocumentFromUrl = async function (url, contentLength, progressive, prefetch, magic) {
	await wrap_openStreamFromUrl(url, contentLength, progressive, prefetch)
	return await wrap_openDocumentFromStream(magic)
}

mupdfView.terminate = function () {
	worker.terminate()
}
