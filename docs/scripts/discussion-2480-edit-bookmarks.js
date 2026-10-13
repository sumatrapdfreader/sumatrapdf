// Discussion #2480: https://github.com/sumatrapdfreader/sumatrapdf/discussions/2480
// Info: List, add, rename, delete, or reorder bookmarks.
// Arg: Input PDF
// Arg: New output PDF
// Arg: Action: list/add/rename/delete/order (default list)
// Arg: Bookmark index for rename/delete; page for add; root order for order
// Arg: Title for add/rename

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
var tree = doc.loadOutline() || [],
  entries = [];
function collect(list) {
  for (var i = 0; i < list.length; i++) {
    entries.push({ node: list[i], list: list, index: i });
    if (list[i].down) collect(list[i].down);
  }
}
collect(tree);
for (var i = 0; i < entries.length; i++) print(i + 1 + ": " + (entries[i].node.title || "(untitled)"));
var action = scriptArgs[2] || "list";
if (action === "add") {
  var p = number(scriptArgs[3], 1);
  if (p % 1 || p < 1 || p > doc.countPages()) throw new Error("Invalid page number.");
  tree.push({ title: scriptArgs[4] || "New bookmark", uri: "#page=" + p });
} else if (action === "rename" || action === "delete") {
  var index = number(scriptArgs[3], 1) - 1;
  if (index % 1 || index < 0 || index >= entries.length) throw new Error("Invalid bookmark index; use list first.");
  var e = entries[index];
  if (action === "rename") {
    if (!scriptArgs[4]) throw new Error("Supply the new title as Arg 5.");
    e.node.title = scriptArgs[4];
  } else e.list.splice(e.index, 1);
} else if (action === "order") {
  if (!scriptArgs[3]) throw new Error("Supply top-level bookmark order, e.g. 2,1.");
  var order = scriptArgs[3].split(","),
    reordered = [],
    seen = {};
  if (order.length !== tree.length) throw new Error("List every top-level bookmark once.");
  for (var i = 0; i < order.length; i++) {
    var p = Number(order[i]) - 1;
    if (p % 1 || p < 0 || p >= tree.length || seen[p]) throw new Error("Invalid top-level order.");
    seen[p] = true;
    reordered.push(tree[p]);
  }
  tree = reordered;
} else if (action !== "list") throw new Error("Action must be list, add, rename, delete or order.");
if (action !== "list") {
  var cursor = doc.outlineIterator();
  while (cursor.item()) cursor.delete();
  function insert(nodes) {
    for (var i = 0; i < nodes.length; i++) {
      var n = nodes[i];
      cursor.insert({ title: n.title || "", uri: n.uri || "", open: true });
      if (n.down && n.down.length) {
        cursor.prev();
        cursor.down();
        insert(n.down);
        cursor.up();
        cursor.next();
      }
    }
  }
  insert(tree);
}
save();
