# Simple Viewer

This project contains a simple demo of a PDF viewer using the MuPDF.js Library.

This viewer uses a Worker background thread to do the heavy lifting, and
inserts rendered images and text in the DOM as pages scroll into view.

To start the HTTP server and open your browser to the viewer:

	npx http-server ../.. -b -o examples/simple-viewer

Use the File menu to open a PDF file.
