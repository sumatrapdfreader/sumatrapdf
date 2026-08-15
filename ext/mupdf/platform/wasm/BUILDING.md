# Building MuPDF.js from source

The WebAssembly build has only been tested on Linux at the moment.
If you use any other platform then you are on your own!

## Dependencies

This project has two dependencies that MUST be installed FIRST!

### Emscripten

You need to install the Emscripten SDK in `/opt/emsdk`. If you install it
elsewhere, you will need to edit `tools/build.sh` to point to the appropriate
location.

> https://emscripten.org/docs/getting_started/downloads.html

We have only tested against EMSDK version 4.0.8. Use another version at your own peril!

	/opt/emsdk/emsdk install 4.0.8
	/opt/emsdk/emsdk activate 4.0.8

### Node.js

Install Node.js and NPM. We use the current LTS version, which at the moment is v22.
The version that ships with Debian is probably too old to use.

> https://nodejs.org/en

If you don't want to install Node separately, you can piggy-back on the Node version
that ships with Emscripten:

	export PATH="/opt/emdsk/node/20.18.0_64bit/bin:$PATH"

## Building

You need to be in the `platform/wasm` directory for these build steps.

	cd platform/wasm

The following command will download and install all the NPM project dependencies.

	npm install

To compile the WebAssembly and Typescript files:

	npm run prepack

The results of the build are put into the `dist` directory:

- `dist/mupdf-wasm.wasm`
- `dist/mupdf-wasm.js`
- `dist/mupdf.js`

The `mupdf-wasm.wasm` file is quite large, because it contains not only the
MuPDF library code, but also the 14 core PDF fonts, various CJK mapping
resources, and ICC profiles.

In order to keep it as small as possible, it is built with a minimal feature set
that excludes the more refined CJK fonts, AcroForm scripting, and XPS input format.

## Installing and Running

The main module is the `mupdf.js` file.

### Use the MuPDF.js module in a browser

To use MuPDF.js directly in the browser, put the `dist/mupdf-wasm.wasm`,
`dist/mupdf-wasm.js`, and `dist/mupdf.js` somewhere on your site, and import
the `mupdf.js` module.

There's an example of using MuPDF.js in the browser with a WebWorker in `examples/simple-viewer`.

	npm run simple-viewer

### Use the MuPDF.js module in Node

You can `npm pack` and `npm install` the project in another directory.

You can also run the examples directly from here.

## Customizing

The build can be customized to enable a different feature set, by calling
the tools/build.sh script with the appropriate environment variables.

See the Makefile for some examples.

The most noteworthy of these is the "memento" configuration, which enables a
memory debugger.

	make memento

If you're seeing weird memory corruption issues, Memento is the best way to
track them down!

## Editing

The source files are:

- `lib/mupdf.c`
- `lib/mupdf.ts`
