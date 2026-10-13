// Discussion #5549: https://github.com/sumatrapdfreader/sumatrapdf/discussions/5549
// Info: Extract pages listed in saved SumatraPDF favorites.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Path to saved SumatraPDF-settings.txt

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
if (!scriptArgs[2]) throw new Error("Supply the saved SumatraPDF-settings.txt path as Arg 3.");
var root = { name: "", values: {}, children: [] },
  stack = [root];
var lines = readFile(scriptArgs[2])
  .asString()
  .replace(/^\uFEFF/, "")
  .split(/\r?\n/);
for (var i = 0; i < lines.length; i++) {
  var line = lines[i].replace(/^\s+|\s+$/g, "");
  if (!line || line.charAt(0) === "#") continue;
  if (line === "]") {
    if (stack.length < 2) throw new Error("Invalid settings brackets.");
    stack.pop();
    continue;
  }
  var m = /^([^\[\]]*?)\s*\[$/.exec(line);
  if (m) {
    var node = { name: m[1].replace(/\s+$/, ""), values: {}, children: [] };
    stack[stack.length - 1].children.push(node);
    stack.push(node);
    continue;
  }
  var equal = line.indexOf("=");
  if (equal >= 0)
    stack[stack.length - 1].values[line.substring(0, equal).replace(/\s+$/, "")] = line
      .substring(equal + 1)
      .replace(/^\s+/, "");
}
if (stack.length !== 1) throw new Error("Invalid settings brackets.");
var selected = [];
function find(node) {
  if (node.values.FilePath && pathKey(node.values.FilePath) === pathKey(input)) {
    for (var i = 0; i < node.children.length; i++)
      if (node.children[i].name === "Favorites") {
        var favorites = node.children[i].children;
        for (var j = 0; j < favorites.length; j++) {
          var s = favorites[j].values.PageNo;
          if (!/^\d+$/.test(s || "")) throw new Error("Favorite uses a non-numeric page: " + s);
          var p = Number(s) - 1;
          if (p < 0 || p >= doc.countPages()) throw new Error("Favorite page outside PDF.");
          if (selected.indexOf(p) < 0) selected.push(p);
        }
      }
  }
  for (var i = 0; i < node.children.length; i++) find(node.children[i]);
}
find(root);
if (!selected.length) throw new Error("No saved Favorites found for this input path.");
selected.sort(function (a, b) {
  return a - b;
});
doc.rearrangePages(selected);
save();
