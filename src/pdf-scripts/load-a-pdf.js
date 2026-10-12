// Adapted from MuPDF documentation; arguments: input, output, optional extras.
var doc = Document.openDocument(scriptArgs[0]);
print(doc.countPages());
