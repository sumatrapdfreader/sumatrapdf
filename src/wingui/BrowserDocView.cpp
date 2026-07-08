/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/HtmlWindow.h"
#include "wingui/WebView.h"
#include "wingui/BrowserDocView.h"

#include "AppTools.h"
#include "Accelerators.h"

constexpr const char* kChmVirtualHost = "https://sumatrapdf.chm/";
constexpr const WCHAR* kChmVirtualHostW = L"https://sumatrapdf.chm/";

// Injected on every WebView2 navigation. Reports the document scroll position
// back to the host on each scroll so BrowserDocView can answer GetScrollPos()
// synchronously (WebView2 script eval is async and can't return a value here).
constexpr const char* kReportScrollJs =
    "(function(){var post=function(){try{var x=Math.round(window.scrollX||window.pageXOffset||0);"
    "var y=Math.round(window.scrollY||window.pageYOffset||0);"
    "window.chrome.webview.postMessage('chmscroll '+x+' '+y);}catch(e){}};"
    "window.addEventListener('scroll',post,true);})();";

// Injected on every WebView2 navigation. In-page find driven by the host's
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
// Messages posted back to the host (gen is an echo of the generation the
// host passed in, so it can drop results of a superseded search):
//   'mdfind <gen> <current> <total>'  current 1-based match on this page
//   'mdfindall <gen> <total> <recs>'  recs: page US idx US snippet, RS-joined
constexpr const char* kFindInPageJs = R"JS((function(){
if (window.__sumatraFind) { return; }
var matches = [];
var cur = -1;
var curHl = null;
var gen = 0;
var styleDone = false;
var kMaxMatches = 5000;
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
  try { window.chrome.webview.postMessage("mdfind " + gen + " " + (cur + 1) + " " + matches.length); } catch (e) {}
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
    try { window.chrome.webview.postMessage("mdfindall " + g + " " + recs.length + " " + recs.join("\x1e")); } catch (e) {}
  });
}
window.__sumatraFind = { start: start, gotoMatch: gotoMatch, searchAll: searchAll, clear: clearHighlights };
})();)JS";

// WebView2 host that captures scroll-position and find-result messages posted
// by kReportScrollJs / kFindInPageJs.
struct BrowserWebviewWnd : WebviewWnd {
    BrowserDocView* owner = nullptr;
    void OnBrowserMessage(Str msg) override;
};

void BrowserWebviewWnd::OnBrowserMessage(Str msg) {
    int x = 0;
    int y = 0;
    if (owner && !str::IsNull(str::Parse(msg, "chmscroll %d %d", &x, &y))) {
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        owner->webviewScrollPos = Point(x, y);
        return;
    }
    // note: "mdfindall" must be tested before "mdfind" (shared prefix)
    constexpr int kMdFindAllPrefixLen = 10; // "mdfindall "
    if (owner && owner->cb && str::StartsWith(msg, "mdfindall ")) {
        Str payload = Str(msg.s + kMdFindAllPrefixLen, msg.len - kMdFindAllPrefixLen);
        owner->cb->OnFindAllResult(payload);
        return;
    }
    int gen = 0;
    if (owner && owner->cb && !str::IsNull(str::Parse(msg, "mdfind %d %d %d", &gen, &x, &y))) {
        owner->cb->OnFindResult(gen, x, y);
        return;
    }
    WebviewWnd::OnBrowserMessage(msg);
}

static TempStr ChmMimeFromPathTemp(Str path, Str data) {
    Str ext = str::SliceFromCharLast(path, '.');
    if (str::ContainsChar(ext, ';')) {
        Str semi = str::SliceFromChar(ext, ';');
        TempStr trimmed = str::DupTemp(Str(path.s, (int)(semi.s - path.s)));
        return ChmMimeFromPathTemp(trimmed, data);
    }

    TempStr imgExt = GfxFileExtFromDataTemp(data);
    TempStr mime = MimeTypeFromExtTemp(ext, imgExt);
    if (!mime) {
        mime = "text/html";
    }
    return mime;
}

bool BrowserDocView::ResourceGet(void* ctx, Str path, WebViewResourceResult* res) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb || !res || len(path) == 0) {
        return false;
    }
    Str data = view->cb->GetDataForUrl(path);
    if (len(data) == 0) {
        return false;
    }
    res->data = (u8*)data.s;
    res->dataLen = (size_t)data.len;
    res->contentType = str::Dup(ChmMimeFromPathTemp(path, data));
    res->ownsData = false;
    return true;
}

bool BrowserDocView::NavigationStarting(void* ctx, Str url, bool newWindow) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb) {
        return false;
    }
    return view->cb->OnBeforeNavigate(url, newWindow);
}

void BrowserDocView::NavigationCompleted(void* ctx, Str url, bool success) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb || !success || len(url) == 0) {
        return;
    }
    // the WebView2 child windows that receive drops are created lazily and can
    // be recreated on navigation, so (re)install our forwarding drop target now
    if (view->wv) {
        view->wv->RegisterForwardingDropTarget();
    }
    view->cb->OnDocumentComplete(url);
}

void BrowserDocView::HistoryChanged(void* ctx, bool canGoBack, bool canGoForward) {
    auto* view = (BrowserDocView*)ctx;
    if (!view) {
        return;
    }
    view->canGoBack = canGoBack;
    view->canGoForward = canGoForward;
}

// Maps a key pressed while the WebView2-hosted CHM has focus to the app command
// to run, so app keyboard shortcuts (Ctrl+F, F1, F2, F3, F5, ...) keep working
// when focus is in the document (issue #5735). Esc isn't an accelerator -- it's
// handled by the frame's key handler -- so forward the key itself.
static int ChmResolveAccelCmd(void*, u16 vk, bool ctrl, bool shift, bool alt) {
    if (vk == VK_ESCAPE) {
        return kWebViewForwardKey;
    }
    return SafeAcceleratorCmd(vk, ctrl, shift, alt);
}

void BrowserDocView::UnsubclassParent() {
    if (!subclassId || !hwndParent) {
        return;
    }
    RemoveWindowSubclass(hwndParent, ParentWndProc, subclassId);
    auto curr = (BrowserDocView*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);
    if (curr == this) {
        SetWindowLongPtr(hwndParent, GWLP_USERDATA, 0);
    }
    subclassId = 0;
}

LRESULT BrowserDocView::ParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    auto* view = reinterpret_cast<BrowserDocView*>(data);
    if (!view) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
            if (view->wv && view->wv->hwnd) {
                Rect rc = ClientRect(hwnd);
                view->wv->SetBounds(rc);
                view->wv->UpdateWebviewSize();
            }
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            if (view->wv && view->wv->hwnd) {
                return 0;
            }
            break;

        case WM_VSCROLL:
            if (view->wv) {
                return view->SendMsg(msg, wp, lp);
            }
            break;

        case WM_PARENTNOTIFY:
            if (LOWORD(wp) == WM_LBUTTONDOWN && view->cb) {
                view->cb->OnLButtonDown();
            }
            break;
    }

    // note: WM_DROPFILES is intentionally not handled here so it passes through
    // to the canvas WndProc, which opens dropped files (forwarded from the
    // WebView2 via ForwardingDropTarget)
    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool BrowserDocView::CreateWebView2() {
    auto* chmWv = new BrowserWebviewWnd();
    chmWv->owner = this;
    wv = chmWv;
    wv->dataDir = str::Dup(GetWebViewDataDirTemp());
    wv->resourceUriPrefix = wstr::Dup(virtualHostW);
    wv->resourceProvider.ctx = this;
    wv->resourceProvider.getResource = ResourceGet;
    wv->events.ctx = this;
    wv->events.navigationStarting = NavigationStarting;
    wv->events.navigationCompleted = NavigationCompleted;
    wv->events.historyChanged = HistoryChanged;
    wv->events.resolveAccelCmd = ChmResolveAccelCmd;
    // forward app accelerators (Ctrl+W close tab, Ctrl+K command palette, etc.)
    // to the main window so they work while the WebView2 has keyboard focus
    wv->forwardAppAccelerators = true;
    // let file drops fall through to the canvas (to open the dropped file)
    // instead of being swallowed by the WebView2 control
    wv->allowExternalDrop = false;

    Rect rc = ClientRect(hwndParent);
    CreateWebViewArgs cargs;
    cargs.parent = hwndParent;
    cargs.pos = rc;
    if (!wv->Create(cargs)) {
        delete wv;
        wv = nullptr;
        return false;
    }

    wv->Init(kReportScrollJs);
    wv->Init(kFindInPageJs);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwndParent, ParentWndProc, subclassId, (DWORD_PTR)this);
    ReportIf(!ok);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
    backend = Backend::WebView2;
    return true;
}

BrowserDocView* BrowserDocView::Create(HWND hwndParent, HtmlWindowCallback* cb, Str virtualHostPrefix) {
    if (!hwndParent || !cb) {
        return nullptr;
    }

    auto* view = new BrowserDocView();
    view->hwndParent = hwndParent;
    view->cb = cb;
    if (virtualHostPrefix) {
        view->virtualHost = str::Dup(virtualHostPrefix);
        view->virtualHostW = wstr::Dup(ToWStrTemp(virtualHostPrefix));
    } else {
        view->virtualHost = str::Dup(kChmVirtualHost);
        view->virtualHostW = wstr::Dup(kChmVirtualHostW);
    }

#ifdef _MSC_VER
    if (HasWebView() && view->CreateWebView2()) {
        return view;
    }
#endif

    view->ie = HtmlWindow::Create(hwndParent, cb);
    if (view->ie) {
        view->backend = Backend::IE;
        return view;
    }

    delete view;
    return nullptr;
}

BrowserDocView::~BrowserDocView() {
    UnsubclassParent();
    delete wv;
    delete ie;
    str::Free(virtualHost);
    wstr::Free(virtualHostW);
}

void BrowserDocView::NavigateToDataUrl(Str url) {
    if (!url) {
        return;
    }
    if (backend == Backend::WebView2 && wv) {
        TempStr fullUrl = url;
        if (!str::StartsWith(url, virtualHost)) {
            fullUrl = str::JoinTemp(virtualHost, url);
        }
        wv->Navigate(fullUrl);
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->NavigateToDataUrl(url);
    }
}

void BrowserDocView::GoBack() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoBack();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoBack();
    }
}

void BrowserDocView::GoForward() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoForward();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoForward();
    }
}

void BrowserDocView::SetZoomPercent(int zoom) {
    zoomPercent = zoom;
    if (backend == Backend::WebView2 && wv) {
        wv->SetZoomPercent(zoom);
        zoomPercent = wv->GetZoomPercent();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SetZoomPercent(zoom);
        zoomPercent = ie->GetZoomPercent();
    }
}

int BrowserDocView::GetZoomPercent() const {
    if (backend == Backend::WebView2 && wv) {
        return wv->GetZoomPercent();
    }
    if (backend == Backend::IE && ie) {
        return ie->GetZoomPercent();
    }
    return zoomPercent;
}

Point BrowserDocView::GetScrollPos() const {
    if (backend == Backend::WebView2 && wv) {
        return webviewScrollPos;
    }
    if (backend == Backend::IE && ie) {
        return ie->GetScrollPos();
    }
    return Point(-1, -1);
}

void BrowserDocView::SetScrollPos(Point pos) {
    if (pos.x < 0 && pos.y < 0) {
        return;
    }
    if (pos.x < 0) {
        pos.x = 0;
    }
    if (pos.y < 0) {
        pos.y = 0;
    }
    if (backend == Backend::WebView2 && wv) {
        TempStr js = fmt("window.scrollTo(%d, %d);", pos.x, pos.y);
        wv->Eval(js);
        webviewScrollPos = pos;
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SetScrollPos(pos);
    }
}

void BrowserDocView::PrintCurrentPage(bool showUI) {
    if (backend == Backend::WebView2 && wv) {
        if (showUI) {
            wv->Eval("window.print()");
        } else {
            wv->Eval("window.print()");
        }
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->PrintCurrentPage(showUI);
    }
}

void BrowserDocView::FindInCurrentPage() {
    if (backend == Backend::WebView2 && wv) {
        // trigger the WebView2 (Chromium) find-on-page bar, like IE's own find
        wv->ShowFindUI();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->FindInCurrentPage();
    }
}

bool BrowserDocView::CanFindInPage() const {
    return backend == Backend::WebView2 && wv != nullptr;
}

// escape s for use inside a single-quoted JS string literal
static TempStr JsEscapeTemp(Str s) {
    str::Builder buf;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        switch (c) {
            case '\\':
                buf.Append("\\\\");
                break;
            case '\'':
                buf.Append("\\'");
                break;
            case '\n':
                buf.Append("\\n");
                break;
            case '\r':
                buf.Append("\\r");
                break;
            case '\t':
                buf.Append("\\t");
                break;
            default:
                buf.AppendChar(c);
                break;
        }
    }
    return ToStrTemp(buf);
}

// highlight matches of term on the current page; gotoIdx >= 0 makes that
// match current (used after navigating to a match on another page), -1 picks
// the first match at/below the current scroll position
void BrowserDocView::FindStart(Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx) {
    if (!CanFindInPage()) {
        return;
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    TempStr js = fmt("window.__sumatraFind && __sumatraFind.start('%s', %s, %s, %d, %d);", esc,
                     matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen, gotoIdx);
    wv->Eval(js);
}

// search all pages of the document (pageUrls in page order); the match list
// is posted back asynchronously as one 'mdfindall' message
void BrowserDocView::FindAllPages(const StrVec& pageUrls, Str term, bool matchCase, bool wholeWord, int gen) {
    if (!CanFindInPage()) {
        return;
    }
    str::Builder js;
    js.Append("window.__sumatraFind && __sumatraFind.searchAll([");
    int n = len(pageUrls);
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            js.AppendChar(',');
        }
        js.AppendChar('\'');
        js.Append(JsEscapeTemp(pageUrls.At(i)));
        js.AppendChar('\'');
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    js.Append(fmt("], '%s', %s, %s, %d);", esc, matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen));
    wv->Eval(ToStrTemp(js));
}

// jump to the idx-th match on the current page (as counted by FindStart)
void BrowserDocView::FindGoto(int idx) {
    if (!CanFindInPage()) {
        return;
    }
    TempStr js = fmt("window.__sumatraFind && __sumatraFind.gotoMatch(%d);", idx);
    wv->Eval(js);
}

void BrowserDocView::FindClear() {
    if (!CanFindInPage()) {
        return;
    }
    wv->Eval("window.__sumatraFind && __sumatraFind.clear();");
}

void BrowserDocView::SelectAll() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval("document.execCommand('selectAll', false, null)");
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SelectAll();
    }
}

void BrowserDocView::CopySelection() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval("document.execCommand('copy', false, null)");
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->CopySelection();
    }
}

LRESULT BrowserDocView::SendMsg(UINT msg, WPARAM wp, LPARAM lp) {
    if (backend == Backend::WebView2 && wv && wv->hwnd) {
        return SendMessageW(wv->hwnd, msg, wp, lp);
    }
    if (backend == Backend::IE && ie) {
        return ie->SendMsg(msg, wp, lp);
    }
    return 0;
}
