// Discussion #3920: https://github.com/sumatrapdfreader/sumatrapdf/discussions/3920
// Info: Export inverted page images or a PDF of inverted images.
// Arg: Input PDF
// Arg: Output .pdf or .png template
// Arg: DPI (default 150)
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
var kPointsPerInch = 72,
  dpi = number(scriptArgs[2], 150),
  selected = pageList(scriptArgs[3], doc.countPages());
if (dpi <= 0 || dpi % 1) throw new Error("DPI must be a positive integer.");
var ext = /\.(pdf|png)$/i.exec(output);
if (!ext) throw new Error("Output must end in .pdf or .png.");
var pdf = ext[1].toLowerCase() === "pdf" ? new PDFDocument() : null;
var template = output;
if (!pdf && template.indexOf("[page]") < 0) template = template.replace(/\.png$/i, "-[page].png");
for (var i = 0; i < selected.length; i++) {
  var page = doc.loadPage(selected[i]),
    r = page.getBounds();
  var pix = page.toPixmap(Matrix.scale(dpi / kPointsPerInch, dpi / kPointsPerInch), ColorSpace.DeviceRGB, false);
  pix.invert();
  pix.setResolution(dpi, dpi);
  if (pdf) {
    var ref = pdf.addImage(new Image(pix)),
      w = r[2] - r[0],
      h = r[3] - r[1];
    pdf.insertPage(
      -1,
      pdf.addPage([0, 0, w, h], 0, { XObject: { Im: ref } }, "q " + w + " 0 0 " + h + " 0 0 cm /Im Do Q")
    );
  } else {
    var path = template.replace(/\[page\]/g, String(selected[i] + 1));
    pix.saveAsPNG(path);
    print("Saved " + path);
  }
}
if (pdf) {
  pdf.save(output, "garbage=4,compress");
  print("Saved " + output);
}
