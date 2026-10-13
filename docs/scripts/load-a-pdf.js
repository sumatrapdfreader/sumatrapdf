// Info: Print the page count of a PDF.
// Arg: Input PDF

var doc = Document.openDocument(scriptArgs[0]);
print(doc.countPages());
