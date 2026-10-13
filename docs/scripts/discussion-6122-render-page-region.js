// Discussion #6122: https://github.com/sumatrapdfreader/sumatrapdf/discussions/6122
// Info: Export a rectangular page region as a PNG image.
// Arg: Input PDF
// Arg: Output PNG file
// Arg: Page number (default 1)
// Arg: Rectangle x0,y0,x1,y1
// Arg: DPI (default 300)

var input = scriptArgs[0],
  output = scriptArgs[1];
if (!input || !output) throw new Error("Supply input PDF and output path.");
function pathKey(s) {
  return s.replace(/\\/g, "/").toLowerCase();
}
if (pathKey(input) === pathKey(output)) throw new Error("Choose a different output path.");
var doc = Document.openDocument(input);
if (doc.needsPassword()) throw new Error("Use a decrypted input PDF.");
function save() {
  doc.save(output, "garbage=4,compress");
  print("Saved " + output);
}
function number(s, fallback) {
  var n = s === undefined || s === "" ? fallback : Number(s);
  if (!isFinite(n)) throw new Error("Invalid number: " + s);
  return n;
}
function rectangle(s) {
  if (!s) throw new Error("Supply rectangle x0,y0,x1,y1 in page coordinates.");
  var parts = s.split(",");
  if (parts.length !== 4) throw new Error("Rectangle needs four numbers.");
  var r = parts.map(function (v) {
    return number(v);
  });
  if (r[0] >= r[2] || r[1] >= r[3]) throw new Error("Rectangle must have positive width and height.");
  return r;
}
function pageIndex(s) {
  var p = number(s, 1);
  if (p % 1 || p < 1 || p > doc.countPages()) throw new Error("Invalid page number.");
  return p - 1;
}
var kPointsPerInch = 72;
var page = doc.loadPage(pageIndex(scriptArgs[2])),
  r = rectangle(scriptArgs[3]),
  dpi = number(scriptArgs[4], 300);
if (dpi <= 0 || dpi % 1) throw new Error("DPI must be a positive integer.");
if (!/\.png$/i.test(output)) throw new Error("Choose a .png output.");
page.setPageBox("CropBox", r);
var pix = page.toPixmap(Matrix.scale(dpi / kPointsPerInch, dpi / kPointsPerInch), ColorSpace.DeviceRGB, false);
pix.setResolution(dpi, dpi);
pix.saveAsPNG(output);
print("Saved " + output);
