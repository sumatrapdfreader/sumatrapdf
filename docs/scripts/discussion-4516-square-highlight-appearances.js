// Discussion #4516: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4516
// Info: Give highlight annotations square corners.
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
var changed = 0;
for (var p = 0; p < doc.countPages(); p++) {
  var page = doc.loadPage(p);
  page.update();
  var inverse = Matrix.invert(page.getTransform());
  page.getAnnotations().forEach(function (a) {
    if (a.getType() !== "Highlight") return;
    var quads = a.getQuadPoints(),
      content = "/H gs\n",
      box = [Infinity, Infinity, -Infinity, -Infinity],
      color = a.getColor();
    if (!quads.length) return;
    if (color.length !== 3) color = [1, 1, 0];
    content += color.join(" ") + " rg\n";
    for (var i = 0; i < quads.length; i++) {
      var q = quads[i],
        pts = [];
      for (var j = 0; j < 8; j += 2) {
        var x = q[j] * inverse[0] + q[j + 1] * inverse[2] + inverse[4],
          y = q[j] * inverse[1] + q[j + 1] * inverse[3] + inverse[5];
        pts.push([x, y]);
        box[0] = Math.min(box[0], x);
        box[1] = Math.min(box[1], y);
        box[2] = Math.max(box[2], x);
        box[3] = Math.max(box[3], y);
      }
      content +=
        pts[0].join(" ") + " m " + pts[1].join(" ") + " l " + pts[3].join(" ") + " l " + pts[2].join(" ") + " l h f\n";
    }
    a.setAppearance(
      "N",
      null,
      Matrix.identity,
      box,
      { ExtGState: { H: { Type: "ExtGState", ca: a.getOpacity(), BM: "Multiply" } } },
      content
    );
    changed++;
  });
}
save();
print("Changed " + changed + " highlight appearances.");
