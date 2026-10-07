/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Browser host shared by the document models. The original uses WebView2/IE;
// ng uses the GPUI WebView.

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct BrowserView;

struct BrowserViewCallback {
    // called when we're about to show a given url. Returning false will
    // stop loading this url
    virtual bool OnBeforeNavigate(Str url, bool newWindow) = 0;

    // called after html document has been completely loaded
    virtual void OnDocumentComplete(Str url) = 0;

    // allows for providing data for a given url.
    // returning nullptr means data wasn't provided.
    virtual Str GetDataForUrl(Str url) = 0;

    // called when left mouse button is clicked in the web control window.
    // we use it to maintain proper focus (since it's stolen by left click)
    virtual void OnLButtonDown() = 0;

    // called when a file can't be displayed and has to be downloaded instead
    virtual void DownloadData(Str url, Str data) = 0;

    // in-page find result: search generation, 1-based current match and total
    // match count on this page
    virtual void OnFindResult(int, int, int) {}

    // all-pages find result: the raw "<gen> <total> <records>" payload
    virtual void OnFindAllResult(Str) {}

    // window.__sumatra__.notify(method, ...args) in the page; paramsJson is
    // the JSON array of the arguments
    virtual void OnJsNotify(Str /*method*/, Str /*paramsJson*/) {}

    virtual ~BrowserViewCallback() = default;
};

// takes ownership of nothing; `cb` outlives the view (the model owns it)
BrowserView* BrowserViewCreate(MainWindow* win, HWND hwndParent, BrowserViewCallback* cb, Str virtualHost);
void BrowserViewDelete(BrowserView*);
MainWindow* BrowserViewWindow(BrowserView*);
// for a view in a window of its own whose main window closed
void BrowserViewSetWindow(BrowserView*, MainWindow*);

// show / hide without destroying the browser (a tab switch)
void BrowserViewSetVisible(BrowserView*, bool visible);
bool BrowserViewIsVisible(BrowserView*);

void BrowserViewNavigate(BrowserView*, Str url);
void BrowserViewGoBack(BrowserView*);
void BrowserViewGoForward(BrowserView*);
bool BrowserViewCanGoBack(BrowserView*);
bool BrowserViewCanGoForward(BrowserView*);

void BrowserViewSetZoomPercent(BrowserView*, int zoom);
int BrowserViewGetZoomPercent(BrowserView*);

// scroll position in CSS pixels; (-1, -1) when it isn't known yet
Point BrowserViewGetScrollPos(BrowserView*);
void BrowserViewSetScrollPos(BrowserView*, Point pos);

// run javascript in the page (nothing happens when there is no view yet)
void BrowserViewEval(BrowserView*, Str js);

void BrowserViewSelectAll(BrowserView*);
void BrowserViewCopySelection(BrowserView*);
void BrowserViewPrint(BrowserView*, bool showUI = true);
// the browser's own find bar (orig's FindInCurrentPage)
void BrowserViewFindInPageUI(BrowserView*);

bool BrowserViewCanFindInPage(BrowserView*);
void BrowserViewFindStart(BrowserView*, Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx);
void BrowserViewFindAllPages(BrowserView*, const StrVec& pageUrls, Str term, bool matchCase, bool wholeWord, int gen);
void BrowserViewFindGoto(BrowserView*, int idx);
void BrowserViewFindClear(BrowserView*);
LRESULT BrowserViewPassUIMsg(BrowserView*, UINT msg, WPARAM wp, LPARAM lp);

// the element the webview is positioned over; asked once a frame by the shell
gpui::El* BrowserViewBuild(BrowserView*, gpui::Ctx* cx);
// the shell's tick: makes the views BrowserViewBuild() asked for
void BrowserViewCreatePending(MainWindow* win, gpui::Ctx* cx);
// ng: for a view shown in a tool window (the F1 manual): the frame's tick
// leaves it alone and that window's own tick makes it, with that window's Ctx
void BrowserViewSetOwnHost(BrowserView*);
void BrowserViewCreatePendingIn(BrowserView*, gpui::Ctx* cx);

// is there an embedded browser at all (WebView2 runtime installed)
bool BrowserViewAvailable();
