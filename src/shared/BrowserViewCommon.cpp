/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"

#include "BrowserViewCommon.h"

// Small helpers of the browser-backed document view that orig's
// gui/win/BrowserDocView.cpp and ng's gui/BrowserView.cpp share.

TempStr ChmMimeFromPathTemp(Str path, Str data) {
    Str ext = str::SliceFromCharLast(path, '.');
    if (str::ContainsChar(ext, ';')) {
        Str semi = str::SliceFromChar(ext, ';');
        TempStr trimmed = str::DupTemp(Str(path.s, (int)(semi.s - path.s)));
        return ChmMimeFromPathTemp(trimmed, data);
    }

    TempStr imgExt = GfxFileExtFromDataTemp(data);
    TempStr mime = MimeTypeFromExtTemp(ext, imgExt);
    if (len(mime) == 0) {
        mime = StrL("text/html");
    }
    return mime;
}

// escape s for use inside a single-quoted JS string literal
TempStr JsEscapeTemp(Str s) {
    str::Builder buf;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        switch (c) {
            case '\\':
                buf.Append(StrL("\\\\"));
                break;
            case '\'':
                buf.Append(StrL("\\'"));
                break;
            case '\n':
                buf.Append(StrL("\\n"));
                break;
            case '\r':
                buf.Append(StrL("\\r"));
                break;
            case '\t':
                buf.Append(StrL("\\t"));
                break;
            default:
                buf.AppendChar(c);
                break;
        }
    }
    return ToStrTemp(buf);
}

// Injected on every navigation of a browser view. In-page find driven by the host's
// own find UI: searches the *rendered* DOM text (so what we find is exactly
// what's shown) and highlights matches with the CSS Custom Highlight API,
// which doesn't mutate the DOM. Matches can span text-node boundaries: the
// text nodes are concatenated into one string, the search runs over that, and
// global offsets are mapped back to (node, offset) pairs for the Ranges.
//
// searchAll() additionally sweeps every page of a multi-page document: it
// fetches each page's HTML from the virtual host and parses it with
// DOMParser, so all pages go through the *same* text extraction and matching
// code as the visible page and match counts/indices stay aligned with what
// start() highlights when that page is shown.
//
// Results go to window.__sumatraFindNotify(method, ...args), which each app
// defines (gen is an echo of the generation the host passed in, so it can drop
// results of a superseded search):
//   findResult(gen, current, total)   current 1-based match on this page
//   findAllResult(gen, total, recs)   recs: page US idx US snippet, RS-joined
const char* kFindInPageJs = R"JS((function(){
if (window.__sumatraFind) { return; }
var matches = [];
var cur = -1;
var curHl = null;
var gen = 0;
var styleDone = false;
var kMaxMatches = 5000;
function notifyHost() {
  try { window.__sumatraFindNotify.apply(null, arguments); } catch (e) {}
}
function ensureStyle() {
  if (styleDone) { return; }
  styleDone = true;
  try {
    var ss = new CSSStyleSheet();
    ss.replaceSync("::highlight(sumatra-find){background-color:#ffee70;color:#000;} ::highlight(sumatra-find-cur){background-color:#ff9632;color:#000;}");
    document.adoptedStyleSheets = document.adoptedStyleSheets.concat([ss]);
  } catch (e) {}
}
function post() {
  notifyHost("findResult", gen, cur + 1, matches.length);
}
function isWordChar(c) {
  try { return /[\p{L}\p{N}_]/u.test(c); } catch (e) { return /\w/.test(c); }
}
// concatenated text of all visible-ish text nodes under body, with per-node
// start offsets (also used for documents parsed by DOMParser, which have no
// layout, so no computed-style checks here to keep both paths identical)
function textFromBody(body) {
  var nodes = [];
  var starts = [];
  var text = "";
  var walker = body.ownerDocument.createTreeWalker(body, NodeFilter.SHOW_TEXT, {
    acceptNode: function(n) {
      var p = n.parentElement;
      if (!p) { return NodeFilter.FILTER_REJECT; }
      var tag = p.tagName;
      if (tag === "SCRIPT" || tag === "STYLE" || tag === "NOSCRIPT" || tag === "TEXTAREA") { return NodeFilter.FILTER_REJECT; }
      return NodeFilter.FILTER_ACCEPT;
    }
  });
  var n;
  while ((n = walker.nextNode())) { nodes.push(n); starts.push(text.length); text += n.data; }
  return { nodes: nodes, starts: starts, text: text };
}
// [start, end) offsets of every match of term in text. Literal search: regex
// metacharacters are escaped; a regex is used only for its case-insensitive
// mode (which unlike toLowerCase() can't shift offsets)
function findInText(text, term, matchCase, wholeWord) {
  var out = [];
  var esc = term.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  var re;
  try { re = new RegExp(esc, matchCase ? "g" : "gi"); } catch (e) { return out; }
  var m;
  while ((m = re.exec(text)) !== null) {
    if (m.index === re.lastIndex) { re.lastIndex++; continue; }
    if (wholeWord) {
      var b = m.index > 0 ? text[m.index - 1] : " ";
      var a = re.lastIndex < text.length ? text[re.lastIndex] : " ";
      if (isWordChar(b) || isWordChar(a)) { continue; }
    }
    out.push([m.index, m.index + m[0].length]);
    if (out.length >= kMaxMatches) { break; }
  }
  return out;
}
function makeSnippet(text, s, e) {
  var from = Math.max(0, s - 40);
  var to = Math.min(text.length, e + 40);
  var sn = text.slice(from, to).replace(/[\x00-\x1f]+/g, " ").replace(/\s+/g, " ").trim();
  if (from > 0) { sn = "..." + sn; }
  if (to < text.length) { sn = sn + "..."; }
  return sn;
}
function clearHighlights() {
  if (window.CSS && CSS.highlights) {
    CSS.highlights.delete("sumatra-find");
    CSS.highlights.delete("sumatra-find-cur");
  }
  matches = [];
  cur = -1;
  curHl = null;
}
function setCur(i, scroll) {
  if (matches.length === 0) { cur = -1; return; }
  cur = ((i % matches.length) + matches.length) % matches.length;
  if (curHl) {
    curHl.clear();
    curHl.add(matches[cur]);
  }
  if (scroll) {
    var r = matches[cur].getBoundingClientRect();
    if (r.top < 0 || r.bottom > window.innerHeight) {
      window.scrollBy(0, r.top - window.innerHeight / 3);
    }
  }
}
// initIdx >= 0: make that match current (used when jumping to a match on a
// freshly loaded page); otherwise the first match at/below the viewport top
function start(term, matchCase, wholeWord, g, initIdx) {
  gen = g;
  clearHighlights();
  ensureStyle();
  if (!term || !window.CSS || !CSS.highlights || !window.Highlight) { post(); return; }
  var t = textFromBody(document.body);
  var found = findInText(t.text, term, matchCase, wholeWord);
  // node for a global offset: last node whose start is <= off; for a match
  // end, < off so an end exactly on a node boundary stays in the prior node
  function locate(off, isEnd) {
    var lo = 0, hi = t.nodes.length - 1, res = 0;
    while (lo <= hi) {
      var mid = (lo + hi) >> 1;
      var ok = isEnd ? (t.starts[mid] < off) : (t.starts[mid] <= off);
      if (ok) { res = mid; lo = mid + 1; } else { hi = mid - 1; }
    }
    return [t.nodes[res], off - t.starts[res]];
  }
  var all = new Highlight();
  for (var j = 0; j < found.length; j++) {
    var r = document.createRange();
    var st = locate(found[j][0], false);
    var en = locate(found[j][1], true);
    try {
      r.setStart(st[0], st[1]);
      r.setEnd(en[0], en[1]);
      matches.push(r);
      all.add(r);
    } catch (e) {}
  }
  CSS.highlights.set("sumatra-find", all);
  curHl = new Highlight();
  CSS.highlights.set("sumatra-find-cur", curHl);
  if (matches.length > 0) {
    var first = 0;
    if (initIdx >= 0) {
      first = Math.min(initIdx, matches.length - 1);
    } else {
      for (var k = 0; k < matches.length; k++) {
        var rc = matches[k].getBoundingClientRect();
        if (rc.bottom >= 0) { first = k; break; }
      }
    }
    setCur(first, true);
  }
  post();
}
function gotoMatch(i) {
  if (matches.length === 0) { post(); return; }
  setCur(i, true);
  post();
}
// search every page of the document (urls in page order); pages are fetched
// sequentially so the match records stay in page order
function searchAll(urls, term, matchCase, wholeWord, g) {
  var recs = [];
  var parser = new DOMParser();
  var chain = Promise.resolve();
  urls.forEach(function(url, pi) {
    chain = chain.then(function() {
      if (recs.length >= kMaxMatches) { return; }
      return fetch(url).then(function(r) { return r.text(); }).then(function(html) {
        var doc = parser.parseFromString(html, "text/html");
        if (!doc.body) { return; }
        var t = textFromBody(doc.body);
        var found = findInText(t.text, term, matchCase, wholeWord);
        for (var i = 0; i < found.length && recs.length < kMaxMatches; i++) {
          recs.push((pi + 1) + "\x1f" + i + "\x1f" + makeSnippet(t.text, found[i][0], found[i][1]));
        }
      }).catch(function() {});
    });
  });
  chain.then(function() {
    notifyHost("findAllResult", g, recs.length, recs.join("\x1e"));
  });
}
window.__sumatraFind = { start: start, gotoMatch: gotoMatch, searchAll: searchAll, clear: clearHighlights };
})();)JS";
