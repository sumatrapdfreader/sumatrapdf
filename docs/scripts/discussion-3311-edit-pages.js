// Discussion #3311: https://github.com/sumatrapdfreader/sumatrapdf/discussions/3311
// Info: Rotate, delete, reorder, or insert PDF pages.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Action: rotate/delete/reorder/insert (default rotate)
// Arg: Angle for rotate; page list for delete/reorder; second PDF for insert
// Arg: Page range for rotate; insertion position for insert (0 means beginning)
// Arg: Second PDF page range for insert (default all)

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
var action = scriptArgs[2] || "rotate";
if (action === "rotate") {
  var angle = number(scriptArgs[3], 90);
  if (angle % 90) throw new Error("Rotation must be a multiple of 90 degrees.");
  var selected = pageList(scriptArgs[4], doc.countPages());
  for (var i = 0; i < selected.length; i++) {
    var obj = doc.findPage(selected[i]),
      old = obj.getInheritable("Rotate");
    obj.put("Rotate", ((((old ? old.asNumber() : 0) + angle) % 360) + 360) % 360);
  }
} else if (action === "delete" || action === "reorder") {
  if (!scriptArgs[3]) throw new Error("Supply page numbers as Arg 4.");
  var selected = pageList(scriptArgs[3], doc.countPages());
  if (action === "delete") {
    var keep = [];
    for (var i = 0; i < doc.countPages(); i++) if (selected.indexOf(i) < 0) keep.push(i);
    if (!keep.length) throw new Error("Cannot delete every page.");
    selected = keep;
  }
  doc.rearrangePages(selected);
} else if (action === "insert") {
  if (!scriptArgs[3]) throw new Error("Supply another PDF as Arg 4.");
  var after = number(scriptArgs[4], doc.countPages());
  if (after % 1 || after < 0 || after > doc.countPages()) throw new Error("Invalid insertion position.");
  var other = Document.openDocument(scriptArgs[3]);
  if (other.needsPassword()) throw new Error("Use a decrypted second PDF.");
  var selected = pageList(scriptArgs[5], other.countPages());
  other.bake(true, true);
  for (var i = 0; i < selected.length; i++) doc.graftPage(after + i, other, selected[i]);
} else throw new Error("Action must be rotate, delete, reorder or insert.");
save();
