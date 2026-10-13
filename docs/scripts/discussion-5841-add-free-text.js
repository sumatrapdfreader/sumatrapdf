// Discussion #5841: https://github.com/sumatrapdfreader/sumatrapdf/discussions/5841
// Info: Add a free-text annotation to a page.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Page number (default 1)
// Arg: Rectangle x0,y0,x1,y1
// Arg: Text
// Arg: Font size (default 12)

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
var page = doc.loadPage(pageIndex(scriptArgs[2]));
var r = rectangle(scriptArgs[3]),
  content = scriptArgs[4] || "Hello",
  size = number(scriptArgs[5], 12);
if (size <= 0) throw new Error("Font size must be positive.");
var a = page.createAnnotation("FreeText");
a.setRect(r);
a.setContents(content);
a.setDefaultAppearance("Helv", size, [0, 0, 0]);
a.setBorderWidth(0);
page.update();
save();
