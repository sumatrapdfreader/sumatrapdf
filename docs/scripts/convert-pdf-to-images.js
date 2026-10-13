// Info: Render every page as a PNG at 96 DPI.
// Arg: Input PDF
// Arg: Output filename prefix (folder must exist)

var doc = Document.openDocument(scriptArgs[0]);
for (var i = 0; i < doc.countPages(); ++i) {
  var page = doc.loadPage(i);
  var pixmap = page.toPixmap(Matrix.scale(96 / 72, 96 / 72), ColorSpace.DeviceRGB);
  pixmap.saveAsPNG(scriptArgs[1] + "-" + (i + 1) + ".png");
}
