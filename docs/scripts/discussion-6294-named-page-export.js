// Discussion #6294: https://github.com/sumatrapdfreader/sumatrapdf/discussions/6294
// Info: Export PDF or PNG pages with bookmark, date, or custom filenames.
// Arg: Input PDF
// Arg: Output filename template (.pdf or .png; folder must exist)
// Arg: Page range (default all)
// Arg: PNG DPI (default 150)

// Tokens: [bookmarkname], [pdfname], [page] (or [N]), [yyyy], [mm], [dd].

function pad(value, width) {
  var text = String(value);
  while (text.length < width) text = "0" + text;
  return text;
}

function safeName(name) {
  name = name.replace(/[<>:"/\\|?*\x00-\x1f]/g, "_").replace(/^\s+|[.\s]+$/g, "");
  if (!name) name = "page";
  if (/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(name)) name = "_" + name;
  return name;
}

function parsePages(text, count) {
  var pages = [];
  if (!text || text.toLowerCase() === "all") {
    for (var i = 0; i < count; i++) pages.push(i);
    return pages;
  }
  var parts = text.split(",");
  for (var j = 0; j < parts.length; j++) {
    var match = /^\s*(\d+)\s*(?:-\s*(\d+)\s*)?$/.exec(parts[j]);
    if (!match) throw new Error("Invalid page range: " + parts[j]);
    var first = Number(match[1]);
    var last = match[2] ? Number(match[2]) : first;
    if (first < 1 || last < 1 || first > count || last > count) throw new Error("Page outside document: " + parts[j]);
    var step = first <= last ? 1 : -1;
    for (var p = first; ; p += step) {
      if (pages.indexOf(p - 1) < 0) pages.push(p - 1);
      if (p === last) break;
    }
  }
  return pages;
}

function bookmarkNames(doc, count, fallback) {
  var names = [];
  function visit(nodes) {
    for (var i = 0; i < nodes.length; i++) {
      var node = nodes[i];
      if (node.uri && node.uri.charAt(0) === "#" && node.title && /\S/.test(node.title)) {
        var page = -1;
        try {
          page = doc.resolveLink(node.uri);
        } catch (e) {}
        if (page >= 0 && page < count) names[page] = node.title;
      }
      if (node.down) visit(node.down);
    }
  }
  visit(doc.loadOutline() || []);
  var current = fallback;
  for (var i = 0; i < count; i++) {
    if (names[i]) current = names[i];
    names[i] = current;
  }
  return names;
}

function expandName(template, values) {
  return template.replace(/\[([^\]]+)\]/g, function (token, key) {
    if (!values.hasOwnProperty(key)) throw new Error("Unknown filename token: " + token);
    return values[key];
  });
}

function pathKey(path) {
  return path.replace(/\\/g, "/").toLowerCase();
}

if (scriptArgs.length < 2 || !scriptArgs[0] || !scriptArgs[1]) {
  throw new Error("Arguments: input.pdf output-template.pdf|png [pages] [PNG DPI]");
}

var input = scriptArgs[0];
var template = scriptArgs[1];
var split = Math.max(template.lastIndexOf("/"), template.lastIndexOf("\\")) + 1;
var directory = template.substring(0, split);
var filename = template.substring(split);
var extension = /\.(pdf|png)$/i.exec(filename);
if (!extension) throw new Error("The output template must end in .pdf or .png.");
var format = extension[1].toLowerCase();
var stemTemplate = filename.substring(0, filename.length - extension[0].length);
var sourceName = input.substring(Math.max(input.lastIndexOf("/"), input.lastIndexOf("\\")) + 1).replace(/\.pdf$/i, "");
var doc = Document.openDocument(input);
if (typeof doc.rearrangePages !== "function") throw new Error("Input must be a PDF.");
if (doc.needsPassword()) throw new Error("Input PDF requires a password.");
var count = doc.countPages();
var pages = parsePages(scriptArgs[2], count);
var dpi = scriptArgs[3] ? Number(scriptArgs[3]) : 150;
if (format === "png" && (!isFinite(dpi) || dpi <= 0)) throw new Error("PNG DPI must be a positive number.");
var bookmarks = bookmarkNames(doc, count, sourceName);
var date = new Date();
var used = {};
var outputs = [];

// Plan every filename first, so invalid ranges or tokens cannot leave a partial export.
for (var i = 0; i < pages.length; i++) {
  var page = pages[i];
  var number = pad(page + 1, 3);
  var values = {
    bookmarkname: bookmarks[page],
    pdfname: sourceName,
    page: number,
    N: number,
    yyyy: pad(date.getFullYear(), 4),
    mm: pad(date.getMonth() + 1, 2),
    dd: pad(date.getDate(), 2)
  };
  var stem = safeName(expandName(stemTemplate, values));
  var name = stem;
  var suffix = 0;
  while (used["$" + name.toLowerCase()]) {
    name = stem + "-" + number + (suffix ? "-" + suffix : "");
    suffix++;
  }
  used["$" + name.toLowerCase()] = true;
  var output = directory + name + "." + format;
  if (pathKey(output) === pathKey(input)) throw new Error("Output would replace the input PDF: " + output);
  outputs.push(output);
}

for (var i = 0; i < pages.length; i++) {
  var output = outputs[i];
  if (format === "png") {
    var pixmap = doc.loadPage(pages[i]).toPixmap(Matrix.scale(dpi / 72, dpi / 72), ColorSpace.DeviceRGB, false);
    pixmap.setResolution(dpi, dpi);
    pixmap.saveAsPNG(output);
  } else {
    // Reopen per page to retain annotations and form fields while removing other pages.
    var single = Document.openDocument(input);
    single.rearrangePages([pages[i]]);
    single.save(output, "garbage=4,compress");
  }
  print("Page " + (pages[i] + 1) + ": " + output);
}
print("Exported " + pages.length + " page(s).");
