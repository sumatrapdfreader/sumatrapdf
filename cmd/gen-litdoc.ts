import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

/*
Generates the Microsoft LIT tag / attribute code tables in src/LitDoc.cpp
(between the `@gen-start litdoc-maps` / `@gen-end litdoc-maps` markers), used
to reconstruct HTML and OPF from the tokenized binary form inside .lit files.

The data comes from calibre's src/calibre/ebooks/lit/maps/{html,opf}.py (which
are themselves copied from ConvertLIT).

All attribute lists are packed into two parallel arrays - a SeqStrings of
names and a u16 array of codes - and each list (per-tag, the shared html
fallback, the opf list) becomes a {first, count} span into them. Identical
lists are stored once.

Run: bun cmd/gen-litdoc.ts
*/

type AttrList = [number, string][];

// null: a tag code that has no name
// prettier-ignore
const htmlTags: (string | null)[] = [
  null, null, null, "a", "acronym", "address", "applet", "area", "b", "base", "basefont", "bdo", "bgsound", "big",
  "blink", "blockquote", "body", "br", "button", "caption", "center", "cite", "code", "col", "colgroup", null, null,
  "dd", "del", "dfn", "dir", "div", "dl", "dt", "em", "embed", "fieldset", "font", "form", "frame", "frameset", null,
  "h1", "h2", "h3", "h4", "h5", "h6", "head", "hr", "html", "i", "iframe", "img", "input", "ins", "kbd", "label",
  "legend", "li", "link", "tag61", "map", "tag63", "tag64", "meta", "nextid", "nobr", "noembed", "noframes",
  "noscript", "object", "ol", "option", "p", "param", "plaintext", "pre", "q", "rp", "rt", "ruby", "s", "samp",
  "script", "select", "small", "span", "strike", "strong", "style", "sub", "sup", "table", "tbody", "tc", "td",
  "textarea", "tfoot", "th", "thead", "title", "tr", "tt", "u", "ul", "var", "wbr", null,
];

// attributes shared by all html tags, tried when the per-tag list has no match
// prettier-ignore
const htmlAttrs: AttrList = [
  [0x8010, "tabindex"], [0x8046, "title"], [0x804b, "style"], [0x804d, "disabled"], [0x83ea, "class"],
  [0x83eb, "id"], [0x83fe, "datafld"], [0x83ff, "datasrc"], [0x8400, "dataformatas"], [0x87d6, "accesskey"],
  [0x9392, "lang"], [0x93ed, "language"], [0x93fe, "dir"], [0x9771, "onmouseover"], [0x9772, "onmouseout"],
  [0x9773, "onmousedown"], [0x9774, "onmouseup"], [0x9775, "onmousemove"], [0x9776, "onkeydown"],
  [0x9777, "onkeyup"], [0x9778, "onkeypress"], [0x9779, "onclick"], [0x977a, "ondblclick"], [0x977e, "onhelp"],
  [0x977f, "onfocus"], [0x9780, "onblur"], [0x9783, "onrowexit"], [0x9784, "onrowenter"],
  [0x9786, "onbeforeupdate"], [0x9787, "onafterupdate"], [0x978a, "onreadystatechange"], [0x9790, "onscroll"],
  [0x9794, "ondragstart"], [0x9795, "onresize"], [0x9796, "onselectstart"], [0x9797, "onerrorupdate"],
  [0x9799, "ondatasetchanged"], [0x979a, "ondataavailable"], [0x979b, "ondatasetcomplete"],
  [0x979c, "onfilterchange"], [0x979f, "onlosecapture"], [0x97a0, "onpropertychange"], [0x97a2, "ondrag"],
  [0x97a3, "ondragend"], [0x97a4, "ondragenter"], [0x97a5, "ondragover"], [0x97a6, "ondragleave"],
  [0x97a7, "ondrop"], [0x97a8, "oncut"], [0x97a9, "oncopy"], [0x97aa, "onpaste"], [0x97ab, "onbeforecut"],
  [0x97ac, "onbeforecopy"], [0x97ad, "onbeforepaste"], [0x97af, "onrowsdelete"], [0x97b0, "onrowsinserted"],
  [0x97b1, "oncellchange"], [0x97b2, "oncontextmenu"], [0x97b6, "onbeforeeditfocus"],
];

// per-html-tag attribute lists, keyed by tag code
// prettier-ignore
const htmlTagAttrs: Record<number, AttrList> = {
  3: [
    [0x1, "href"], [0x3ec, "target"], [0x3ee, "rel"], [0x3ef, "rev"], [0x3f0, "urn"], [0x3f1, "methods"],
    [0x8001, "name"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"],
  ],
  5: [[0x9399, "clear"]],
  6: [
    [0x8001, "name"], [0x8006, "width"], [0x8007, "height"], [0x804a, "align"], [0x8bbb, "classid"],
    [0x8bbc, "data"], [0x8bbf, "codebase"], [0x8bc0, "codetype"], [0x8bc1, "code"], [0x8bc2, "type"],
    [0x8bc5, "vspace"], [0x8bc6, "hspace"], [0x978e, "onerror"],
  ],
  7: [
    [0x1, "href"], [0x3ea, "shape"], [0x3eb, "coords"], [0x3ed, "target"], [0x3ee, "alt"], [0x3ef, "nohref"],
    [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"],
  ],
  8: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  9: [[0x3ec, "href"], [0x3ed, "target"]],
  10: [[0x938b, "color"], [0x939b, "face"], [0x93a3, "size"]],
  12: [[0x3ea, "src"], [0x3eb, "loop"], [0x3ec, "volume"], [0x3ed, "balance"]],
  13: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  15: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  16: [
    [0x7db, "link"], [0x7dc, "alink"], [0x7dd, "vlink"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"],
    [0x83eb, "id"], [0x938a, "background"], [0x938b, "text"], [0x938e, "nowrap"], [0x93ae, "topmargin"],
    [0x93af, "rightmargin"], [0x93b0, "bottommargin"], [0x93b1, "leftmargin"], [0x93b6, "bgproperties"],
    [0x93d8, "scroll"], [0x977b, "onselect"], [0x9791, "onload"], [0x9792, "onunload"], [0x9798, "onbeforeunload"],
    [0x97b3, "onbeforeprint"], [0x97b4, "onafterprint"], [0xfe0c, "bgcolor"],
  ],
  17: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  18: [[0x7d1, "type"], [0x8001, "name"]],
  19: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x93a8, "valign"]],
  20: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  21: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  22: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  23: [[0x3ea, "span"], [0x8006, "width"], [0x8049, "align"], [0x93a8, "valign"], [0xfe0c, "bgcolor"]],
  24: [[0x3ea, "span"], [0x8006, "width"], [0x8049, "align"], [0x93a8, "valign"], [0xfe0c, "bgcolor"]],
  27: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938e, "nowrap"]],
  29: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  31: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938e, "nowrap"]],
  32: [[0x3ea, "compact"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  33: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938e, "nowrap"]],
  34: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  35: [
    [0x8001, "name"], [0x8006, "width"], [0x8007, "height"], [0x804a, "align"], [0x8bbd, "palette"],
    [0x8bbe, "pluginspage"], [0x8bbf, "src"], [0x8bc1, "units"], [0x8bc2, "type"], [0x8bc3, "hidden"],
  ],
  36: [[0x804a, "align"]],
  37: [
    [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938b, "color"], [0x939b, "face"],
    [0x939c, "size"],
  ],
  38: [
    [0x3ea, "action"], [0x3ec, "enctype"], [0x3ed, "method"], [0x3ef, "target"], [0x3f4, "accept-charset"],
    [0x8001, "name"], [0x977c, "onsubmit"], [0x977d, "onreset"],
  ],
  39: [
    [0x8000, "align"], [0x8001, "name"], [0x8bb9, "src"], [0x8bbb, "border"], [0x8bbc, "frameborder"],
    [0x8bbd, "framespacing"], [0x8bbe, "marginwidth"], [0x8bbf, "marginheight"], [0x8bc0, "noresize"],
    [0x8bc1, "scrolling"], [0x8fa2, "bordercolor"],
  ],
  40: [
    [0x3e9, "rows"], [0x3ea, "cols"], [0x3eb, "border"], [0x3ec, "bordercolor"], [0x3ed, "frameborder"],
    [0x3ee, "framespacing"], [0x8001, "name"], [0x9791, "onload"], [0x9792, "onunload"], [0x9798, "onbeforeunload"],
    [0x97b3, "onbeforeprint"], [0x97b4, "onafterprint"],
  ],
  42: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  43: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  44: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  45: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  46: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  47: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  49: [
    [0x3ea, "noshade"], [0x8006, "width"], [0x8007, "size"], [0x8046, "title"], [0x8049, "align"],
    [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938b, "color"],
  ],
  51: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  52: [
    [0x8001, "name"], [0x8006, "width"], [0x8007, "height"], [0x804a, "align"], [0x8bb9, "src"], [0x8bbb, "border"],
    [0x8bbc, "frameborder"], [0x8bbd, "framespacing"], [0x8bbe, "marginwidth"], [0x8bbf, "marginheight"],
    [0x8bc0, "noresize"], [0x8bc1, "scrolling"], [0x8fa2, "vspace"], [0x8fa3, "hspace"],
  ],
  53: [
    [0x3eb, "alt"], [0x3ec, "src"], [0x3ed, "border"], [0x3ee, "vspace"], [0x3ef, "hspace"], [0x3f0, "lowsrc"],
    [0x3f1, "vrml"], [0x3f2, "dynsrc"], [0x3f4, "loop"], [0x3f6, "start"], [0x7d3, "ismap"], [0x7d9, "usemap"],
    [0x8001, "name"], [0x8006, "width"], [0x8007, "height"], [0x8046, "title"], [0x804a, "align"],
    [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x978d, "onabort"], [0x978e, "onerror"],
    [0x9791, "onload"],
  ],
  54: [
    [0x7d1, "type"], [0x7d3, "size"], [0x7d4, "maxlength"], [0x7d6, "readonly"], [0x7d8, "indeterminate"],
    [0x7da, "checked"], [0x7db, "alt"], [0x7dc, "src"], [0x7dd, "border"], [0x7de, "vspace"], [0x7df, "hspace"],
    [0x7e0, "lowsrc"], [0x7e1, "vrml"], [0x7e2, "dynsrc"], [0x7e4, "loop"], [0x7e5, "start"], [0x8001, "name"],
    [0x8006, "width"], [0x8007, "height"], [0x804a, "align"], [0x93ee, "value"], [0x977b, "onselect"],
    [0x978d, "onabort"], [0x978e, "onerror"], [0x978f, "onchange"], [0x9791, "onload"],
  ],
  56: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  57: [[0x3e9, "for"]],
  58: [[0x804a, "align"]],
  59: [[0x3ea, "value"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x939a, "type"]],
  60: [
    [0x3ee, "href"], [0x3ef, "rel"], [0x3f0, "rev"], [0x3f1, "type"], [0x3f9, "media"], [0x3fa, "target"],
    [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x978e, "onerror"],
    [0x9791, "onload"],
  ],
  61: [[0x9399, "clear"]],
  62: [[0x8001, "name"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  63: [
    [0x1771, "scrolldelay"], [0x1772, "direction"], [0x1773, "behavior"], [0x1774, "scrollamount"],
    [0x1775, "loop"], [0x1776, "vspace"], [0x1777, "hspace"], [0x1778, "truespeed"], [0x8006, "width"],
    [0x8007, "height"], [0x9785, "onbounce"], [0x978b, "onfinish"], [0x978c, "onstart"], [0xfe0c, "bgcolor"],
  ],
  65: [[0x3ea, "http-equiv"], [0x3eb, "content"], [0x3ec, "url"], [0x3f6, "charset"], [0x8001, "name"]],
  66: [[0x3f5, "n"]],
  71: [
    [0x8000, "usemap"], [0x8001, "name"], [0x8006, "width"], [0x8007, "height"], [0x8046, "title"],
    [0x804a, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x8bbb, "classid"], [0x8bbc, "data"],
    [0x8bbf, "codebase"], [0x8bc0, "codetype"], [0x8bc1, "code"], [0x8bc2, "type"], [0x8bc5, "vspace"],
    [0x8bc6, "hspace"], [0x978e, "onerror"],
  ],
  72: [
    [0x3eb, "compact"], [0x3ec, "start"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"],
    [0x939a, "type"],
  ],
  73: [[0x3ea, "selected"], [0x3eb, "value"]],
  74: [[0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  75: [[0x8000, "type"]],
  76: [[0x9399, "clear"]],
  77: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x9399, "clear"]],
  78: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  82: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  83: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  84: [[0x3ea, "src"], [0x3ed, "for"], [0x3ee, "event"], [0x3f0, "defer"], [0x3f2, "type"], [0x978e, "onerror"]],
  85: [[0x3eb, "size"], [0x3ec, "multiple"], [0x8000, "align"], [0x8001, "name"], [0x978f, "onchange"]],
  86: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  87: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  88: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  89: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  90: [[0x3eb, "type"], [0x3ef, "media"], [0x8046, "title"], [0x978e, "onerror"], [0x9791, "onload"]],
  91: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  92: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  93: [
    [0x3ea, "cols"], [0x3eb, "border"], [0x3ec, "rules"], [0x3ed, "frame"], [0x3ee, "cellspacing"],
    [0x3ef, "cellpadding"], [0x3fa, "datapagesize"], [0x8006, "width"], [0x8007, "height"], [0x8046, "title"],
    [0x804a, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938a, "background"],
    [0x93a5, "bordercolor"], [0x93a6, "bordercolorlight"], [0x93a7, "bordercolordark"], [0xfe0c, "bgcolor"],
  ],
  94: [[0x8049, "align"], [0x93a8, "valign"], [0xfe0c, "bgcolor"]],
  95: [[0x8049, "align"], [0x93a8, "valign"]],
  96: [
    [0x7d2, "rowspan"], [0x7d3, "colspan"], [0x8006, "width"], [0x8007, "height"], [0x8046, "title"],
    [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938a, "background"],
    [0x938e, "nowrap"], [0x93a5, "bordercolor"], [0x93a6, "bordercolorlight"], [0x93a7, "bordercolordark"],
    [0x93a8, "valign"], [0xfe0c, "bgcolor"],
  ],
  97: [
    [0x1b5a, "rows"], [0x1b5b, "cols"], [0x1b5c, "wrap"], [0x1b5d, "readonly"], [0x8001, "name"],
    [0x977b, "onselect"], [0x978f, "onchange"],
  ],
  98: [[0x8049, "align"], [0x93a8, "valign"], [0xfe0c, "bgcolor"]],
  99: [
    [0x7d2, "rowspan"], [0x7d3, "colspan"], [0x8006, "width"], [0x8007, "height"], [0x8046, "title"],
    [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x938a, "background"],
    [0x938e, "nowrap"], [0x93a5, "bordercolor"], [0x93a6, "bordercolorlight"], [0x93a7, "bordercolordark"],
    [0x93a8, "valign"], [0xfe0c, "bgcolor"],
  ],
  100: [[0x8049, "align"], [0x93a8, "valign"], [0xfe0c, "bgcolor"]],
  102: [
    [0x8007, "height"], [0x8046, "title"], [0x8049, "align"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"],
    [0x93a5, "bordercolor"], [0x93a6, "bordercolorlight"], [0x93a7, "bordercolordark"], [0x93a8, "valign"],
    [0xfe0c, "bgcolor"],
  ],
  103: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  104: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
  105: [[0x3eb, "compact"], [0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"], [0x939a, "type"]],
  106: [[0x8046, "title"], [0x804b, "style"], [0x83ea, "class"], [0x83eb, "id"]],
};

// prettier-ignore
const opfTags: (string | null)[] = [
  null, "package", "dc:Title", "dc:Creator", null, null, null, null, null, null, null, null, null, null, null, null,
  "manifest", "item", "spine", "itemref", "metadata", "dc-metadata", "dc:Subject", "dc:Description", "dc:Publisher",
  "dc:Contributor", "dc:Date", "dc:Type", "dc:Format", "dc:Identifier", "dc:Source", "dc:Language", "dc:Relation",
  "dc:Coverage", "dc:Rights", "x-metadata", "meta", "tours", "tour", "site", "guide", "reference", null,
];

// prettier-ignore
const opfAttrs: AttrList = [
  [0x1, "href"], [0x2, "%never-used"], [0x3, "%guid"], [0x4, "%minimum_level"], [0x5, "%attr5"], [0x6, "id"],
  [0x7, "href"], [0x8, "media-type"], [0x9, "fallback"], [0xa, "idref"], [0xb, "xmlns:dc"],
  [0xc, "xmlns:oebpackage"], [0xd, "role"], [0xe, "file-as"], [0xf, "event"], [0x10, "scheme"], [0x11, "title"],
  [0x12, "type"], [0x13, "unique-identifier"], [0x14, "name"], [0x15, "content"], [0x16, "xml:lang"],
];

// --- generation ---

const kMaxLineLen = 118;

// pack the attr lists into one pool of (code, name) entries; identical lists
// are stored once and referenced by {first, count} spans
const poolCodes: number[] = [];
const poolNames: string[] = [];
const listStartByKey = new Map<string, number>();

function addList(list: AttrList | undefined): [number, number] {
  if (!list || list.length === 0) {
    return [0, 0];
  }
  const key = JSON.stringify(list);
  let first = listStartByKey.get(key);
  if (first === undefined) {
    first = poolCodes.length;
    for (const [code, name] of list) {
      if (code > 0xffff) {
        throw new Error(`attr code 0x${code.toString(16)} does not fit u16`);
      }
      poolCodes.push(code);
      poolNames.push(name);
    }
    listStartByKey.set(key, first);
  }
  return [first, list.length];
}

const nHtmlTags = htmlTags.length;
const htmlSpans: [number, number][] = [];
for (let tag = 0; tag < nHtmlTags; tag++) {
  htmlSpans.push(addList(htmlTagAttrs[tag]));
}
for (const tag of Object.keys(htmlTagAttrs)) {
  if (Number(tag) < 0 || Number(tag) >= nHtmlTags) {
    throw new Error(`htmlTagAttrs key ${tag} out of tag range`);
  }
}
const htmlFallbackSpan = addList(htmlAttrs);
const opfSpan = addList(opfAttrs);

// SeqStrings literal: "s1\0" "s2\0" ... wrapped to kMaxLineLen, ending with "\0"
// null entries are stored as the "\x01" sentinel (empty isn't representable mid-list)
function seqStringsLines(strs: (string | null)[], indent: string): string[] {
  const parts = strs.map((s) => `"${s === null ? "\\x01" : s}\\0"`);
  parts.push(`"\\0"`);
  const lines: string[] = [];
  let cur = indent;
  for (const p of parts) {
    const next = cur === indent ? cur + p : cur + " " + p;
    if (next.length > kMaxLineLen && cur !== indent) {
      lines.push(cur);
      cur = indent + p;
    } else {
      cur = next;
    }
  }
  lines.push(cur + ";");
  return lines;
}

function wrapItems(items: string[], indent: string): string[] {
  const lines: string[] = [];
  let cur = indent;
  for (const it of items) {
    const next = cur === indent ? cur + it : cur + " " + it;
    if (next.length > kMaxLineLen && cur !== indent) {
      lines.push(cur);
      cur = indent + it;
    } else {
      cur = next;
    }
  }
  if (cur !== indent) {
    lines.push(cur);
  }
  return lines;
}

function hex(n: number): string {
  return "0x" + n.toString(16);
}

const out: string[] = [];
out.push("// clang-format off");
out.push(`// tag code -> name, indexed by the code; "\\x01" marks a code with no tag`);
out.push("static SeqStrings gLitHtmlTags =");
out.push(...seqStringsLines(htmlTags, "    "));
out.push("");
out.push("static SeqStrings gLitOpfTags =");
out.push(...seqStringsLines(opfTags, "    "));
out.push("");
out.push("// Every attribute list packed into two parallel arrays: entry i pairs the");
out.push("// code gLitAttrCodes[i] with the i-th string of gLitAttrNames. Identical");
out.push("// lists are stored once and shared.");
out.push("static SeqStrings gLitAttrNames =");
out.push(...seqStringsLines(poolNames, "    "));
out.push("");
out.push(`static const u16 gLitAttrCodes[${poolCodes.length}] = {`);
out.push(
  ...wrapItems(
    poolCodes.map((c) => hex(c) + ","),
    "    ",
  ),
);
out.push("};");
out.push("");
out.push("// an attribute list: a slice of the two entry arrays above");
out.push("struct LitAttrSpan {");
out.push("    u16 first; // index into gLitAttrCodes / gLitAttrNames");
out.push("    u16 n;");
out.push("};");
out.push("");
out.push("// per-tag attribute lists, indexed by the html tag code");
out.push(`static const LitAttrSpan gLitHtmlTagAttrs[${nHtmlTags}] = {`);
out.push(
  ...wrapItems(
    htmlSpans.map(([first, n]) => `{${first}, ${n}},`),
    "    ",
  ),
);
out.push("};");
out.push("");
out.push("// attributes shared by all html tags, tried when the per-tag list has no match");
out.push(`static const LitAttrSpan gLitHtmlAttrs = {${htmlFallbackSpan[0]}, ${htmlFallbackSpan[1]}};`);
out.push("");
out.push(`static const LitAttrSpan gLitOpfAttrs = {${opfSpan[0]}, ${opfSpan[1]}};`);
out.push("// clang-format on");

const startMarker = "// @gen-start litdoc-maps";
const endMarker = "// @gen-end litdoc-maps";
const dst = join(import.meta.dir, "..", "src", "LitDoc.cpp");
const content = readFileSync(dst, "utf8");
const startIdx = content.indexOf(startMarker);
const endIdx = content.indexOf(endMarker);
if (startIdx < 0 || endIdx < 0 || endIdx < startIdx) {
  throw new Error(`could not find '${startMarker}' / '${endMarker}' markers in ${dst}`);
}
const before = content.substring(0, startIdx + startMarker.length);
const after = content.substring(endIdx);
writeFileSync(dst, before + "\n" + out.join("\n") + "\n" + after);
const nEntries = poolCodes.length;
let nRaw = htmlAttrs.length + opfAttrs.length;
for (const list of Object.values(htmlTagAttrs)) {
  nRaw += list.length;
}
console.log(`wrote ${dst}: ${nEntries} pooled attr entries (${nRaw} before dedup)`);
