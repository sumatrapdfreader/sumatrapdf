# MuPDF.js

Welcome to the official MuPDF.js library from [Artifex](https://artifex.com).

Use [MuPDF](https://mupdf.com) in your JavaScript and TypeScript projects!

This library is powered by WebAssembly and can be used in all the usual
JavaScript environments: Node, Bun, Firefox, Safari, Chrome, etc.

## License

MuPDF.js is available under Open Source
[AGPL](https://www.gnu.org/licenses/agpl-3.0.html) and commercial license
agreements.

> If you cannot meet the requirements of the AGPL, please contact
> [Artifex](https://artifex.com/contact/mupdf-inquiry.php) regarding a
> commercial license.

## Installation

	npm install mupdf

## Usage

The module is only available as an ESM module!

	import mupdf from "mupdf"

	var doc = mupdf.Document.openDocument("test.pdf")
	console.log(doc.countPages())

Check out the [example projects](examples/) to help you get started.

## Documentation

- [MuPDF.js Reference](https://mupdf.readthedocs.io/en/latest/reference/javascript/)
- [Getting Started & Examples](https://mupdfjs.readthedocs.io/en/latest/)

## Contact

Join the Discord at [#mupdf.js](https://discord.gg/zpyAHM7XtF) to chat with the
developers directly.
