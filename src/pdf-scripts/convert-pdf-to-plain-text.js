// Adapted from MuPDF documentation; arguments: input, output, optional extras.
var doc = Document.openDocument(scriptArgs[0]);
for (var i = 0; i < doc.countPages(); ++i) {
  var page = doc.loadPage(i);
  var text = page.toStructuredText().asText();
  print(text);
}
