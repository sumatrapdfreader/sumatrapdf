// Discussion #4230: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4230
// Info: Report fonts used by selected characters as tab-separated values.
// Arg: Input PDF
// Arg: Output TSV file
// Arg: Characters to report (empty means all)
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
function writeText(s) {
  var b = new Buffer();
  b.write(s);
  b.save(output);
  print("Saved " + output);
}
var needle = scriptArgs[2] || "",
  selected = pageList(scriptArgs[3], doc.countPages()),
  rows = ["page\tcharacter\tx\ty\tfont\tsize"];
for (var i = 0; i < selected.length; i++)
  doc
    .loadPage(selected[i])
    .toStructuredText()
    .walk({
      onChar: function (c, origin, font, size) {
        if (needle && needle.indexOf(c) < 0) return;
        rows.push(
          [selected[i] + 1, c.replace(/[\t\r\n]/g, " "), origin[0], origin[1], font.getName(), size].join("\t")
        );
      }
    });
writeText(rows.join("\n") + "\n");
