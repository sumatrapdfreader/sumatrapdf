# MuPDF.js

This is a build of MuPDF targeting WASM environments.

The MuPDF.js library can be used both in browsers and in Node.

This library is very similar in design and use to the MuPDF Java library.
The same classes and methods can be used in the same way -- but there are also a few
conveniences available here thanks to the dynamic nature of Javascript that are
not available in the Java API.

## Getting started using NPM

From the command line, go to the folder you want to work from and run:

	npm install mupdf

To verify your installation you can create a file "test.js" with the following script:

	const mupdf = require("mupdf")
	console.log(mupdf)

Then, on the command line, run:

	node test.js

If all is well, this will print the `mupdf` module object to the console.

## Loading a document

The following example demonstrates how to load a document and then print out the page count.
Ensure you have a "my_document.pdf" file alongside this example before trying it.

	const fs = require("fs")
	const mupdf = require("mupdf")
	var data = fs.readFileSync("my_document.pdf")
	var doc = mupdf.Document.openDocument(data, "application/pdf")
	console.log(doc.countPages())

## License

AGPLv3 or later. See https://www.mupdf.com/licensing/ for more details.

## Documentation

For documentation please refer to https://mupdf.readthedocs.io/.
