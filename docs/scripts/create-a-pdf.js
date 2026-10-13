// Info: Create a PDF with one blank page.
// Arg: Unused (leave empty)
// Arg: Output PDF

var doc = new PDFDocument();
doc.insertPage(-1, doc.addPage([0, 0, 595, 842], 0, null, ""));
doc.save(scriptArgs[1]);
