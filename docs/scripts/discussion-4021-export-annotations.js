// Discussion #4021: https://github.com/sumatrapdfreader/sumatrapdf/discussions/4021
// Info: Export annotations to JSON or XML.
// Arg: Input PDF
// Arg: Output .json or .xml file

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
function writeText(s) {
  var b = new Buffer();
  b.write(s);
  b.save(output);
  print("Saved " + output);
}
var records = [];
annotations(function (a, page, p) {
  var r = {
    page: p + 1,
    type: a.getType(),
    rect: a.getBounds(),
    color: a.getColor(),
    opacity: a.getOpacity(),
    author: a.getAuthor(),
    contents: a.getContents()
  };
  if (a.hasQuadPoints()) r.quads = a.getQuadPoints();
  records.push(r);
});
var data = { source: input, annotations: records },
  content = JSON.stringify(data, null, 2);
if (/\.xml$/i.test(output)) {
  function escape(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }
  content = '<?xml version="1.0" encoding="UTF-8"?>\n<annotations>\n';
  for (var i = 0; i < records.length; i++)
    content +=
      '  <annotation page="' +
      records[i].page +
      '"><data>' +
      escape(JSON.stringify(records[i])) +
      "</data></annotation>\n";
  content += "</annotations>\n";
}
writeText(content);
