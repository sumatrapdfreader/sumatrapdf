"use strict"

import * as mupdf from "mupdf"

async function fetch_and_open_document(url) {
	var response = await fetch(url)
	if (!response.ok)
		throw new Error(response.status + " " + response.statusText)
	var data = await response.arrayBuffer()
	return mupdf.Document.openDocument(data, url)
}

if (process.argv.length < 3) {
	console.error("usage: node examples/streams/fetch.js http://mupdf.com/docs/mupdf_explored.pdf")
} else {
	for (var url of process.argv.slice(2)) {
		var doc = await fetch_and_open_document(url)
		console.log(url + " has " + doc.countPages() + " pages.")
	}
}
