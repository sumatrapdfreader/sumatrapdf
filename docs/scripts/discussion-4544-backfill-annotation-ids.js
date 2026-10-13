// Discussion #4544: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4544
// Info: Add missing annotation IDs and optional timestamps.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Mode: ids or stamp (default ids)

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
var mode = scriptArgs[2] || "ids";
if (mode !== "ids" && mode !== "stamp") throw new Error("Mode must be ids or stamp.");
var prefix = "sumatra-" + new Date().getTime().toString(36) + "-" + Math.floor(Math.random() * 1000000000).toString(36),
  changed = 0;
annotations(function (a, page, p, i) {
  var obj = a.getObject();
  if (!obj.NM || !obj.NM.asString()) {
    obj.put("NM", doc.newString(prefix + "-" + (p + 1) + "-" + (i + 1)));
    changed++;
  }
  if (mode === "stamp") a.setModificationDate(new Date());
});
save();
print("Assigned " + changed + " missing IDs.");
