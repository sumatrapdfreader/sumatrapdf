# Building MuPDF.js from source

The WebAssembly build has only been tested on Linux at the moment. If you use
any other platform, you are on your own.

In order to build this you will need to install the Emscripten SDK in
/opt/emsdk. If you install it elsewhere, you will need to edit the build.sh
script to point to the appropriate location.

        https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html

From the mupdf platform/wasm directory, you can run build.sh to build the WebAssembly library.
The results of the build are the files lib/mupdf-wasm.wasm and lib/mupdf-wasm.js.
These files are not intended for direct use, but only to be used via lib/mupdf.js which provides the MuPDF.js module.

The mupdf-wasm.wasm binary is quite large, because it contains not only the
MuPDF library code, but also the 14 core PDF fonts, various CJK mapping
resources, and ICC profiles.

In order to keep it as small as possible, it is built with a minimal feature set
that excludes CJK fonts, PDF scripting, XPS format, and EPUB format support.

In lib/mupdf.js is a module that provides a usable Javascript API on top of
this WASM binary. This library works both in "node" and in browsers.
