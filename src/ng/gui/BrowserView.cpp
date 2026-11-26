/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's gui/win/BrowserDocView.cpp on a gpui WebView (wry). What is the
// same: the virtual host the pages are served from, the resource provider that
// asks the model for a url's bytes, the navigation callbacks, the scroll
// position reported back from the page, the zoom percent and the in-page find
// scripts. What differs is the plumbing: wry takes a custom protocol instead of
// a WebView2 resource filter, and the notifications come back through
// chrome.webview.postMessage (window.ipc on mac and Linux).

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "base/Win.h"
#if OS_WIN
#include "../../../ext/webview2/WebView2.h"
#endif

#include "gui/UIModels.h"

#include "AppTools.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/BrowserView.h"
#include "BrowserViewCommon.h"
#if OS_WIN
#include "WebViewCapture.h"
#endif

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
      var x = Math.round(window.scrollX || window.pageXOffset || 0);
      var y = Math.round(window.scrollY || window.pageYOffset || 0);
      var s = "scroll " + x + " " + y;
      var wv = window.chrome && window.chrome.webview;
      if (wv && wv.postMessage) { wv.postMessage(s); return; }
      if (window.ipc && window.ipc.postMessage) { window.ipc.postMessage(s); }
    } catch (e) {}
  };
  if (!window.__sumatraScrollHooked) {
    window.__sumatraScrollHooked = true;
    window.addEventListener("scroll", post, true);
    window.addEventListener("hashchange", post);
    window.addEventListener("load", post);
    window.addEventListener("resize", post);
  }
  try {
    var h = location.hash;
    if (h && h.length > 1) {
      var el = document.getElementById(decodeURIComponent(h.slice(1)));
      if (el) { el.scrollIntoView(); }
    }
  } catch (e2) {}
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
    try {
      var s = "notify " + method + " " + JSON.stringify(params);
      var wv = window.chrome && window.chrome.webview;
      if (wv && wv.postMessage) { wv.postMessage(s); return; }
      if (window.ipc && window.ipc.postMessage) { window.ipc.postMessage(s); }
    } catch (e) {}
  };
  window.__sumatra__ = S;
})();)JS";

// How kFindInPageJs reports to the host: a posted "<method> <args...>" line.
static const char* kFindNotifyJs = R"JS(window.__sumatraFindNotify = function() {
  var s = Array.prototype.join.call(arguments, " ");
  var wv = window.chrome && window.chrome.webview;
  if (wv && wv.postMessage) { wv.postMessage(s); return; }
  if (window.ipc && window.ipc.postMessage) { window.ipc.postMessage(s); }
};)JS";

#if OS_WIN
// ng: orig's AcceleratorKeyPressed handler (WebView.cpp). gpui's webview has
// none, so the page posts "key <vk> <mods>" for what WebView2 treats as an
// accelerator (Ctrl / Alt combos, F-keys, Esc) and the frame runs it.
// Clipboard and select-all stay with the page. mods: 1 shift, 2 ctrl, 4 alt.
static const char* kForwardKeysJs = R"JS((function(){
  if (window.__sumatraKeysHooked) { return; }
  window.__sumatraKeysHooked = true;
  var pageKeys = { 65: 1, 67: 1, 86: 1, 88: 1, 89: 1, 90: 1 };
  window.addEventListener("keydown", function(e) {
    var vk = e.keyCode;
    if (vk === 16 || vk === 17 || vk === 18 || vk === 91 || vk === 92) { return; }
    var fkey = vk >= 112 && vk <= 135;
    if (!e.ctrlKey && !e.altKey && !fkey && vk !== 27) { return; }
    if (e.ctrlKey && !e.altKey && !e.shiftKey && pageKeys[vk]) { return; }
    var mods = (e.shiftKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.altKey ? 4 : 0);
    try {
      var wv = window.chrome && window.chrome.webview;
      if (wv && wv.postMessage) { wv.postMessage("key " + vk + " " + mods); }
    } catch (ex) {}
    e.preventDefault();
    e.stopPropagation();
  }, true);
})();)JS";
#endif

struct BrowserView {
    MainWindow* win = nullptr;
    BrowserViewCallback* cb = nullptr;
    Str virtualHost; // owned
    gp::App* app = nullptr;
    gp::Entity<gp::WebView> view;
    bool created = false;
    bool creating = false;
    // the element asked for a view; it is made from the shell's tick, not
    // during the frame (see BrowserViewCreatePending)
    bool wantCreate = false;
    // WebView2's create runs a nested message loop. A control request in that
    // loop makes the first WebViewNew fail, so the tick tries a few times.
    int createTries = 0;
    // drawn in a tool window, which makes it from its own tick
    bool ownHost = false;
    bool visible = false;
    int clipTop = 0;
    // popups drawn over the view: wanted, and what the window region cuts out
    Vec<Rect> holes;
    Vec<Rect> clipHoles;
    // composition color; unset keeps the white host chm and markdown use
    Color background = kColorUnset;
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

// Asked while building every frame's command context, so the answer is kept
// for two seconds: finding the WebView2 runtime reads the registry each time.
bool BrowserViewAvailable() {
    static double checkedAt = -1;
    static bool available = false;
    double now = gp::TimeNow();
    if (checkedAt < 0 || now - checkedAt >= 2.0) {
        available = wry::WebViewAvailable();
        checkedAt = now;
    }
    return available;
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
    // A mac load is sumatrapdf://…; the model speaks the https virtual host.
    uri = FromProtocolUrlTemp(uri);
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

static void ScrollToFragment(BrowserView* bv, Str url);

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

static wry::Rgba WryColor(Color c) {
    return wry::Rgba{GetRed(c), GetGreen(c), GetBlue(c), 0xff};
}

static void ApplyBackground(BrowserView* bv) {
    if (!bv || ColorSkipsPaint(bv->background)) {
        return;
    }
    wry::WebView* raw = Raw(bv);
    if (!raw) {
        return;
    }
    wry::WebViewSetBackgroundColor(raw, WryColor(bv->background));
}

// orig's webView->defaultBackgroundColor: the host shows through until the
// page paints, and again while the window is resized
void BrowserViewSetBackground(BrowserView* bv, Color c) {
    if (!bv) {
        return;
    }
    bv->background = c;
    ApplyBackground(bv);
}

static void OnPageLoad(void* ctx, wry::PageLoadEvent ev, gp::Str url) {
    auto* bv = (BrowserView*)ctx;
    if (!bv || !bv->cb || ev != wry::PageLoadEvent::Finished) {
        return;
    }
    ApplyBackground(bv);
    if (!ColorSkipsPaint(bv->background) && bv->visible) {
        wry::WebView* raw = Raw(bv);
        if (raw) {
            wry::WebViewSetVisible(raw, true);
        }
    }
    TempStr u = UrlForEventTemp(bv, FromGpui(url));
    if (len(u) == 0) {
        return;
    }
    NoteHistoryNavigation(bv, u);
    bv->cb->OnDocumentComplete(u);
    // after zoom / restore have applied: the init scripts may have missed this
    // document, and hash navigation often never fires a 'scroll' event
    ScrollToFragment(bv, u);
    Eval(bv, Str(kReportScrollJs));
    AppShellInvalidate(bv->win);
}

// the notifications kReportScrollJs / kFindInPageJs post back
struct PageKeyData {
    MainWindow* win = nullptr;
    int vk = 0;
    int mods = 0;
};

// a shortcut pressed in the page, run by the frame as if pressed there.
// Posted: the IPC callback runs inside WebView2's event dispatch.
static void RunPageKey(PageKeyData* d) {
    AutoDelete delData(d);
    if (!IsMainWindowValidAndNotClosing(d->win) || !d->win->gpuiWin) {
        return;
    }
    gp::Window* gw = d->win->gpuiWin;
    gp::WindowKeyDown(gw, d->vk, (d->mods & 1) != 0, (d->mods & 2) != 0, (d->mods & 4) != 0);
    // no WM_CHAR follows a key that didn't come from the keyboard
    gw->eatChar = false;
    gp::AppInvalidate(gw);
}

static void ForwardPageKey(BrowserView* bv, int vk, int mods) {
    auto* d = new PageKeyData;
    d->win = bv->win;
    d->vk = vk;
    d->mods = mods;
    uitask::Post(MkFunc0<PageKeyData>(RunPageKey, d), "BrowserViewPageKey");
}

static void OnIpcMessage(void* ctx, gp::Str, gp::Str body) {
    auto* bv = (BrowserView*)ctx;
    if (!bv || !bv->cb) {
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
    int vk = 0, mods = 0;
    if (!str::IsNull(str::Parse(msg, "key %d %d", &vk, &mods))) {
        ForwardPageKey(bv, vk, mods);
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
    if (!VecAppend(gBrowserViews, bv)) {
        str::Free(bv->virtualHost);
        delete bv;
        return nullptr;
    }
    return bv;
}

static void FreeBrowserView(BrowserView* bv) {
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

void BrowserViewDelete(BrowserView* bv) {
    if (!bv || VecRemove(gBrowserViews, bv) < 0) {
        return;
    }
    bv->cb = nullptr;
    if (!bv->creating) {
        FreeBrowserView(bv);
    }
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

void BrowserViewRefreshSurface(BrowserView*) {}

// --- navigation -------------------------------------------------------------

// WKWebView only delivers a scheme we registered. The app still speaks
// https://sumatrapdf.* ; the view is asked for sumatrapdf://* instead.
static TempStr ToWebViewUrlTemp(Str url) {
#if OS_DARWIN
    Str prefix(kHostPrefix);
    if (str::StartsWith(url, prefix)) {
        Str rest(url.s + prefix.len, url.len - prefix.len);
        return str::JoinTemp(Str(kProtocolPrefix), rest);
    }
#endif
    return str::DupTemp(url);
}

// A custom-scheme load often ignores #fragment. Scroll once the node exists.
static void ScrollToFragment(BrowserView* bv, Str url) {
    Str frag = str::SliceFromChar(url, '#');
    if (len(frag) < 2) {
        return;
    }
    TempStr id = url::DecodeTemp(Str(frag.s + 1, frag.len - 1));
    if (len(id) == 0) {
        return;
    }
    Eval(bv, fmt("var el=document.getElementById('%s'); if (el) el.scrollIntoView();", JsEscapeTemp(id)));
}

static TempStr FullUrlTemp(BrowserView* bv, Str url) {
    if (str::StartsWith(url, bv->virtualHost)) {
        return str::DupTemp(url);
    }
    Str rel = url;
    str::TrimAny(rel, "/");
    return str::JoinTemp(bv->virtualHost, rel);
}

void BrowserViewNavigate(BrowserView* bv, Str url) {
    if (!bv || len(url) == 0) {
        return;
    }
    TempStr fullUrl = ToWebViewUrlTemp(FullUrlTemp(bv, url));
    wry::WebView* raw = Raw(bv);
    if (!raw) {
        // the webview is made in the first frame that shows it; go there then
        str::ReplaceWithCopy(&bv->pendingUrl, fullUrl);
        AppShellInvalidate(bv->win);
        return;
    }
    wry::WebViewLoadUrl(raw, ToGpui(fullUrl));
    ScrollToFragment(bv, fullUrl);
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
    // Fit-page modes are negative. pageZoom is a scale, and a negative one
    // inverts the viewport (innerWidth becomes width / zoom).
    if (zoom <= 0) {
        zoom = 100;
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

#if OS_WIN
// async png of the visible page; false if not supported (done is then not called)
bool BrowserViewCapturePng(BrowserView* bv, const Func1<Str>& done) {
    auto* controller = (ICoreWebView2Controller*)wry::WebViewControllerRaw(Raw(bv));
    HWND hwnd = nullptr;
    ICoreWebView2* webview = nullptr;
    if (!controller || FAILED(controller->get_ParentWindow(&hwnd)) || FAILED(controller->get_CoreWebView2(&webview))) {
        return false;
    }
    bool ok = WebView2CapturePng(webview, hwnd, done);
    if (webview) {
        webview->Release();
    }
    return ok;
}
#else
bool BrowserViewCapturePng(BrowserView*, const Func1<Str>&) {
    return false;
}
#endif

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
        TempStr fullUrl = ToWebViewUrlTemp(FullUrlTemp(bv, pageUrls.At(i)));
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

// one arrow key, in CSS pixels. A page key uses the window height instead.
constexpr int kBrowserArrowStep = 40;

void BrowserViewScrollKey(BrowserView* bv, int vk) {
    if (!bv || !Raw(bv)) {
        return;
    }
    switch (vk) {
        case VK_DOWN:
            Eval(bv, fmt("window.scrollBy(0, %d)", kBrowserArrowStep));
            break;
        case VK_UP:
            Eval(bv, fmt("window.scrollBy(0, %d)", -kBrowserArrowStep));
            break;
        case VK_RIGHT:
            Eval(bv, fmt("window.scrollBy(%d, 0)", kBrowserArrowStep));
            break;
        case VK_LEFT:
            Eval(bv, fmt("window.scrollBy(%d, 0)", -kBrowserArrowStep));
            break;
        case VK_NEXT:
            Eval(bv, StrL("window.scrollBy(0, window.innerHeight || 400)"));
            break;
        case VK_PRIOR:
            Eval(bv, StrL("window.scrollBy(0, -(window.innerHeight || 400))"));
            break;
        case VK_HOME:
            Eval(bv, StrL("window.scrollTo(0, 0)"));
            break;
        case VK_END:
            Eval(bv, StrL("window.scrollTo(0, document.documentElement.scrollHeight)"));
            break;
    }
}

// Chromium ignores WM_MOUSEWHEEL on the host window, and an arrow key sent to
// the frame never reaches the page. Both scroll from here.
LRESULT BrowserViewPassUIMsg(BrowserView* bv, UINT msg, WPARAM wp, LPARAM) {
#if OS_WIN
    if (!bv || !Raw(bv)) {
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        BrowserViewScrollKey(bv, (int)wp);
        return 0;
    }
    if (msg == WM_KEYUP) {
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

    wry::InitializationScript scripts[5];
    int nScripts = 0;
    scripts[nScripts++].script = ToGpui(Str(kReportScrollJs));
    scripts[nScripts++].script = ToGpui(Str(kFindInPageJs));
    scripts[nScripts++].script = ToGpui(Str(kJsNotifyJs));
    scripts[nScripts++].script = ToGpui(Str(kFindNotifyJs));
#if OS_WIN
    scripts[nScripts++].script = ToGpui(Str(kForwardKeysJs));
#endif

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
    attrs.initializationScriptCount = nScripts;
    attrs.ctx = bv;
    attrs.navigationHandler = OnNavigationStarting;
    attrs.onPageLoadHandler = OnPageLoad;
    attrs.newWindowReqHandler = OnNewWindow;
    attrs.ipcHandler = OnIpcMessage;
    attrs.hasBackgroundColor = true;
    attrs.backgroundColor = WryColor(ColorSkipsPaint(bv->background) ? kColWhite : bv->background);
    // an empty document is the dark canvas; keep it hidden until the page is
    // up so the theme-colored window is what shows
    attrs.visible = ColorSkipsPaint(bv->background);
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
        attrs.url = ToGpui(ToWebViewUrlTemp(bv->pendingUrl));
    }

    // creating runs a nested message loop, which can ask for another page
    Str startUrl = str::Dup(bv->pendingUrl);
    bv->creating = true;
    gWebViewCreateDepth++;
    gp::Entity<gp::WebView> view = gp::WebViewNew(cx, &attrs);
    gWebViewCreateDepth--;
    bv->creating = false;
    bv->view = view;
    // Keep the callback context alive until native creation and disposal finish.
    if (!bv->cb) {
        FreeBrowserView(bv);
        str::Free(startUrl);
        return;
    }
    gp::WebView* wv = bv->view.Get(cx->app);
    if (!wv || !gp::WebViewRaw(wv)) {
        str::Free(startUrl);
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
    // a navigation asked for while it was being made is loaded on the next build
    if (str::Eq(bv->pendingUrl, startUrl)) {
        str::FreePtr(&bv->pendingUrl);
    }
    str::Free(startUrl);
    if (bv->zoomPercent != 100) {
        wry::WebViewZoom(gp::WebViewRaw(wv), (double)bv->zoomPercent / 100.0);
    }
    // hidden before the tick made it (a panel closed right after opening)
    if (!bv->visible) {
        gp::WebViewHide(wv);
    }
    logf("BrowserView: webview created, host '%s'\n", bv->virtualHost);
}

// ng: a WebView2 is created with a nested message loop, which must not run
// inside gpui's paint (the window never stops repainting if it does), so the
// shell's tick makes the pending views instead of BrowserViewBuild()
void BrowserViewCreatePending(MainWindow* win, gp::Ctx* cx) {
    // a copy: creating runs a nested loop that can add or delete views
    Vec<BrowserView*> views = gBrowserViews;
    for (BrowserView* bv : views) {
        if (!VecContains(gBrowserViews, bv)) {
            continue;
        }
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
    MainWindow* win = bv->win;
    CreateWebView(bv, cx);
    AppShellInvalidate(win);
}

// ng: gpui draws a popup menu in the frame, under the web view's native
// window (orig's menus are windows of their own), so the region cuts it out
void BrowserViewSetHoles(BrowserView* bv, const Vec<Rect>& holes) {
    if (!bv) {
        return;
    }
    VecReset(bv->holes);
    for (const Rect& r : holes) {
        VecAppend(bv->holes, r);
    }
}

static bool SameRects(const Vec<Rect>& a, const Vec<Rect>& b) {
    if (len(a) != len(b)) {
        return false;
    }
    for (int i = 0; i < len(a); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void ClipBrowserTop(BrowserView* bv, int top) {
#if OS_WIN
    if (top == bv->clipTop && SameRects(bv->holes, bv->clipHoles)) {
        return;
    }
    auto* controller = (ICoreWebView2Controller*)wry::WebViewControllerRaw(Raw(bv));
    HWND hwnd = nullptr;
    if (!controller || FAILED(controller->get_ParentWindow(&hwnd)) || !hwnd) {
        return;
    }
    // Clip the native surface without changing the page's viewport or scroll position.
    HRGN region = nullptr;
    if (top > 0 || len(bv->holes) > 0) {
        HWND frameHwnd = AppShellNativeHwnd(bv->win);
        Rect frame = HwndClientRect(frameHwnd);
        region = CreateRectRgn(0, AppShellDpiPx(bv->win, top), frame.dx, frame.dy);
        // holes are in frame client pixels, the region in the host window's
        for (const Rect& r : bv->holes) {
            POINT pts[2] = {{r.x, r.y}, {r.x + r.dx, r.y + r.dy}};
            MapWindowPoints(frameHwnd, hwnd, pts, 2);
            HRGN hole = CreateRectRgn(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
            CombineRgn(region, region, hole, RGN_DIFF);
            DeleteObject(hole);
        }
    }
    if (!SetWindowRgn(hwnd, region, TRUE)) {
        if (region) {
            DeleteObject(region);
        }
        return;
    }
    bv->clipTop = top;
    VecReset(bv->clipHoles);
    for (const Rect& r : bv->holes) {
        VecAppend(bv->clipHoles, r);
    }
#endif
}

gp::El* BrowserViewBuild(BrowserView* bv, gp::Ctx* cx, int clipTop) {
    if (!bv) {
        return nullptr;
    }
    if (!bv->created) {
        bv->wantCreate = true;
        // orig's host brush shows until the web view exists. An unset color
        // keeps the muted fill chm and markdown use.
        gp::El* fill = gp::Div(cx->a)->SizeFull();
        if (ColorSkipsPaint(bv->background)) {
            fill->Bg(gp::ThemeNow(cx->app).tokens.muted);
        } else {
            fill->Bg(ToGpui(bv->background));
        }
        return fill;
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
    ClipBrowserTop(bv, clipTop);
    // The web view is transparent, so this is what shows through a page that
    // does not paint its own ground. An unset color is white.
    gp::El* host = gp::Div(cx->a)->SizeFull();
    if (ColorSkipsPaint(bv->background)) {
        host->Bg(ToGpui(kColWhite));
    } else {
        host->Bg(ToGpui(bv->background));
    }
    return host->Child(gp::WebViewEl(bv->view, cx));
}
