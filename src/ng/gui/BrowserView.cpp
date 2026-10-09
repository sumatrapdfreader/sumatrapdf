/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's gui/win/BrowserDocView.cpp on a gpui WebView (wry). What is the
// same: the virtual host the pages are served from, the resource provider that
// asks the model for a url's bytes, the navigation callbacks, the scroll
// position reported back from the page, the zoom percent and the in-page find
// scripts. What differs is the plumbing: wry takes a custom protocol instead of
// a WebView2 resource filter, and the notifications come back through
// chrome.webview.postMessage instead of orig's JS bridge object.

#include "gui/GpuiBridge.h"

#include "base/GuessFileType.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "AppTools.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/BrowserView.h"

#include "SumatraLog.h"

// wry serves a custom protocol "x" from "<http|https>://x." (WorkAroundUriPrefix),
// so this name plus useHttpsScheme gives orig's "https://sumatrapdf.chm/" and
// "https://sumatrapdf.markdown/" virtual hosts unchanged
static const char* kProtocolName = "sumatrapdf";
static const char* kProtocolPrefix = "sumatrapdf://";
static const char* kHostPrefix = "https://sumatrapdf.";
constexpr const char* kDefaultVirtualHost = "https://sumatrapdf.chm/";

// Reports the document scroll position back to the host so GetScrollPos() can
// answer synchronously (script eval is async and can't return a value). Hash
// navigation often never fires a 'scroll' event, which would leave the position
// at (-1,-1), so we post immediately, on scroll/hashchange/load/resize, and
// again shortly after.
// ng: orig posts a JSON envelope its own bridge object understands; gpui's
// ipcHandler hands us the raw string, so the payload is a plain line.
static const char* kReportScrollJs = R"JS((function(){
  var post = function() {
    try {
      var wv = window.chrome && window.chrome.webview;
      if (!wv) { return; }
      var x = Math.round(window.scrollX || window.pageXOffset || 0);
      var y = Math.round(window.scrollY || window.pageYOffset || 0);
      wv.postMessage("scroll " + x + " " + y);
    } catch (e) {}
  };
  if (!window.__sumatraScrollHooked) {
    window.__sumatraScrollHooked = true;
    window.addEventListener("scroll", post, true);
    window.addEventListener("hashchange", post);
    window.addEventListener("load", post);
    window.addEventListener("resize", post);
  }
  post();
  setTimeout(post, 0);
  setTimeout(post, 50);
})();)JS";

// ng: the one piece of orig's JS bridge (WebView.cpp kJsBridgeScript) a page
// uses here: window.__sumatra__.notify(method, ...args). Posted as the line
// "notify <method> <params as a JSON array>".
static const char* kJsNotifyJs = R"JS((function(){
  if (window.__sumatra__) { return; }
  var S = {};
  S.notify = function(method) {
    var params = Array.prototype.slice.call(arguments, 1);
    try { window.chrome.webview.postMessage("notify " + method + " " + JSON.stringify(params)); } catch (e) {}
  };
  window.__sumatra__ = S;
})();)JS";

// Injected on every navigation. In-page find driven by the host's own find UI:
// searches the *rendered* DOM text (so what we find is exactly what's shown)
// and highlights matches with the CSS Custom Highlight API, which doesn't
// mutate the DOM. Matches can span text-node boundaries: the text nodes are
// concatenated into one string, the search runs over that, and global offsets
// are mapped back to (node, offset) pairs for the Ranges.
//
// searchAll() additionally sweeps every page of a multi-page document: it
// fetches each page's HTML from the virtual host and parses it with DOMParser,
// so all pages go through the *same* text extraction and matching code as the
// visible page and match counts/indices stay aligned with what start()
// highlights when that page is shown.
//
// Notifications posted back to the host (gen is an echo of the generation the
// host passed in, so it can drop results of a superseded search):
//   findResult <gen> <current> <total>    current 1-based match on this page
//   findAllResult <gen> <total> <recs>    recs: page US idx US snippet, RS-joined
static const char* kFindInPageJs = R"JS((function(){
if (window.__sumatraFind) { return; }
var matches = [];
var cur = -1;
var curHl = null;
var gen = 0;
var styleDone = false;
var kMaxMatches = 5000;
function send(s) {
  try { window.chrome.webview.postMessage(s); } catch (e) {}
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
  send("findResult " + gen + " " + (cur + 1) + " " + matches.length);
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
    send("findAllResult " + g + " " + recs.length + " " + recs.join("\x1e"));
  });
}
window.__sumatraFind = { start: start, gotoMatch: gotoMatch, searchAll: searchAll, clear: clearHighlights };
})();)JS";

struct BrowserView {
    MainWindow* win = nullptr;
    BrowserViewCallback* cb = nullptr;
    Str virtualHost; // owned
    gp::App* app = nullptr;
    gp::Entity<gp::WebView> view;
    bool created = false;
    // the element asked for a view; it is made from the shell's tick, not
    // during the frame (see BrowserViewCreatePending)
    bool wantCreate = false;
    // WebView2's create runs a nested message loop. A control request in that
    // loop makes the first WebViewNew fail, so the tick tries a few times.
    int createTries = 0;
    // drawn in a tool window, which makes it from its own tick
    bool ownHost = false;
    bool visible = false;
    int zoomPercent = 100;
    // last scroll position the page reported, in CSS pixels; (-1, -1) means
    // "not known yet"
    Point scrollPos = Point(-1, -1);
    // navigation asked for before the webview existed
    Str pendingUrl;   // owned
    Vec<Str> history; // owned
    int historyIdx = -1;
    int historyDelta = 0;
};

static Vec<BrowserView*> gBrowserViews;

bool BrowserViewAvailable() {
    return wry::WebViewAvailable();
}

static wry::WebView* Raw(BrowserView* bv) {
    if (!bv || !bv->app || !bv->view.IsValid()) {
        return nullptr;
    }
    gp::WebView* wv = bv->view.Get(bv->app);
    return wv ? gp::WebViewRaw(wv) : nullptr;
}

static void Eval(BrowserView* bv, Str js) {
    wry::WebView* raw = Raw(bv);
    if (raw) {
        wry::WebViewEval(raw, ToGpui(js));
    }
}

// --- the served resources ---------------------------------------------------

static TempStr ChmMimeFromPathTemp(Str path, Str data) {
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

// wry hands the handler the url in its "<protocol>://" form; put it back into
// the "https://<protocol>." form the model's urls use
static TempStr FromProtocolUrlTemp(Str uri) {
    Str prefix(kProtocolPrefix);
    if (!str::StartsWith(uri, prefix)) {
        return str::DupTemp(uri);
    }
    Str rest = Str(uri.s + prefix.len, uri.len - prefix.len);
    return str::JoinTemp(Str(kHostPrefix), rest);
}

// orig's UriPathFromPrefix: the url with the virtual host taken off
static TempStr UriPathFromPrefixTemp(Str uri, Str prefix, bool keepQueryAndFragment) {
    if (len(uri) == 0 || len(prefix) == 0 || !str::StartsWith(uri, prefix)) {
        return {};
    }
    int pathOff = prefix.len;
    while (pathOff < uri.len && uri.s[pathOff] == '/') {
        pathOff++;
    }
    if (pathOff >= uri.len) {
        return {};
    }
    Str path = Str(uri.s + pathOff, uri.len - pathOff);
    if (!keepQueryAndFragment) {
        int q = str::IndexOfChar(path, '?');
        if (q >= 0) {
            path = Str(path.s, q);
        }
        int h = str::IndexOfChar(path, '#');
        if (h >= 0) {
            path = Str(path.s, h);
        }
    }
    return str::DupTemp(path);
}

static void RespondWith(wry::RequestResponder* responder, Str data, Str contentType, int status) {
    wry::Header header;
    header.name = GStrL("Content-Type");
    header.value = ToGpui(contentType);
    wry::Response res;
    res.status = status;
    res.headers = &header;
    res.headerCount = 1;
    res.body = (const u8*)data.s;
    res.bodyLen = data.len;
    wry::Respond(responder, &res);
}

static void OnResourceRequest(void* ctx, gp::Str, const wry::Request* request, wry::RequestResponder* responder) {
    auto* bv = (BrowserView*)ctx;
    Str notFound = StrL("not found");
    if (!bv || !bv->cb || !request) {
        RespondWith(responder, notFound, StrL("text/plain"), 404);
        return;
    }
    TempStr uri = FromProtocolUrlTemp(FromGpui(request->uri));
    TempStr path = UriPathFromPrefixTemp(uri, bv->virtualHost, false);
    Str data = len(path) > 0 ? bv->cb->GetDataForUrl(path) : Str();
    if (len(data) == 0) {
        RespondWith(responder, notFound, StrL("text/plain"), 404);
        return;
    }
    RespondWith(responder, data, ChmMimeFromPathTemp(path, data), 200);
}

// orig's UrlForWebViewEvent: an in-document url reaches the model with the
// virtual host stripped, but with its ?query / #fragment kept
static TempStr UrlForEventTemp(BrowserView* bv, Str uri) {
    if (len(uri) == 0) {
        return {};
    }
    if (str::StartsWith(uri, bv->virtualHost)) {
        return UriPathFromPrefixTemp(uri, bv->virtualHost, true);
    }
    return str::DupTemp(uri);
}

static void NoteHistoryNavigation(BrowserView* bv, Str url) {
    int target = bv->historyIdx + bv->historyDelta;
    if (bv->historyDelta != 0 && target >= 0 && target < len(bv->history)) {
        bv->historyIdx = target;
        bv->historyDelta = 0;
        return;
    }
    bv->historyDelta = 0;
    if (bv->historyIdx >= 0 && str::Eq(bv->history[bv->historyIdx], url)) {
        return;
    }
    if (bv->historyIdx > 0 && str::Eq(bv->history[bv->historyIdx - 1], url)) {
        bv->historyIdx--;
        return;
    }
    if (bv->historyIdx + 1 < len(bv->history) && str::Eq(bv->history[bv->historyIdx + 1], url)) {
        bv->historyIdx++;
        return;
    }
    while (len(bv->history) > bv->historyIdx + 1) {
        str::Free(VecPop(bv->history));
    }
    VecAppend(bv->history, str::Dup(url));
    bv->historyIdx++;
}

static bool OnNavigationStarting(void* ctx, gp::Str url) {
    auto* bv = (BrowserView*)ctx;
    if (!bv || !bv->cb) {
        return true;
    }
    TempStr u = UrlForEventTemp(bv, FromGpui(url));
    if (len(u) == 0) {
        return true;
    }
    bool allow = bv->cb->OnBeforeNavigate(u, false);
    // drop the previous page's position so GetScrollPos() does not keep a stale
    // y while the new document (or hash) is still applying
    if (allow) {
        bv->scrollPos = Point(-1, -1);
    } else {
        bv->historyDelta = 0;
    }
    return allow;
}

static wry::NewWindowResponse OnNewWindow(void* ctx, gp::Str url, const wry::NewWindowFeatures*, wry::WebView**) {
    auto* bv = (BrowserView*)ctx;
    if (bv && bv->cb) {
        TempStr u = UrlForEventTemp(bv, FromGpui(url));
        bv->cb->OnBeforeNavigate(u, true);
    }
    return wry::NewWindowResponse::Deny;
}

static void OnPageLoad(void* ctx, wry::PageLoadEvent ev, gp::Str url) {
    auto* bv = (BrowserView*)ctx;
    if (!bv || !bv->cb || ev != wry::PageLoadEvent::Finished) {
        return;
    }
    TempStr u = UrlForEventTemp(bv, FromGpui(url));
    if (len(u) == 0) {
        return;
    }
    NoteHistoryNavigation(bv, u);
    bv->cb->OnDocumentComplete(u);
    // after zoom / restore have applied: the init scripts may have missed this
    // document, and hash navigation often never fires a 'scroll' event
    Eval(bv, Str(kReportScrollJs));
    AppShellInvalidate(bv->win);
}

// the notifications kReportScrollJs / kFindInPageJs post back
static void OnIpcMessage(void* ctx, gp::Str, gp::Str body) {
    auto* bv = (BrowserView*)ctx;
    if (!bv) {
        return;
    }
    Str msg = FromGpui(body);
    int x = 0, y = 0;
    if (!str::IsNull(str::Parse(msg, "scroll %d %d", &x, &y))) {
        bv->scrollPos = Point(std::max(x, 0), std::max(y, 0));
        return;
    }
    int gen = 0, current = 0, total = 0;
    if (!str::IsNull(str::Parse(msg, "findResult %d %d %d", &gen, &current, &total))) {
        if (bv->cb) {
            bv->cb->OnFindResult(gen, current, total);
        }
        return;
    }
    Str payload = msg;
    if (str::TrimPrefix(payload, StrL("findAllResult "))) {
        if (bv->cb) {
            bv->cb->OnFindAllResult(payload);
        }
        return;
    }
    if (str::TrimPrefix(payload, StrL("notify "))) {
        int at = str::IndexOfChar(payload, ' ');
        if (at > 0 && bv->cb) {
            bv->cb->OnJsNotify(Str(payload.s, at), Str(payload.s + at + 1, payload.len - at - 1));
        }
        return;
    }
    logf("BrowserView: unhandled page message '%s'\n", msg);
}

// --- lifetime ---------------------------------------------------------------

BrowserView* BrowserViewCreate(MainWindow* win, HWND, BrowserViewCallback* cb, Str virtualHost) {
    if (!win || !cb) {
        return nullptr;
    }
    auto* bv = new BrowserView();
    bv->win = win;
    bv->cb = cb;
    bv->virtualHost = str::Dup(len(virtualHost) > 0 ? virtualHost : Str(kDefaultVirtualHost));
    VecAppend(gBrowserViews, bv);
    return bv;
}

void BrowserViewDelete(BrowserView* bv) {
    if (!bv) {
        return;
    }
    VecRemove(gBrowserViews, bv);
    if (bv->app && bv->view.IsValid()) {
        gp::EntityDrop(bv->app, bv->view.id);
    }
    str::Free(bv->virtualHost);
    str::Free(bv->pendingUrl);
    for (Str url : bv->history) {
        str::Free(url);
    }
    delete bv;
}

MainWindow* BrowserViewWindow(BrowserView* bv) {
    return bv ? bv->win : nullptr;
}

void BrowserViewSetWindow(BrowserView* bv, MainWindow* win) {
    if (bv) {
        bv->win = win;
    }
}

void BrowserViewSetVisible(BrowserView* bv, bool visible) {
    if (!bv || bv->visible == visible) {
        return;
    }
    bv->visible = visible;
    if (!bv->app || !bv->view.IsValid()) {
        return;
    }
    gp::WebView* wv = bv->view.Get(bv->app);
    if (!wv) {
        return;
    }
    if (visible) {
        gp::WebViewShow(wv);
    } else {
        gp::WebViewHide(wv);
    }
}

bool BrowserViewIsVisible(BrowserView* bv) {
    return bv && bv->visible;
}

void BrowserViewRefreshSurface(BrowserView*) {}

// --- navigation -------------------------------------------------------------

static TempStr FullUrlTemp(BrowserView* bv, Str url) {
    if (str::StartsWith(url, bv->virtualHost)) {
        return str::DupTemp(url);
    }
    Str rel = url;
    while (len(rel) > 0 && rel.s[0] == '/') {
        rel = Str(rel.s + 1, rel.len - 1);
    }
    return str::JoinTemp(bv->virtualHost, rel);
}

void BrowserViewNavigate(BrowserView* bv, Str url) {
    if (!bv || len(url) == 0) {
        return;
    }
    TempStr fullUrl = FullUrlTemp(bv, url);
    wry::WebView* raw = Raw(bv);
    if (!raw) {
        // the webview is made in the first frame that shows it; go there then
        str::ReplaceWithCopy(&bv->pendingUrl, fullUrl);
        AppShellInvalidate(bv->win);
        return;
    }
    wry::WebViewLoadUrl(raw, ToGpui(fullUrl));
}

void BrowserViewGoBack(BrowserView* bv) {
    if (!BrowserViewCanGoBack(bv)) {
        return;
    }
    bv->historyDelta = -1;
    Eval(bv, StrL("history.back();"));
}

void BrowserViewGoForward(BrowserView* bv) {
    if (!BrowserViewCanGoForward(bv)) {
        return;
    }
    bv->historyDelta = 1;
    Eval(bv, StrL("history.forward();"));
}

bool BrowserViewCanGoBack(BrowserView* bv) {
    return bv && Raw(bv) && bv->historyIdx > 0;
}

bool BrowserViewCanGoForward(BrowserView* bv) {
    return bv && Raw(bv) && bv->historyIdx + 1 < len(bv->history);
}

void BrowserViewSetZoomPercent(BrowserView* bv, int zoom) {
    if (!bv) {
        return;
    }
    bv->zoomPercent = zoom;
    wry::WebView* raw = Raw(bv);
    if (raw) {
        wry::WebViewZoom(raw, (double)zoom / 100.0);
    }
}

int BrowserViewGetZoomPercent(BrowserView* bv) {
    return bv ? bv->zoomPercent : 100;
}

Point BrowserViewGetScrollPos(BrowserView* bv) {
    return bv ? bv->scrollPos : Point(-1, -1);
}

void BrowserViewSetScrollPos(BrowserView* bv, Point pos) {
    if (!bv || (pos.x < 0 && pos.y < 0)) {
        return;
    }
    pos.x = std::max(pos.x, 0);
    pos.y = std::max(pos.y, 0);
    Eval(bv, fmt("window.scrollTo(%d, %d);", pos.x, pos.y));
    bv->scrollPos = pos;
}

void BrowserViewEval(BrowserView* bv, Str js) {
    Eval(bv, js);
}

void BrowserViewSelectAll(BrowserView* bv) {
    Eval(bv, StrL("document.execCommand('selectAll', false, null)"));
}

void BrowserViewCopySelection(BrowserView* bv) {
    Eval(bv, StrL("document.execCommand('copy', false, null)"));
}

void BrowserViewPrint(BrowserView* bv, bool) {
    Eval(bv, StrL("window.print()"));
}

void BrowserViewFindInPageUI(BrowserView* bv) {
    // ng: wry exposes no ShowFindUI; the application's own find bar drives
    // BrowserViewFindStart() instead
    BrowserViewFindStart(bv, {}, false, false, 0, -1);
}

// --- in-page find -----------------------------------------------------------

bool BrowserViewCanFindInPage(BrowserView* bv) {
    return Raw(bv) != nullptr;
}

// escape s for use inside a single-quoted JS string literal
static TempStr JsEscapeTemp(Str s) {
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

// highlight matches of term on the current page; gotoIdx >= 0 makes that match
// current (used after navigating to a match on another page), -1 picks the
// first match at/below the current scroll position
void BrowserViewFindStart(BrowserView* bv, Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx) {
    if (!BrowserViewCanFindInPage(bv)) {
        return;
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    // no {} in the js passed to fmt(): it would parse them as positional args
    TempStr js = fmt("window.__sumatraFind && __sumatraFind.start('%s', %s, %s, %d, %d);", esc,
                     matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen, gotoIdx);
    Eval(bv, js);
}

// search all pages of the document (pageUrls in page order); the match list
// comes back asynchronously as one findAllResult notification
void BrowserViewFindAllPages(BrowserView* bv, const StrVec& pageUrls, Str term, bool matchCase, bool wholeWord,
                             int gen) {
    if (!BrowserViewCanFindInPage(bv)) {
        return;
    }
    str::Builder js;
    js.Append(StrL("window.__sumatraFind && __sumatraFind.searchAll(["));
    int n = len(pageUrls);
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            js.AppendChar(',');
        }
        // page urls may be internal document paths (e.g. chm pages); prefix the
        // virtual host like BrowserViewNavigate() does to make them fetchable
        TempStr fullUrl = FullUrlTemp(bv, pageUrls.At(i));
        js.AppendChar('\'');
        js.Append(JsEscapeTemp(fullUrl));
        js.AppendChar('\'');
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    js.Append(fmt("], '%s', %s, %s, %d);", esc, matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen));
    Eval(bv, ToStrTemp(js));
}

// jump to the idx-th match on the current page (as counted by FindStart)
void BrowserViewFindGoto(BrowserView* bv, int idx) {
    if (!BrowserViewCanFindInPage(bv)) {
        return;
    }
    Eval(bv, fmt("window.__sumatraFind && __sumatraFind.gotoMatch(%d);", idx));
}

void BrowserViewFindClear(BrowserView* bv) {
    if (!BrowserViewCanFindInPage(bv)) {
        return;
    }
    Eval(bv, StrL("window.__sumatraFind && __sumatraFind.clear();"));
}

// Chromium ignores WM_MOUSEWHEEL on the host window. A wheel that lands on the
// frame (the browser child is not focused) scrolls the page from here.
LRESULT BrowserViewPassUIMsg(BrowserView* bv, UINT msg, WPARAM wp, LPARAM) {
#if OS_WIN
    if (!bv || !Raw(bv)) {
        return 0;
    }
    if (msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL) {
        return 0;
    }
    if ((LOWORD(wp) & MK_CONTROL) || IsCtrlPressed()) {
        return 0;
    }
    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    bool horiz = (msg == WM_MOUSEHWHEEL) || (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    int d = -(int)delta;
    Eval(bv, horiz ? fmt("window.scrollBy(%d, 0)", d) : fmt("window.scrollBy(0, %d)", d));
#else
    (void)bv;
    (void)msg;
    (void)wp;
#endif
    return 0;
}

// --- the element ------------------------------------------------------------

// WebView2 creation pumps the thread queue. A paint or another create from
// that pump fails the one in progress, so both are held off until it returns.
static int gWebViewCreateDepth = 0;

bool BrowserViewCreateInProgress() {
    return gWebViewCreateDepth > 0;
}

static void CreateWebView(BrowserView* bv, gp::Ctx* cx) {
    if (gWebViewCreateDepth > 0) {
        return;
    }
    bv->app = cx->app;
    bv->createTries++;

    wry::InitializationScript scripts[3];
    scripts[0].script = ToGpui(Str(kReportScrollJs));
    scripts[1].script = ToGpui(Str(kFindInPageJs));
    scripts[2].script = ToGpui(Str(kJsNotifyJs));

    wry::CustomProtocol protocol;
    protocol.name = ToGpui(Str(kProtocolName));
    protocol.ctx = bv;
    protocol.handler = OnResourceRequest;

    wry::WebViewAttributes attrs;
    attrs.id = ToGpui(Str(kProtocolName));
    // orig's virtual hosts are https:// urls, and this is what makes wry serve
    // the custom protocol from "https://sumatrapdf." instead of "http://..."
    attrs.useHttpsScheme = true;
    attrs.customProtocols = &protocol;
    attrs.customProtocolCount = 1;
    attrs.initializationScripts = scripts;
    attrs.initializationScriptCount = 3;
    attrs.ctx = bv;
    attrs.navigationHandler = OnNavigationStarting;
    attrs.onPageLoadHandler = OnPageLoad;
    attrs.newWindowReqHandler = OnNewWindow;
    attrs.ipcHandler = OnIpcMessage;
    attrs.hasBackgroundColor = true;
    attrs.backgroundColor = wry::Rgba{0xff, 0xff, 0xff, 0xff};
    // keep the keyboard with the application until the user clicks the page
    attrs.focused = false;
    // gpui zeroes the bounds and positions the view from its element's bounds
    // on the first paint; without this wry would make a CW_USEDEFAULT child
    // that covers the whole window until then
    attrs.hasBounds = true;
#if OS_WIN
    TempStr dataDir = GetWebViewDataDirTemp();
    if (len(dataDir) > 0) {
        attrs.dataDirectory = ToGpui(dataDir);
    }
#endif
    if (len(bv->pendingUrl) > 0) {
        attrs.url = ToGpui(bv->pendingUrl);
    }

    gWebViewCreateDepth++;
    bv->view = gp::WebViewNew(cx, &attrs);
    gWebViewCreateDepth--;
    gp::WebView* wv = bv->view.Get(cx->app);
    if (!wv || !gp::WebViewRaw(wv)) {
        bv->view = {};
        // give up and show the missing-runtime message
        if (bv->createTries >= 3) {
            bv->created = true;
            logf("BrowserView: could not create a webview (WebView2 runtime missing?)\n");
        }
        return;
    }
    bv->created = true;
    // ng: gpui always makes the view 0 x 0 and sizes it from its element's
    // bounds on the first paint. A WebView2 that has never had a size does not
    // start loading, so give it the canvas rect right away
    Rect rc = bv->win->canvasRc;
    if (bv->ownHost) {
        gp::WinSize ws = gp::WindowSize(cx->win);
        rc = Rect(0, 0, (int)ws.dipW, (int)ws.dipH);
    }
    if (rc.dx > 0 && rc.dy > 0) {
        wry::Rect r;
        r.position = wry::LogicalPosition(rc.x, rc.y);
        r.size = wry::LogicalSize(rc.dx, rc.dy);
        wry::WebViewSetBounds(gp::WebViewRaw(wv), r);
    }
    str::FreePtr(&bv->pendingUrl);
    if (bv->zoomPercent != 100) {
        wry::WebViewZoom(gp::WebViewRaw(wv), (double)bv->zoomPercent / 100.0);
    }
    logf("BrowserView: webview created, host '%s'\n", bv->virtualHost);
}

// ng: a WebView2 is created with a nested message loop, which must not run
// inside gpui's paint (the window never stops repainting if it does), so the
// shell's tick makes the pending views instead of BrowserViewBuild()
void BrowserViewCreatePending(MainWindow* win, gp::Ctx* cx) {
    for (BrowserView* bv : gBrowserViews) {
        if (bv->win == win && bv->wantCreate && !bv->created && !bv->ownHost) {
            CreateWebView(bv, cx);
            AppShellInvalidate(win);
        }
    }
}

void BrowserViewSetOwnHost(BrowserView* bv) {
    if (bv) {
        bv->ownHost = true;
    }
}

void BrowserViewCreatePendingIn(BrowserView* bv, gp::Ctx* cx) {
    if (!bv || !bv->ownHost || !bv->wantCreate || bv->created) {
        return;
    }
    CreateWebView(bv, cx);
    AppShellInvalidate(bv->win);
}

gp::El* BrowserViewBuild(BrowserView* bv, gp::Ctx* cx) {
    if (!bv) {
        return nullptr;
    }
    if (!bv->created) {
        bv->wantCreate = true;
        const gp::Theme& th = gp::ThemeNow(cx->app);
        return gp::Div(cx->a)->SizeFull()->Bg(th.tokens.muted);
    }
    if (!bv->view.IsValid()) {
        const gp::Theme& th = gp::ThemeNow(cx->app);
        return gp::Div(cx->a)
            ->FlexCol()
            ->SizeFull()
            ->ItemsCenter()
            ->JustifyCenter()
            ->Bg(th.tokens.muted)
            ->Child(gp::TextEl(cx->a, GStrL("No embedded browser (WebView2) is available"))->Font(14)->Fg(th.mutedFg));
    }
    if (len(bv->pendingUrl) > 0) {
        wry::WebView* raw = Raw(bv);
        if (raw) {
            wry::WebViewLoadUrl(raw, ToGpui(bv->pendingUrl));
            str::FreePtr(&bv->pendingUrl);
        }
    }
    return gp::WebViewEl(bv->view, cx);
}
