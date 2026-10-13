// Discussion #4359: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4359
// Info: Copy highlighted text into highlight annotation contents.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Mode: missing or all (default missing)

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
function annotations(fn) {
  for (var p = 0; p < doc.countPages(); p++) {
    var page = doc.loadPage(p),
      list = page.getAnnotations();
    for (var i = 0; i < list.length; i++) fn(list[i], page, p, i);
    page.update();
  }
}
var mode = scriptArgs[2] || "missing";
if (mode !== "missing" && mode !== "all") throw new Error("Mode must be missing or all.");
function inside(x, y, q) {
  var order = [0, 2, 6, 4],
    sign = 0;
  for (var i = 0; i < 4; i++) {
    var a = order[i],
      b = order[(i + 1) % 4];
    var cross = (q[b] - q[a]) * (y - q[a + 1]) - (q[b + 1] - q[a + 1]) * (x - q[a]);
    if (Math.abs(cross) < 0.00001) continue;
    var s = cross < 0 ? -1 : 1;
    if (sign && sign !== s) return false;
    sign = s;
  }
  return true;
}
var changed = 0;
annotations(function (a, page) {
  if (a.getType() !== "Highlight" || (mode === "missing" && a.getContents())) return;
  var quads = a.getQuadPoints(),
    lines = [],
    chars = [];
  page.toStructuredText().walk({
    beginLine: function () {
      chars = [];
    },
    onChar: function (c, origin, font, size, q) {
      var x = (q[0] + q[2] + q[4] + q[6]) / 4,
        y = (q[1] + q[3] + q[5] + q[7]) / 4;
      for (var i = 0; i < quads.length; i++)
        if (inside(x, y, quads[i])) {
          chars.push(c);
          break;
        }
    },
    endLine: function () {
      if (chars.length) lines.push(chars.join(""));
    }
  });
  if (lines.length) {
    a.setContents(lines.join("\n"));
    changed++;
  }
});
save();
print("Populated " + changed + " highlights.");
