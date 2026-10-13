// Discussion #3174: https://github.com/sumatrapdfreader/sumatrapdf/discussions/3174
// Info: Merge two PDFs or extract selected pages.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Action: merge or extract
// Arg: Second PDF for merge; page range for extract
// Arg: First PDF page range for merge (default all)
// Arg: Second PDF page range for merge (default all)

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
var action = scriptArgs[2] || "extract";
if (action === "extract") {
  doc.rearrangePages(pageList(scriptArgs[3], doc.countPages()));
  save();
} else if (action === "merge") {
  if (!scriptArgs[3]) throw new Error("Supply the second PDF as Arg 4.");
  if (pathKey(scriptArgs[3]) === pathKey(output)) throw new Error("Output must differ from the second PDF.");
  var other = Document.openDocument(scriptArgs[3]);
  if (other.needsPassword()) throw new Error("Use a decrypted second PDF.");
  var a = pageList(scriptArgs[4], doc.countPages()),
    b = pageList(scriptArgs[5], other.countPages());
  doc.bake(true, true);
  other.bake(true, true);
  var merged = new PDFDocument();
  for (var i = 0; i < a.length; i++) merged.graftPage(-1, doc, a[i]);
  for (var i = 0; i < b.length; i++) merged.graftPage(-1, other, b[i]);
  merged.save(output, "garbage=4,compress");
  print("Saved " + output);
} else throw new Error("Action must be extract or merge.");
