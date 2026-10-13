// Discussion #3574: https://github.com/sumatrapdfreader/sumatrapdf/discussions/3574
// Info: Save embedded files and file-attachment annotations.
// Arg: Input PDF
// Arg: Existing destination folder
// Arg: Filter: all or xml (default all)

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
function safeName(s) {
  s = String(s)
    .replace(/[<>:"/\\|?*\x00-\x1f]/g, "_")
    .replace(/^\s+|[.\s]+$/g, "");
  if (!s) s = "attachment";
  if (/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(s)) s = "_" + s;
  return s;
}
var filter = (scriptArgs[2] || "all").toLowerCase(),
  seen = {};
if (filter !== "all" && filter !== "xml") throw new Error("Filter must be all or xml.");
var tree = doc.getTrailer().get("Root", "Names", "EmbeddedFiles"),
  files = tree ? doc.getEmbeddedFiles() : {};
function emit(name, spec) {
  if (filter !== "all" && name.toLowerCase().slice(-4) !== ".xml") return;
  var data = doc.getEmbeddedFileContents(spec);
  if (!data) return;
  var stem = safeName(name),
    unique = stem,
    suffix = 2;
  while (seen["$" + unique.toLowerCase()]) unique = safeName(String(suffix++) + "-" + stem);
  seen["$" + unique.toLowerCase()] = true;
  var path = output.replace(/[\\/]$/, "") + "/" + unique;
  if (pathKey(path) === pathKey(input)) throw new Error("Attachment output would replace input.");
  data.save(path);
  print("Saved " + path);
}
for (var name in files) if (files.hasOwnProperty(name)) emit(name, files[name]);
for (var p = 0; p < doc.countPages(); p++)
  doc
    .loadPage(p)
    .getAnnotations()
    .forEach(function (a) {
      if (a.getType() === "FileAttachment" && a.hasFilespec()) {
        var fs = a.getFilespec();
        emit(doc.getFilespecParams(fs).filename, fs);
      }
    });
