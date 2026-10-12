// Adapted from MuPDF documentation; arguments: input, output, optional extras.
var doc = Document.openDocument(scriptArgs[0]);
for (var i = 0; i < doc.countPages(); ++i) {
  var page = doc.loadPage(i);
  var pixmap = page.toPixmap(Matrix.scale(96 / 72, 96 / 72), ColorSpace.DeviceRGB);
  pixmap.saveAsPNG(scriptArgs[1] + "-" + (i + 1) + ".png");
}
