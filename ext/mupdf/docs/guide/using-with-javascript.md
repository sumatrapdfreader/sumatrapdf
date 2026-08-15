# Using with Javascript

MuPDF can be used with Javascript in two ways:

* With the MuPDF.js module for Javascript.
* With the mutool run command line interpreter.

## MuPDF.js

We've created a portable WebAssembly build of MuPDF that works with all modern
Javascript environments such as Node, Bun, Firefox, Chrome, etc. This module
provides an object oriented interface to the core library, similar to the Java
library. There are also TypeScript definition files.

The easy way to get started using this is by using Node.js and install
the latest release from NPM:

	npm install mupdf

> Note that the mupdf module is only usable with ESM imports!

Then run the following script to verify that everything works by listing
all the public classes and functions in the mupdf module:

	import * as mupdf from mupdf
	console.log(Object.keys(mupdf))

Once that is working, try out some of the
<a href="../cookbook/javascript/index.html">Javascript examples</a>!

See the [mupdfjs.readthedocs.io](https://mupdfjs.readthedocs.io/) site for more extensive
examples of using MuPDF.js in modern Javascript frameworks.

## mutool run

The mutool command line has a built-in ES5 interpreter that can run scripts
with the same high level API as MuPDF.js (with some minor differences) without
needing to install Node or Bun.

The main limitation is that you can only use ES5 language features (so no
"let", arrow functions, or "class" syntactic sugar).

## How to build MuPDF.js

See the README and BUILDING files in the `platform/wasm` directory of the MuPDF source.
