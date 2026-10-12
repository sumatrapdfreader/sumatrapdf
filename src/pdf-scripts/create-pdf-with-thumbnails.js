// Adapted from MuPDF documentation; arguments: input, output, optional extras.
// Create a PDF containing thumbnails of pages rendered from another PDF.

var pdf = new PDFDocument();

var subdoc = Document.openDocument(scriptArgs[0]);

var resources = { XObject: {} };

var contents = new Buffer();
for (var i = 0; i < Math.min(5, subdoc.countPages()); ++i) {
  var pixmap = subdoc.loadPage(i).toPixmap([0.2, 0, 0, 0.2, 0, 0], ColorSpace.DeviceRGB, true);
  resources.XObject["Im" + i] = pdf.addImage(new Image(pixmap));
  contents.writeLine("q 100 0 0 150 " + (50 + 100 * i) + " 50 cm /Im" + i + " Do Q");
}

var page = pdf.addPage([0, 0, 100 + i * 100, 250], 0, resources, contents);
pdf.insertPage(-1, page);

pdf.save(scriptArgs[1]);
