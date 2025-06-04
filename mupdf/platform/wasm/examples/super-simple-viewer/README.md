# Super Simple viewer

This is an extremely simple example of using the MuPDF.js module from a web browser.

The page rendering runs directly on the main thread, so this viewer may cause
your browser to slow down while it is loading the PDF document!

To start a local HTTP server and open your browser to the viewer:

	npx http-server ../.. -b -o examples/super-simple-viewer

Then click the "Browse..." button to load a local PDF file into the viewer.
The viewer will then render all the pages in the PDF and insert them as
images into the document.
