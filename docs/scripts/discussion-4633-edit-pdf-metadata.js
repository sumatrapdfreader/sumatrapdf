// Discussion #4633: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4633
// Info: Set or clear a PDF title, author, subject, or keywords.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Field: title/author/subject/keywords
// Arg: New value (empty clears it)

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
var keys = { title: "info:Title", author: "info:Author", subject: "info:Subject", keywords: "info:Keywords" };
var key = (scriptArgs[2] || "title").toLowerCase();
if (!keys[key] || scriptArgs[3] === undefined)
  throw new Error("Supply title|author|subject|keywords and a value (empty clears it).");
doc.setMetaData(keys[key], scriptArgs[3]);
save();
