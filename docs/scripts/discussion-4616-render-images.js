// Discussion #4616: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4616
// Info: Export pages as PNG or JPEG images at a chosen DPI.
// Arg: Input PDF
// Arg: Image filename template (.png or .jpg; folder must exist)
// Arg: DPI (default 300)
// Arg: Page range (default all)
// Arg: JPEG quality (default 90)

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
var kPointsPerInch = 72;
var dpi = number(scriptArgs[2], 300),
  selected = pageList(scriptArgs[3], doc.countPages());
var quality = number(scriptArgs[4], 90);
if (dpi <= 0 || dpi % 1) throw new Error("DPI must be a positive integer.");
if (quality < 1 || quality > 100) throw new Error("JPEG quality must be 1..100.");
var ext = /\.(png|jpe?g)$/i.exec(output);
if (!ext) throw new Error("Output template must end in .png or .jpg.");
var template = output;
if (!/\[page\]|<N>/.test(template)) template = template.replace(/(\.[^.]+)$/, "-[page]$1");
for (var i = 0; i < selected.length; i++) {
  var p = selected[i],
    n = ("000" + (p + 1)).slice(-Math.max(3, String(p + 1).length));
  var path = template.replace(/\[page\]|<N>/g, n);
  var pix = doc
    .loadPage(p)
    .toPixmap(Matrix.scale(dpi / kPointsPerInch, dpi / kPointsPerInch), ColorSpace.DeviceRGB, false);
  pix.setResolution(dpi, dpi);
  if (ext[1].toLowerCase() === "png") pix.saveAsPNG(path);
  else pix.saveAsJPEG(path, quality);
  print("Saved " + path);
}
