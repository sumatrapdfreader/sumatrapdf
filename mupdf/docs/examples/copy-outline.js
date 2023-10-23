// Copy an outline from one document to another PDF file.
// Can be used to transfer outlines when converting from another format into PDF.
// Can also be used to change outlines to use page numbers instead of named destinations.

if (scriptArgs.length != 3) {
	print("usage: mutool run copy-outline.js a.xps b.pdf output.pdf");
	quit();
}

function copy_outline_rec(cursor, input, list) {
	list.forEach(function (node) {
		var page = input.resolveLink(node.uri)
		cursor.insert({ title: node.title, uri: "#page=" + (page + 1) })
		if (node.down) {
			cursor.prev()
			cursor.down()
			copy_outline_rec(cursor, input, node.down)
			cursor.up()
			cursor.next()
		}
	})
}

function copy_outline(output, input, list) {
	var cursor = output.outlineIterator()
	while (cursor.item())
		cursor.delete()
	copy_outline_rec(cursor, input, list)
}

var input = mupdf.Document.openDocument(scriptArgs[0])
var output = mupdf.Document.openDocument(scriptArgs[1])

copy_outline(output, input, input.loadOutline())

output.save(scriptArgs[2], "incremental");
