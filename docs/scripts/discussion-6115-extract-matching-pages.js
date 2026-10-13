// Discussion #6115: https://github.com/sumatrapdfreader/sumatrapdf/discussions/6115
// Info: Extract pages containing a search phrase.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Search phrase

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
var needle = scriptArgs[2];
if (!needle || !/\S/.test(needle)) throw new Error("Supply text to search for.");
var selected = [];
for (var i = 0; i < doc.countPages(); i++) if (doc.loadPage(i).search(needle).length) selected.push(i);
if (!selected.length) throw new Error("No matching pages; no output written.");
doc.rearrangePages(selected);
save();
print("Extracted " + selected.length + " matching pages.");
