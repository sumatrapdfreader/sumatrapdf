// Discussion #2472: https://github.com/sumatrapdfreader/sumatrapdf/discussions/2472
// Info: Crop page margins using rendered content bounds.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Padding in points (default 6)
// Arg: Page range (default all)

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
function pageList(text, count) {
  var result = [];
  if (!text || /^all$/i.test(text)) {
    for (var i = 0; i < count; i++) result.push(i);
    return result;
  }
  var parts = text.split(",");
  for (var i = 0; i < parts.length; i++) {
    var m = /^\s*(\d+|N)\s*(?:-\s*(\d+|N)?\s*)?$/i.exec(parts[i]);
    if (!m) throw new Error("Invalid page range: " + parts[i]);
    var first = /^N$/i.test(m[1]) ? count : Number(m[1]);
    var last = m[2] ? (/^N$/i.test(m[2]) ? count : Number(m[2])) : parts[i].indexOf("-") >= 0 ? count : first;
    if (first < 1 || last < 1 || first > count || last > count) throw new Error("Page outside document: " + parts[i]);
    var step = first <= last ? 1 : -1;
    for (var p = first; ; p += step) {
      if (result.indexOf(p - 1) < 0) result.push(p - 1);
      if (p === last) break;
    }
  }
  return result;
}
var padding = number(scriptArgs[2], 6),
  selected = pageList(scriptArgs[3], doc.countPages());
if (padding < 0) throw new Error("Padding must not be negative.");
var kScanScale = 0.5,
  kWhiteThreshold = 250;
for (var i = 0; i < selected.length; i++) {
  var page = doc.loadPage(selected[i]),
    bounds = page.getBounds();
  var pix = page.toPixmap(Matrix.scale(kScanScale, kScanScale), ColorSpace.DeviceRGB, false);
  var pixels = pix.getPixels(),
    w = pix.getWidth(),
    h = pix.getHeight(),
    n = pix.getNumberOfComponents();
  var x0 = w,
    y0 = h,
    x1 = -1,
    y1 = -1;
  for (var y = 0; y < h; y++)
    for (var x = 0; x < w; x++) {
      var offset = (y * w + x) * n;
      if (
        pixels[offset] < kWhiteThreshold ||
        pixels[offset + 1] < kWhiteThreshold ||
        pixels[offset + 2] < kWhiteThreshold
      ) {
        x0 = Math.min(x0, x);
        y0 = Math.min(y0, y);
        x1 = Math.max(x1, x);
        y1 = Math.max(y1, y);
      }
    }
  if (x1 < 0) continue;
  var r = [
    Math.max(bounds[0], (pix.getX() + x0) / kScanScale - padding),
    Math.max(bounds[1], (pix.getY() + y0) / kScanScale - padding),
    Math.min(bounds[2], (pix.getX() + x1 + 1) / kScanScale + padding),
    Math.min(bounds[3], (pix.getY() + y1 + 1) / kScanScale + padding)
  ];
  page.setPageBox("CropBox", r);
}
save();
