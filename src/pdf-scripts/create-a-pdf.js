// Adapted from MuPDF documentation; arguments: input, output, optional extras.
var doc = new PDFDocument();
doc.insertPage(-1, doc.addPage([0, 0, 595, 842], 0, null, ""));
doc.save(scriptArgs[1]);
