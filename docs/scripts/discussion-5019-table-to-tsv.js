// Discussion #5019: https://github.com/sumatrapdfreader/sumatrapdf/discussions/5019
// Info: Extract table text as tab-separated values using column dividers.
// Arg: Input PDF
// Arg: Output TSV file
// Arg: Page range (default all)
// Arg: Comma-separated column-divider x coordinates, e.g. 60,120
// Arg: Baseline row tolerance in points (default 3)

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
var selected = pageList(scriptArgs[2], doc.countPages()),
  cuts = (scriptArgs[3] || "").split(","),
  tolerance = number(scriptArgs[4], 3);
if (!scriptArgs[3]) throw new Error("Supply x coordinates of column separators as Arg 4, e.g. 60,120.");
if (tolerance <= 0) throw new Error("Row tolerance must be positive.");
cuts = cuts.map(function (s) {
  return number(s);
});
for (var i = 1; i < cuts.length; i++) if (cuts[i] <= cuts[i - 1]) throw new Error("Column separators must increase.");
var out = [
  "page\t" +
    cuts
      .map(function (v, i) {
        return "column" + (i + 1);
      })
      .concat(["column" + (cuts.length + 1)])
      .join("\t")
];
for (var i = 0; i < selected.length; i++) {
  var rows = [];
  doc
    .loadPage(selected[i])
    .toStructuredText()
    .walk({
      onChar: function (c, origin) {
        var row = null;
        for (var j = 0; j < rows.length; j++)
          if (Math.abs(rows[j].y - origin[1]) <= tolerance) {
            row = rows[j];
            break;
          }
        if (!row) {
          row = { y: origin[1], chars: [] };
          rows.push(row);
        }
        row.chars.push({ x: origin[0], c: c });
      }
    });
  rows.sort(function (a, b) {
    return a.y - b.y;
  });
  for (var j = 0; j < rows.length; j++) {
    var cells = [];
    for (var k = 0; k <= cuts.length; k++) cells.push("");
    rows[j].chars.sort(function (a, b) {
      return a.x - b.x;
    });
    for (var k = 0; k < rows[j].chars.length; k++) {
      var ch = rows[j].chars[k],
        column = 0;
      while (column < cuts.length && ch.x >= cuts[column]) column++;
      cells[column] += ch.c;
    }
    out.push(
      String(selected[i] + 1) +
        "\t" +
        cells
          .map(function (s) {
            return s.replace(/[\t\r\n]/g, " ").replace(/^\s+|\s+$/g, "");
          })
          .join("\t")
    );
  }
}
writeText(out.join("\n") + "\n");
