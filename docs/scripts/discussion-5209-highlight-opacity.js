// Discussion #5209: https://github.com/sumatrapdfreader/sumatrapdf/discussions/5209
// Info: Change the opacity of all highlight annotations.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Opacity percent (0–100; default 50)

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
var percent = number(scriptArgs[2], 50),
  changed = 0;
if (percent < 0 || percent > 100) throw new Error("Opacity must be 0..100 percent.");
annotations(function (a) {
  if (a.getType() === "Highlight") {
    a.setOpacity(percent / 100);
    changed++;
  }
});
save();
print("Changed " + changed + " highlights.");
