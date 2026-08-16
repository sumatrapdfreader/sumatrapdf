/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "base/ScopedWin.h"

#include "gui/win/HtmlWindow.h"
#include "gui/win/BrowserDocView.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"

#include "SumatraPDF.h"
#include "EmbeddedResources.h"
#include "MarkdownModel.h"
#include "MarkdownToc.h"

constexpr const char* kMdVirtualHost = "https://sumatrapdf.markdown/";
constexpr int kMdVirtualHostLen = sizeof("https://sumatrapdf.markdown/") - 1;

static bool IsMarkdownVirtualHostUrl(Str url) {
    if (!url) {
        return false;
    }
    if (str::StartsWith(url, kMdVirtualHost)) {
        return true;
    }
    TempStr plain = url::GetFullPathTemp(url);
    return plain && str::StartsWith(plain, kMdVirtualHost);
}

// Virtual-host pages use an https:// scheme but are served in-app via WebView2.
static bool IsMarkdownExternalUrl(Str url) {
    if (!url || IsMarkdownVirtualHostUrl(url)) {
        return false;
    }
    return IsExternalUrl(url);
}

static TempStr NormalizeMarkdownUrlTemp(Str url) {
    TempStr plainUrl = url::GetFullPathTemp(url);
    if (!plainUrl) {
        return {};
    }
    if (str::StartsWith(plainUrl, kMdVirtualHost)) {
        return plainUrl;
    }
    return str::JoinTemp(kMdVirtualHost, plainUrl);
}

// Keep the fragment when navigating the browser. GetFullPathTemp() intentionally
// removes it for page lookup and state tracking, but WebView2 needs it to scroll
// to a heading within the current HTML page.
static Str MarkdownBrowserNavigationUrl(Str url) {
    str::TrimPrefix(url, kMdVirtualHost);
    return url;
}

// Extensions the embedded browser can display on its own: the pages we render
// plus the web resources WebView2 renders natively. An extension-less link is a
// link to another page (VirtualUrlToFileTemp() resolves the name to a .md file).
static bool IsBrowserViewableExt(Str urlOrPath) {
    static SeqStrings exts =
        ".md\0.markdown\0.html\0.htm\0.xhtml\0.txt\0.css\0.js\0.json\0"
        ".svg\0.png\0.apng\0.jpg\0.jpeg\0.gif\0.bmp\0.webp\0.avif\0.ico\0";
    TempStr ext = path::GetExtTemp(urlOrPath);
    return !ext || SeqStrIndexIS(exts, ext) >= 0;
}

// "...#page=3" -> "page=3", url-decoded
static TempStr UrlFragmentTemp(Str url) {
    Str frag = str::SliceFromChar(url, '#');
    if (len(frag) < 2) {
        return {};
    }
    return url::DecodeTemp(Str(frag.s + 1, frag.len - 1));
}

static TempStr RelPathFromBaseTemp(Str filePath, Str baseDir) {
    TempStr normFile = path::NormalizeTemp(filePath);
    TempStr normBase = path::NormalizeTemp(baseDir);
    if (!normBase || !str::TrimPrefix(normFile, normBase)) {
        return path::GetBaseNameTemp(filePath);
    }
    Str rel = normFile;
    while (len(rel) > 0 && (rel.s[0] == '\\' || rel.s[0] == '/')) {
        rel.s++;
        rel.len--;
    }
    if (!rel) {
        return path::GetBaseNameTemp(filePath);
    }
    return str::DupTemp(rel);
}

struct MarkdownCacheEntry {
    Str url;
    Str data;
};

struct MarkdownTocTraceItem {
    Str title;
    Str url;
    int level = 0;
    int pageNo = 0;
};

// State shared with the background TOC builder. Outlives the model: it holds
// copies of everything the worker needs, and `model` is nulled (under the lock)
// when the model is destroyed, so a build that finishes too late is harmless.
struct MarkdownTocBuildTask {
    Mutex lock;
    MarkdownModel* model = nullptr;
    StrVec pages;
    Str baseDir;
    bool isHtml = false;
    TocTree* tocTree = nullptr; // the result, owned until installed

    ~MarkdownTocBuildTask() {
        str::Free(baseDir);
        delete tocTree;
    }
};

// Opens the document a link points at once we're out of the WebView2 callback
// (see MaybeLaunchLinkedDoc). `model` is nulled when the model is destroyed, so
// a document closed before the task runs just drops the request.
struct MarkdownLaunchTask {
    MarkdownModel* model = nullptr;
    Str path; // owned
    Str dest; // owned

    ~MarkdownLaunchTask() {
        str::Free(path);
        str::Free(dest);
    }
};

static IPageDestination* NewMarkdownNamedDest(Str url, int pageNo) {
    if (!url) {
        return nullptr;
    }
    IPageDestination* dest = nullptr;
    if (IsMarkdownExternalUrl(url)) {
        dest = new PageDestinationURL(url);
    } else {
        auto* pdest = new PageDestination();
        pdest->kind = kindDestinationScrollTo;
        pdest->name = str::Dup(url);
        dest = pdest;
    }
    dest->pageNo = pageNo;
    dest->rect = RectF(kDestUseDefault, kDestUseDefault, kDestUseDefault, kDestUseDefault);
    return dest;
}

static TocItem* NewMarkdownTocItem(TocItem* parent, Str title, int pageNo, Str url) {
    auto* res = AllocTocItem(nullptr, title, pageNo);
    res->parent = parent;
    res->dest = NewMarkdownNamedDest(url, pageNo);
    return res;
}

class MarkdownHtmlWindowHandler : public HtmlWindowCallback {
    MarkdownModel* mm;

  public:
    explicit MarkdownHtmlWindowHandler(MarkdownModel* mm) : mm(mm) {}
    ~MarkdownHtmlWindowHandler() override = default;

    bool OnBeforeNavigate(Str url, bool newWindow) override { return mm->OnBeforeNavigate(url, newWindow); }
    void OnDocumentComplete(Str url) override { mm->OnDocumentComplete(url); }
    void OnLButtonDown() override { mm->OnLButtonDown(); }
    Str GetDataForUrl(Str url) override { return mm->GetDataForUrl(url); }
    void DownloadData(Str url, Str data) override { mm->DownloadData(url, data); }
    void OnFindResult(int gen, int current, int total) override { mm->OnFindResult(gen, current, total); }
    void OnFindAllResult(Str payload) override { mm->OnFindAllResult(payload); }
};

MarkdownModel::MarkdownModel(DocControllerCallback* cb) : DocController(cb) {
    poolAlloc = ArenaNew();
}

MarkdownModel::~MarkdownModel() {
    if (tocBuildTask) {
        // a build may still be running; tell it there's nobody to deliver to
        ScopedMutex scope(&tocBuildTask->lock);
        tocBuildTask->model = nullptr;
        tocBuildTask = nullptr;
    }
    if (launchTask) {
        // a queued open has nobody to ask about the document anymore
        launchTask->model = nullptr;
        launchTask = nullptr;
    }
    docAccess.Lock();
    delete docView;
    delete htmlWindowCb;
    delete tocTree;
    DeleteVecMembers(urlDataCache);
    docAccess.Unlock();
    ArenaDelete(poolAlloc);
    str::Free(fileName);
    str::Free(currentPageUrl);
    str::Free(pendingFindTerm);
}

Str MarkdownModel::GetFilePath() const {
    return fileName;
}

Str MarkdownModel::GetDefaultFileExt() const {
    return isHtml ? ".html" : ".md";
}

int MarkdownModel::PageCount() const {
    return len(pages);
}

TempStr MarkdownModel::GetPropertyTemp(DocProp prop) {
    if (prop == DocProp::Title) {
        return path::GetBaseNameTemp(fileName);
    }
    return {};
}

int MarkdownModel::CurrentPageNo() const {
    return currentPageNo;
}

// the TOC is also built on a background thread, which has no model to ask, so
// this takes the two fields it needs instead of being a method
static TempStr FileToVirtualUrlTemp(Str filePath, Str baseDir, bool isHtml) {
    if (!filePath) {
        return {};
    }
    TempStr rel = RelPathFromBaseTemp(filePath, baseDir);
    if (!rel) {
        rel = path::GetBaseNameTemp(filePath);
    }
    rel = str::ReplaceTemp(rel, StrL("\\"), StrL("/"));
    if (isHtml) {
        // .html files are served raw, so keep their real name/extension
        return fmt("%s%s", Str(kMdVirtualHost, kMdVirtualHostLen), rel);
    }
    Str relStr = rel;
    if (str::EndsWithI(relStr, StrL(".markdown"))) {
        relStr.len -= 9;
    } else if (str::EndsWithI(relStr, StrL(".md"))) {
        relStr.len -= 3;
    }
    return fmt("%s%s.html", Str(kMdVirtualHost, kMdVirtualHostLen), relStr);
}

TempStr MarkdownModel::FileToVirtualUrlTemp(Str filePath) const {
    return ::FileToVirtualUrlTemp(filePath, baseDir, isHtml);
}

TempStr MarkdownModel::VirtualUrlToFileTemp(Str url) const {
    if (!url || !str::TrimPrefix(url, kMdVirtualHost)) {
        return {};
    }
    Str pathPart = url;
    Str fragment = str::SliceFromChar(pathPart, '#');
    if (fragment) {
        pathPart = Str(pathPart.s, (int)(fragment.s - pathPart.s));
    }
    TempStr rel = str::ReplaceTemp(pathPart, StrL("/"), StrL("\\"));
    if (isHtml) {
        // page urls keep their real name; images/links resolve against baseDir too
        return path::JoinTemp(baseDir, rel);
    }
    if (str::EndsWithI(rel, StrL(".html"))) {
        // a page url made by FileToVirtualUrlTemp(): <name>.html for <name>.md
        rel.len -= 5;
    } else {
        // a file referenced by its real name: an image, a raw link to
        // another .md file etc.
        TempStr direct = path::JoinTemp(baseDir, rel);
        if (pages.Find(direct) >= 0 || file::Exists(direct)) {
            return direct;
        }
        // fall through: possibly an extension-less link to a page
    }
    TempStr mdPath = path::JoinTemp(baseDir, Str(str::JoinTemp(rel, StrL(".md"))));
    if (pages.Find(mdPath) >= 0) {
        return mdPath;
    }
    TempStr mdownPath = path::JoinTemp(baseDir, Str(str::JoinTemp(rel, StrL(".markdown"))));
    if (pages.Find(mdownPath) >= 0) {
        return mdownPath;
    }
    return mdPath;
}

bool MarkdownModel::SetParentHwnd(HWND hwnd) {
    // reuse the existing browser when switching back to this tab: creating a
    // WebView2 is hundreds of ms, so we only hide it in RemoveParentHwnd
    if (docView) {
        if (docView->GetParentHwnd() == hwnd) {
            docView->SetVisible(true);
            return true;
        }
        // different parent (shouldn't happen for a tab): rebuild
        delete docView;
        docView = nullptr;
        delete htmlWindowCb;
        htmlWindowCb = nullptr;
    }
    htmlWindowCb = new MarkdownHtmlWindowHandler(this);
    docView = BrowserDocView::Create(hwnd, htmlWindowCb, Str(kMdVirtualHost));
    if (!docView) {
        delete htmlWindowCb;
        htmlWindowCb = nullptr;
        return false;
    }
    docView->SetVisible(true);
    return true;
}

void MarkdownModel::RemoveParentHwnd() {
    if (!docView) {
        return;
    }
    // keep the browser alive (hidden) so the next SetParentHwnd is cheap
    SaveHtmlScrollPos();
    restoreHtmlScrollPos = true;
    docView->SetVisible(false);
}

void MarkdownModel::DestroyParentHwnd() {
    if (!docView && !htmlWindowCb) {
        return;
    }
    SaveHtmlScrollPos();
    restoreHtmlScrollPos = true;
    // DestroyWindow inside ~BrowserDocView / ~WebviewWnd pumps messages
    delete docView;
    docView = nullptr;
    delete htmlWindowCb;
    htmlWindowCb = nullptr;
}

void MarkdownModel::PrintCurrentPage(bool showUI) const {
    if (docView) {
        docView->PrintCurrentPage(showUI);
    }
}

void MarkdownModel::FindInCurrentPage() const {
    if (docView) {
        docView->FindInCurrentPage();
    }
}

bool MarkdownModel::CanFindInPage() const {
    return docView && docView->CanFindInPage();
}

void MarkdownModel::FindStart(Str term, bool matchCase, bool wholeWord, int gen) {
    if (docView) {
        docView->FindStart(term, matchCase, wholeWord, gen, -1);
    }
}

void MarkdownModel::FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) {
    if (!docView) {
        return;
    }
    StrVec urls;
    for (Str page : pages) {
        urls.Append(FileToVirtualUrlTemp(page));
    }
    docView->FindAllPages(urls, term, matchCase, wholeWord, gen);
}

void MarkdownModel::FindGoto(int idx) {
    if (docView) {
        docView->FindGoto(idx);
    }
}

// navigate to pageNo and, once it has loaded, highlight term there and make
// its idx-th match current (see OnDocumentComplete)
void MarkdownModel::GoToPageWithFind(int pageNo, Str term, bool matchCase, bool wholeWord, int idx, int gen) {
    str::ReplaceWithCopy(&pendingFindTerm, term);
    pendingFindMatchCase = matchCase;
    pendingFindWholeWord = wholeWord;
    pendingFindIdx = idx;
    pendingFindGen = gen;
    hasPendingFind = true;
    GoToPage(pageNo, false);
}

void MarkdownModel::FindClear() {
    if (docView) {
        docView->FindClear();
    }
}

void MarkdownModel::OnFindResult(int gen, int current, int total) {
    cb->FindResultReceived(gen, current, total);
}

void MarkdownModel::OnFindAllResult(Str payload) {
    cb->FindAllResultReceived(payload);
}

void MarkdownModel::SelectAll() const {
    if (docView) {
        docView->SelectAll();
    }
}

void MarkdownModel::CopySelection() const {
    if (docView) {
        docView->CopySelection();
    }
}

static bool gSendingMarkdownHtmlWindowMsg = false;

LRESULT MarkdownModel::PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const {
    if (!docView || gSendingMarkdownHtmlWindowMsg) {
        return 0;
    }
    gSendingMarkdownHtmlWindowMsg = true;
    auto res = docView->SendMsg(msg, wp, lp);
    gSendingMarkdownHtmlWindowMsg = false;
    return res;
}

// The path a link points at when it's a file the browser view can't show itself
// (a .pdf, .epub, an archive, ...), or {} when the link stays in the view.
// Unlike VirtualUrlToFileTemp() this doesn't fall back to page lookups, so a
// link to a file that doesn't exist still resolves (and reports an error).
TempStr MarkdownModel::LinkedDocPathTemp(Str url) const {
    if (!url || IsMarkdownExternalUrl(url)) {
        return {};
    }
    // WebView2 reports an in-document url with the virtual host already stripped
    // ("sub/doc.pdf"), a TOC destination carries it; normalize to have it
    TempStr urlPath = NormalizeMarkdownUrlTemp(url);
    if (!urlPath || !str::TrimPrefix(urlPath, kMdVirtualHost) || IsBrowserViewableExt(urlPath)) {
        return {};
    }
    TempStr rel = str::ReplaceTemp(urlPath, StrL("/"), StrL("\\"));
    return path::NormalizeTemp(path::JoinTemp(baseDir, rel));
}

// Runs on the UI thread, from the message loop.
static void MarkdownLaunchDoc(MarkdownLaunchTask* task) {
    AutoDelete<MarkdownLaunchTask> delTask(task);
    MarkdownModel* mm = task->model;
    if (!mm) {
        return;
    }
    mm->launchTask = nullptr;
    // a model that no longer belongs to a tab is on its way out and its callback
    // (owned by the window) may be gone already
    if (!FindTabByController(mm) || !mm->cb) {
        return;
    }
    // a fragment is the destination to scroll to in the opened document, the
    // same way LinkHandler::LaunchURL() treats one on a file:// url
    auto* dest = new PageDestinationFile(task->path, task->dest);
    mm->cb->GotoLink(dest);
    delete dest;
}

// A relative link can point at a document rather than at a page of this one, e.g.
// [the manual](./manual.pdf). Let the app open it (in a tab for the formats we
// support, in the shell otherwise) instead of navigating the document webview to
// it, which would render the raw bytes as HTML (discussion #5924).
//
// Deferred to a uitask: opening the document selects a new tab, which tears this
// model's webview down, and we may be called from inside one of its callbacks.
bool MarkdownModel::MaybeLaunchLinkedDoc(Str url) {
    if (!cb) {
        return false;
    }
    TempStr filePath = LinkedDocPathTemp(url);
    if (!filePath) {
        return false;
    }
    if (launchTask) {
        // an open is already queued (link clicked twice): the first one wins
        return true;
    }
    auto* task = new MarkdownLaunchTask;
    task->model = this;
    task->path = str::Dup(filePath);
    task->dest = str::Dup(UrlFragmentTemp(url));
    launchTask = task;
    auto fn = MkFunc0(MarkdownLaunchDoc, task);
    uitask::Post(fn, "MarkdownLaunchDoc");
    return true;
}

bool MarkdownModel::DisplayPage(Str pageUrl) {
    if (!pageUrl) {
        return false;
    }
    pageUrl = str::DupTemp(pageUrl);
    if (IsMarkdownExternalUrl(pageUrl)) {
        if (cb) {
            auto* item = NewMarkdownTocItem(nullptr, nullptr, 1, pageUrl);
            cb->GotoLink(item->dest);
            FreeTocItemRec(nullptr, item);
        }
        return false;
    }

    TempStr plainUrl = url::GetFullPathTemp(pageUrl);
    int pageNo = pages.Find(VirtualUrlToFileTemp(plainUrl)) + 1;
    if (pageNo < 1) {
        pageNo = currentPageNo;
    }

    skipNextBeforeNavigateScrollSave = true;
    str::ReplaceWithCopy(&currentPageUrl, plainUrl);
    currentPageNo = pageNo;
    if (docView) {
        Str navUrl = MarkdownBrowserNavigationUrl(pageUrl);
        docView->NavigateToDataUrl(navUrl);
    }
    return true;
}

void MarkdownModel::GoToPage(int pageNo, bool /*addNavPoint*/) {
    if (!ValidPageNo(pageNo)) {
        return;
    }
    if (pageNo == currentPageNo && len(currentPageUrl) > 0) {
        DisplayPage(currentPageUrl);
        return;
    }
    TempStr url = FileToVirtualUrlTemp(pages[pageNo - 1]);
    DisplayPage(url);
}

void MarkdownModel::ScrollTo(int pageNo, RectF rect, float zoom) {
    if (IsValidZoom(zoom)) {
        SetZoomVirtual(zoom, nullptr);
    }
    if (rect.x >= 0 || rect.y >= 0) {
        htmlScrollPos = PointF(rect.x, rect.y);
        restoreHtmlScrollPos = true;
        if (ValidPageNo(pageNo)) {
            SaveHtmlScrollPosForUrl(FileToVirtualUrlTemp(pages[pageNo - 1]), htmlScrollPos);
        }
    }
    GoToPage(pageNo, false);
}

bool MarkdownModel::HandleLink(IPageDestination* link, ILinkHandler* /*linkHandler*/) {
    Str url = PageDestGetName(link);
    if (MaybeLaunchLinkedDoc(url)) {
        return true;
    }
    if (DisplayPage(url)) {
        return true;
    }
    int pageNo = PageDestGetPageNo(link);
    GoToPage(pageNo, false);
    return true;
}

bool MarkdownModel::CanNavigate(int dir) const {
    if (!docView) {
        return false;
    }
    if (dir < 0) {
        return docView->canGoBack;
    }
    return docView->canGoForward;
}

void MarkdownModel::Navigate(int dir) {
    if (!docView) {
        return;
    }
    if (dir < 0) {
        for (; dir < 0 && CanNavigate(dir); dir++) {
            docView->GoBack();
        }
    } else {
        for (; dir > 0 && CanNavigate(dir); dir--) {
            docView->GoForward();
        }
    }
}

void MarkdownModel::SetDisplayMode(DisplayMode /*mode*/, bool /*keepContinuous*/) {}

DisplayMode MarkdownModel::GetDisplayMode() const {
    return DisplayMode::SinglePage;
}

void MarkdownModel::SetInPresentation(bool /*enable*/) {}

void MarkdownModel::SetViewPortSize(Size /*size*/) {}

MarkdownModel* MarkdownModel::AsMarkdown() {
    return this;
}

void MarkdownModel::SetZoomVirtual(float zoom, Point* /*fixPt*/) {
    if (zoom > 0) {
        zoom = limitValue(zoom, kZoomMin, kZoomMax);
    }
    if (zoom <= 0 || !IsValidZoom(zoom)) {
        zoom = 100.0f;
    }
    ZoomTo(zoom);
    zoomVirtual = zoom;
    initZoom = zoom;
}

void MarkdownModel::SaveHtmlScrollPos() {
    if (!docView) {
        return;
    }
    Point pos = docView->GetScrollPos();
    if (pos.x < 0 && pos.y < 0) {
        return;
    }
    htmlScrollPos = PointF((float)pos.x, (float)pos.y);
    if (len(currentPageUrl) > 0) {
        SaveHtmlScrollPosForUrl(currentPageUrl, htmlScrollPos);
        return;
    }
    SaveHtmlScrollPosForPage(currentPageNo);
}

void MarkdownModel::SaveHtmlScrollPosForPage(int pageNo) {
    if (!ValidPageNo(pageNo)) {
        return;
    }
    SaveHtmlScrollPosForUrl(FileToVirtualUrlTemp(pages[pageNo - 1]), htmlScrollPos);
}

void MarkdownModel::SaveHtmlScrollPosForUrl(Str url, PointF pos) {
    if (!url || pos.x < 0 || pos.y < 0) {
        return;
    }
    TempStr plainUrl = url::GetFullPathTemp(url);
    int idx = htmlScrollUrls.Find(plainUrl);
    if (idx >= 0) {
        htmlScrollPositions[idx] = pos;
        return;
    }
    htmlScrollUrls.Append(plainUrl);
    htmlScrollPositions.Append(pos);
}

bool MarkdownModel::GetSavedHtmlScrollPosForPage(int pageNo, PointF* pos) const {
    if (!pos || !ValidPageNo(pageNo)) {
        return false;
    }
    return GetSavedHtmlScrollPosForUrl(FileToVirtualUrlTemp(pages[pageNo - 1]), pos);
}

bool MarkdownModel::GetSavedHtmlScrollPosForUrl(Str url, PointF* pos) const {
    if (!url || !pos) {
        return false;
    }
    TempStr plainUrl = url::GetFullPathTemp(url);
    int idx = htmlScrollUrls.Find(plainUrl);
    if (idx < 0) {
        return false;
    }
    *pos = htmlScrollPositions[idx];
    return pos->x >= 0 || pos->y >= 0;
}

void MarkdownModel::RestoreHtmlScrollPos() {
    if (!docView || !restoreHtmlScrollPos) {
        return;
    }
    restoreHtmlScrollPos = false;
    if (htmlScrollPos.x < 0 && htmlScrollPos.y < 0) {
        return;
    }
    int x = (int)htmlScrollPos.x;
    int y = (int)htmlScrollPos.y;
    x = std::max(x, 0);
    y = std::max(y, 0);
    docView->SetScrollPos(Point(x, y));
}

void MarkdownModel::ZoomTo(float zoomLevel) const {
    if (docView) {
        docView->SetZoomPercent((int)zoomLevel);
    }
}

float MarkdownModel::GetZoomVirtual(bool /*absolute*/) const {
    if (!docView) {
        return 100;
    }
    return (float)docView->GetZoomPercent();
}

float MarkdownModel::GetNextZoomStep(float towardsLevel) const {
    float currZoom = GetZoomVirtual(true);
    if (MaybeGetNextZoomByIncrement(&currZoom, towardsLevel)) {
        int iCurrZoom2 = (int)GetZoomVirtual(true);
        int iCurrZoom = (int)currZoom;
        if (iCurrZoom == iCurrZoom2) {
            currZoom += 1.f;
        }
        return currZoom;
    }

    int nZoomLevels;
    float* zoomLevels = GetDefaultZoomLevels(&nZoomLevels);
    int iCurrZoom = (int)currZoom;
    int iTowardsLevel = (int)towardsLevel;
    int iNewZoom = iTowardsLevel;
    if ((float)iCurrZoom < towardsLevel) {
        for (int i = 0; i < nZoomLevels; i++) {
            int iZoom = (int)zoomLevels[i];
            if (iZoom > iCurrZoom) {
                iNewZoom = iZoom;
                break;
            }
        }
    } else if ((float)iCurrZoom > towardsLevel) {
        for (int i = nZoomLevels - 1; i >= 0; i--) {
            int iZoom = (int)zoomLevels[i];
            if (iZoom < iCurrZoom) {
                iNewZoom = iZoom;
                break;
            }
        }
    }
    return (float)iNewZoom;
}

MarkdownCacheEntry* MarkdownModel::FindDataForUrl(Str url) const {
    TempStr plainUrl = url::GetFullPathTemp(url);
    for (MarkdownCacheEntry* e : urlDataCache) {
        if (str::Eq(e->url, plainUrl)) {
            return e;
        }
    }
    return nullptr;
}

bool MarkdownModel::OnBeforeNavigate(Str url, bool newWindow) {
    if (skipNextBeforeNavigateScrollSave) {
        skipNextBeforeNavigateScrollSave = false;
    } else {
        SaveHtmlScrollPos();
    }
    if (cb) {
        cb->FocusFrame(false);
    }
    // external links go to the OS browser / link handler — never navigate the
    // document webview off-document (issue #5920)
    if (IsMarkdownExternalUrl(url)) {
        if (url && cb) {
            auto* item = NewMarkdownTocItem(nullptr, nullptr, 1, url);
            cb->GotoLink(item->dest);
            FreeTocItemRec(nullptr, item);
        }
        return false;
    }
    // a link to a document (.pdf, .epub, ...) opens in the app, not in the view
    if (MaybeLaunchLinkedDoc(url)) {
        return false;
    }
    // new-window request for an in-document URL: navigate in place
    if (newWindow) {
        TempStr plainUrl = NormalizeMarkdownUrlTemp(url);
        if (plainUrl) {
            DisplayPage(plainUrl);
        }
        return false;
    }
    return true;
}

void MarkdownModel::OnDocumentComplete(Str url) {
    if (!url) {
        return;
    }
    TempStr plainUrl = NormalizeMarkdownUrlTemp(url);
    TempStr filePath = VirtualUrlToFileTemp(plainUrl);
    int pageNo = pages.Find(filePath) + 1;
    if (pageNo < 1) {
        pageNo = currentPageNo;
    }
    currentPageNo = pageNo;
    str::ReplaceWithCopy(&currentPageUrl, plainUrl);
    // Keep GetFilePath() on the currently shown .md (folder multi-file model).
    if (ValidPageNo(pageNo)) {
        str::ReplaceWithCopy(&this->fileName, pages[pageNo - 1]);
    }

    if (GetSavedHtmlScrollPosForUrl(plainUrl, &htmlScrollPos)) {
        restoreHtmlScrollPos = true;
    }
    ZoomTo(zoomVirtual);
    RestoreHtmlScrollPos();

    if (cb && pageNo > 0) {
        cb->PageNoChanged(this, pageNo);
    }

    // finish a pending "jump to a match on another page": the fresh document
    // has no find state, so re-run the search and go to the requested match
    if (hasPendingFind && docView) {
        docView->FindStart(pendingFindTerm, pendingFindMatchCase, pendingFindWholeWord, pendingFindGen, pendingFindIdx);
        hasPendingFind = false;
        str::FreePtr(&pendingFindTerm);
    }
}

Str MarkdownModel::GetDataForUrl(Str url) {
    ScopedMutex scope(&docAccess);
    TempStr plainUrl = NormalizeMarkdownUrlTemp(url);
    MarkdownCacheEntry* e = FindDataForUrl(plainUrl);
    if (e) {
        return e->data;
    }

    Str data;
    // mermaid runtime from IDR_EMBEDDED_PAK for ```mermaid fences (see MarkdownToHtmlPage)
    if (str::EndsWithI(plainUrl, StrL("/mermaid.min.js")) || str::EqI(plainUrl, StrL("mermaid.min.js"))) {
        int n = 0;
        u8* js = GetEmbeddedFileData(StrL("mermaid.min.js"), &n);
        if (js && n > 0) {
            data = str::Dup(poolAlloc, Str((const char*)js, n));
        }
        free(js);
    } else {
        TempStr filePath = VirtualUrlToFileTemp(plainUrl);
        // in html mode every resource (the page, images, linked pages) is served raw;
        // in markdown mode .md/.markdown/.html are rendered to a styled page and other
        // resources (images) are served raw
        bool renderMd = !isHtml && filePath &&
                        (str::EndsWithI(filePath, StrL(".md")) || str::EndsWithI(filePath, StrL(".markdown")) ||
                         str::EndsWithI(filePath, StrL(".html")));
        if (renderMd) {
            Str md = file::ReadFile(filePath);
            if (md) {
                data = MarkdownToHtmlPage(md);
            }
        } else if (filePath) {
            data = file::ReadFile(filePath);
        }
    }

    if (!data) {
        return {};
    }

    Str urlDup = str::Dup(poolAlloc, plainUrl);
    e = new MarkdownCacheEntry{urlDup, str::Dup(poolAlloc, data)};
    urlDataCache.Append(e);
    return e->data;
}

// theme colors are baked into the generated HTML: drop the cached pages and
// re-render the current one with the new colors (a hidden tab has no docView;
// it regenerates when re-selected)
void MarkdownModel::UpdateTheme() {
    {
        ScopedMutex scope(&docAccess);
        DeleteVecMembers(urlDataCache);
        urlDataCache.Reset();
    }
    if (docView && currentPageUrl) {
        SaveHtmlScrollPos();
        restoreHtmlScrollPos = true;
        DisplayPage(currentPageUrl);
    }
}

void MarkdownModel::DownloadData(Str url, Str data) {
    if (cb) {
        cb->SaveDownload(url, data);
    }
}

void MarkdownModel::OnLButtonDown() {
    if (cb) {
        cb->FocusFrame(true);
    }
}

IPageDestination* MarkdownModel::GetNamedDest(Str name) {
    TempStr url = url::GetFullPathTemp(name);
    int pageNo = 0;
    TempStr filePath = VirtualUrlToFileTemp(url);
    if (filePath) {
        pageNo = pages.Find(filePath) + 1;
    }
    pageNo = std::max(pageNo, 1);
    return NewMarkdownNamedDest(url, pageNo);
}

TocTree* MarkdownModel::GetToc() {
    return tocTree;
}

void MarkdownModel::GetDisplayState(FileState* fs) {
    Str fileNameA = fileName;
    if (!fs->filePath || !str::EqI(fs->filePath, fileNameA)) {
        SetFileStatePath(fs, fileNameA);
    }
    fs->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;
    str::ReplaceWithCopy(&fs->displayMode, DisplayModeToString(GetDisplayMode()));
    ZoomToString(&fs->zoom, GetZoomVirtual(), fs);
    fs->pageNo = CurrentPageNo();
    SaveHtmlScrollPos();
    fs->scrollPos = htmlScrollPos;
}

void MarkdownModel::CreateThumbnail(Size /*size*/, const OnBitmapRendered* /*saveThumbnail*/) {}

bool MarkdownModel::IsSupportedFileType(FileType kind) {
    return kind == FileType::Markdown || kind == FileType::HTML;
}

bool MarkdownModel::IsHtmlFileType(FileType kind) {
    return kind == FileType::HTML;
}

#if defined(DEBUG)
bool MarkdownModel_UnitTestBrowserNavigationUrl() {
    Str url = StrL("https://sumatrapdf.markdown/issue-5842.html#target-heading");
    return str::Eq(MarkdownBrowserNavigationUrl(url), StrL("issue-5842.html#target-heading"));
}
#endif

// trace items own their strings: the full TOC is built on a background thread,
// which must not touch the model's arena (the model can go away under it)
static void FreeTocTrace(Vec<MarkdownTocTraceItem>& tocTrace) {
    for (MarkdownTocTraceItem& ti : tocTrace) {
        str::Free(ti.title);
        str::Free(ti.url);
    }
    tocTrace.Reset();
}

static TocTree* BuildTocTreeFromTrace(Vec<MarkdownTocTraceItem>& tocTrace) {
    TocItem* root = nullptr;
    TocItem** nextChild = &root;
    Vec<TocItem*> levels;
    bool foundRoot = false;
    int idCounter = 0;
    for (MarkdownTocTraceItem& ti : tocTrace) {
        TocItem* item = NewMarkdownTocItem(nullptr, ti.title, ti.pageNo, ti.url);
        item->id = ++idCounter;
        if (ti.level <= len(levels)) {
            levels.RemoveAt(ti.level, len(levels) - ti.level);
            levels.Last()->AddSiblingAtEnd(item);
        } else {
            *nextChild = item;
            levels.Append(item);
            foundRoot = true;
        }
        nextChild = &item->child;
    }
    if (!foundRoot) {
        return nullptr;
    }
    auto* realRoot = AllocTocItem(nullptr, {}, 0);
    realRoot->child = root;
    return new TocTree(realRoot);
}

static void AppendFileTocTraceItem(Vec<MarkdownTocTraceItem>& tocTrace, Str filePath, Str pageUrl, int pageNo) {
    MarkdownTocTraceItem fileItem;
    // first-level items show the full file name, extension included
    fileItem.title = str::Dup(path::GetBaseNameTemp(filePath));
    fileItem.url = str::Dup(pageUrl);
    fileItem.level = 1;
    fileItem.pageNo = pageNo;
    tocTrace.Append(fileItem);
}

// The TOC we can build without reading a single file. Parsing every sibling
// .html file to find its headings takes minutes in a directory with thousands
// of them (#5918), so the document opens with this and BuildFullToc() replaces
// it when it's ready.
static TocTree* BuildFilesOnlyToc(StrVec& pages, Str baseDir, bool isHtml) {
    Vec<MarkdownTocTraceItem> tocTrace;
    for (int i = 0; i < len(pages); i++) {
        Str filePath = pages[i];
        AppendFileTocTraceItem(tocTrace, filePath, FileToVirtualUrlTemp(filePath, baseDir, isHtml), i + 1);
    }
    TocTree* res = BuildTocTreeFromTrace(tocTrace);
    FreeTocTrace(tocTrace);
    return res;
}

// the real TOC: every file plus the hierarchy of headings inside it
static TocTree* BuildFullToc(StrVec& pages, Str baseDir, bool isHtml) {
    Vec<MarkdownFileToc> fileTocs;
    ParseMarkdownTocsParallel(pages, isHtml, fileTocs);

    Vec<MarkdownTocTraceItem> tocTrace;
    for (int i = 0; i < len(fileTocs); i++) {
        MarkdownFileToc& ft = fileTocs[i];
        int pageNo = i + 1;
        TempStr pageUrl = FileToVirtualUrlTemp(ft.filePath, baseDir, isHtml);
        AppendFileTocTraceItem(tocTrace, ft.filePath, pageUrl, pageNo);

        for (MarkdownHeadingItem& hi : ft.headings) {
            TempStr destUrl = pageUrl;
            if (hi.anchor) {
                destUrl = str::JoinTemp(pageUrl, fmt("#%s", hi.anchor));
            }
            MarkdownTocTraceItem hItem;
            hItem.title = str::Dup(hi.title);
            hItem.url = str::Dup(destUrl);
            hItem.level = hi.level + 1;
            hItem.pageNo = pageNo;
            tocTrace.Append(hItem);
        }
    }

    for (MarkdownFileToc& ft : fileTocs) {
        str::Free(ft.filePath);
        for (MarkdownHeadingItem& hi : ft.headings) {
            str::Free(hi.title);
            str::Free(hi.anchor);
        }
        ft.headings.Reset();
    }

    TocTree* res = BuildTocTreeFromTrace(tocTrace);
    FreeTocTrace(tocTrace);
    return res;
}

// Runs on the UI thread when the background build finished. The model clears
// task->model when it goes away, so a document closed mid-build just drops the
// result on the floor.
static void MarkdownTocBuildFinished(MarkdownTocBuildTask* task) {
    AutoDelete<MarkdownTocBuildTask> delTask(task);
    ScopedMutex scope(&task->lock);
    MarkdownModel* mm = task->model;
    // the task is deleted on every path, so the model must drop its pointer
    // even when the result is discarded, or ~MarkdownModel touches freed memory
    if (mm) {
        mm->tocBuildTask = nullptr;
    }
    // a model that no longer belongs to a tab is on its way out and its
    // callback (owned by the window) may be gone already, so drop the result
    if (!mm || !FindTabByController(mm)) {
        return;
    }
    mm->SetToc(task->tocTree);
    task->tocTree = nullptr;
}

static void MarkdownTocBuildThread(MarkdownTocBuildTask* task) {
    task->tocTree = BuildFullToc(task->pages, task->baseDir, task->isHtml);
    auto fn = MkFunc0(MarkdownTocBuildFinished, task);
    uitask::Post(fn, "MarkdownTocBuildFinished");
}

// take ownership of a newly built TOC and let the UI show it. The old tree is
// deleted only after the UI dropped its references to it (TocChanged rebuilds
// the tree view, which holds TocItem pointers).
void MarkdownModel::SetToc(TocTree* newToc) {
    if (!newToc) {
        return;
    }
    TocTree* old = tocTree;
    tocTree = newToc;
    if (cb) {
        cb->TocChanged(this);
    }
    delete old;
}

bool MarkdownModel::Load(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    str::ReplaceWithCopy(&baseDir, path::GetDirTemp(fileName));
    isHtml = IsHtmlFileType(GuessFileType(fileName, true));

    StrVec mdFiles;
    CollectMarkdownFiles(baseDir, fileName, isHtml, mdFiles);
    if (len(mdFiles) == 0) {
        return false;
    }

    pages = mdFiles;
    // show the files right away, then fill in the headings in the background
    tocTree = BuildFilesOnlyToc(pages, baseDir, isHtml);

    auto* task = new MarkdownTocBuildTask;
    task->model = this;
    task->pages = pages;
    task->baseDir = str::Dup(baseDir);
    task->isHtml = isHtml;
    tocBuildTask = task;
    auto fn = MkFunc0(MarkdownTocBuildThread, task);
    ThreadHandle th = StartThread(fn, "MarkdownTocBuild");
    SafeCloseThreadHandle(&th);

    // Prefer path::IsSame: DirIter paths may differ from the open path in
    // case, separators, or long/short form, so StrVec::Find can miss.
    int openedIdx = -1;
    for (int i = 0; i < len(pages); i++) {
        if (path::IsSame(pages[i], fileName)) {
            openedIdx = i;
            break;
        }
    }
    currentPageNo = openedIdx >= 0 ? openedIdx + 1 : 1;
    currentPageUrl = nullptr;
    return true;
}

MarkdownModel* MarkdownModel::Create(Str fileName, DocControllerCallback* cb) {
    auto* mm = new MarkdownModel(cb);
    if (!mm->Load(fileName)) {
        delete mm;
        return nullptr;
    }
    return mm;
}
