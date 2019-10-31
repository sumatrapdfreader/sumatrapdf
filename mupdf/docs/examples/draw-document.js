// Draw all pages in a document and save them as PNG files.

var doc = new Document(scriptArgs[0]);
var n = doc.countPages();
for (var i = 0; i < n; ++i) {
	var page = doc.loadPage(i);
	var pixmap = page.toPixmap(Identity, DeviceRGB);
	pixmap.saveAsPNG("out" + (i+1) + ".png");
}
