// Adapted from MuPDF documentation; arguments: input, output, optional extras.
var doc = Document.openDocument(scriptArgs[0]);

var page = doc.loadPage(0);

var annot = page.createAnnotation("FreeText");
annot.setRect([10, 10, 200, 100]);
annot.setContents("Hello, world!");

page.update();

doc.save(scriptArgs[1]);
