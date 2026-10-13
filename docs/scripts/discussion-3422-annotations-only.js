// Discussion #3422: https://github.com/sumatrapdfreader/sumatrapdf/discussions/3422
// Info: Remove page content and keep annotations.
// Arg: Input PDF
// Arg: New output PDF

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
for (var p = 0; p < doc.countPages(); p++) doc.findPage(p).delete("Contents");
doc.bake(true, false);
for (var p = 0; p < doc.countPages(); p++) doc.findPage(p).delete("Annots");
doc.getTrailer().Root.delete("AcroForm");
save();
