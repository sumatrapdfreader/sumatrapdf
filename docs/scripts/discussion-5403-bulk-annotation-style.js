// Discussion #5403: https://github.com/sumatrapdfreader/sumatrapdf/discussions/5403
// Info: Change annotation colors and opacity in bulk.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Annotation type or all (default Highlight)
// Arg: RGB hex color without # (default ffff00)
// Arg: Opacity percent (default 50)

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
var type = (scriptArgs[2] || "Highlight").toLowerCase(),
  rgb = scriptArgs[3] || "ffff00",
  opacity = number(scriptArgs[4], 50);
if (!/^[0-9a-f]{6}$/i.test(rgb)) throw new Error("Color must be six hex digits, e.g. ff9900.");
if (opacity < 0 || opacity > 100) throw new Error("Opacity must be 0..100 percent.");
var color = [
    parseInt(rgb.substring(0, 2), 16) / 255,
    parseInt(rgb.substring(2, 4), 16) / 255,
    parseInt(rgb.substring(4, 6), 16) / 255
  ],
  changed = 0;
annotations(function (a) {
  if (type === "all" || a.getType().toLowerCase() === type) {
    a.setColor(color);
    a.setOpacity(opacity / 100);
    changed++;
  }
});
save();
print("Changed " + changed + " annotations.");
