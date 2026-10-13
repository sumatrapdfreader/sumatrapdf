// Discussion #4674: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4674
// Info: Export bookmark titles and page numbers to a text file.
// Arg: Input PDF
// Arg: Output text file

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
function writeText(s) {
  var b = new Buffer();
  b.write(s);
  b.save(output);
  print("Saved " + output);
}
var rows = [];
function visit(nodes, depth) {
  for (var i = 0; i < nodes.length; i++) {
    var n = nodes[i],
      page = "external";
    if (n.uri && n.uri.charAt(0) === "#") {
      try {
        var p = doc.resolveLink(n.uri);
        if (p >= 0) page = String(p + 1);
      } catch (e) {}
    }
    rows.push(new Array(depth + 1).join("  ") + (n.title || "(untitled)") + "\t" + page);
    if (n.down) visit(n.down, depth + 1);
  }
}
visit(doc.loadOutline() || [], 0);
writeText(rows.join("\n") + "\n");
print(rows.length + " embedded bookmarks.");
