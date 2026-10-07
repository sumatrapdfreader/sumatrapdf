/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "gui/BrowserView.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "FileThumbnails.h"
#include "AppSettings.h"
#include "Theme.h"
#include "PagePosition.h"
#include "ChmModel.h"

static bool IsBlankUrl(Str url) {
    return str::EqI(StrL("about:blank"), url);
}

static IPageDestination* NewChmNamedDest(Arena* arena, Str url, int pageNo) {
    if (len(url) == 0) {
        return nullptr;
    }
    IPageDestination* dest = nullptr;
    if (IsExternalUrl(url)) {
        dest = arena ? New<PageDestinationURL>(arena, url) : new PageDestinationURL(url);
    } else {
        auto* pdest = arena ? New<PageDestination>(arena) : new PageDestination();
        pdest->kind = kindDestinationScrollTo;
        pdest->name = str::Dup(url);
        dest = pdest;
    }
    dest->pageNo = pageNo;
    ReportIf(!dest->kind);
    dest->rect = RectF(kDestUseDefault, kDestUseDefault, kDestUseDefault, kDestUseDefault);
    return dest;
}

static TocItem* NewChmTocItem(Arena* arena, TocItem* parent, Str title, int pageNo, Str url) {
    auto* res = AllocTocItem(arena, title, pageNo);
    res->parent = parent;
    res->dest = NewChmNamedDest(arena, url, pageNo);
    return res;
}

class BrowserViewHandler : public BrowserViewCallback {
    ChmModel* cm;

  public:
    explicit BrowserViewHandler(ChmModel* cm) : cm(cm) {}
    ~BrowserViewHandler() override = default;

    bool OnBeforeNavigate(Str url, bool newWindow) override { return cm->OnBeforeNavigate(url, newWindow); }
    void OnDocumentComplete(Str url) override { cm->OnDocumentComplete(url); }
    void OnLButtonDown() override { cm->OnLButtonDown(); }
    Str GetDataForUrl(Str url) override { return cm->GetDataForUrl(url); }
    void DownloadData(Str url, Str data) override { cm->DownloadData(url, data); }
    void OnFindResult(int gen, int current, int total) override { cm->OnFindResult(gen, current, total); }
    void OnFindAllResult(Str payload) override { cm->OnFindAllResult(payload); }
};

struct ChmTocTraceItem {
    Str title; // owned by ChmModel::poolAllocator
    Str url;   // owned by ChmModel::poolAllocator
    int level = 0;
    int pageNo = 0;
};

ChmModel::ChmModel(DocControllerCallback* cb) : BrowserDocController(cb) {
    poolAlloc = ArenaNew();
}

ChmModel::~ChmModel() {
    docAccess.Lock();
    BrowserViewDelete(docView);
    delete browserCb;
    delete doc;
    delete tocTrace;
    DestroyTocTree(tocTree);
    DeleteVecMembers(urlDataCache);
    docAccess.Unlock();
    ArenaDelete(poolAlloc);
    str::Free(fileName);
}

// meta data
Str ChmModel::GetFilePath() const {
    return fileName;
}

Str ChmModel::GetDefaultFileExt() const {
    return StrL(".chm");
}

TempStr ChmModel::GetPropertyTemp(DocProp prop) {
    return doc->GetPropertyTemp(prop);
}

// page navigation (stateful)
void ChmModel::GoToPage(int pageNo, bool /*addNavPoint*/) {
    ReportIf(!ValidPageNo(pageNo));
    if (!ValidPageNo(pageNo)) {
        return;
    }
    // re-display the exact current url (which may be a redirect/anchor not in
    // `pages`) so navigating to the same page preserves it
    if (pageNo == currentPageNo && len(currentPageUrl) > 0) {
        DisplayPage(currentPageUrl);
        return;
    }
    DisplayPage(pages[pageNo - 1]);
}

// the following is specific to ChmModel
void ChmModel::FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) {
    if (!docView) {
        return;
    }
    // pages are internal chm paths; BrowserViewFindAllPages prefixes the
    // virtual host to make them fetchable
    BrowserViewFindAllPages(docView, pages, term, matchCase, wholeWord, gen);
}

// navigate to pageNo and, once it has loaded, highlight term there and make
// its idx-th match current (see OnDocumentComplete)
bool ChmModel::DisplayPage(Str pageUrl) {
    if (len(pageUrl) == 0) {
        return false;
    }
    // pageUrl may alias currentPageUrl (e.g. via GoToPage), which we overwrite
    // below with SetCopy(); take a stable copy so the later use of pageUrl
    // (NavigateToDataUrl) doesn't read freed memory
    pageUrl = str::DupTemp(pageUrl);
    if (IsExternalUrl(pageUrl)) {
        // open external links in an external browser
        // (same as for PDF, XPS, etc. documents)
        if (cb) {
            IPageDestination* dest = NewChmNamedDest(nullptr, pageUrl, 0);
            cb->GotoLink(dest);
            delete dest;
        }
        return true;
    }

    TempStr url = url::GetFullPathTemp(pageUrl);
    bool wasSameUrl = len(currentPageUrl) > 0 && str::Eq(currentPageUrl, url);
    int pageNo = pages.Find(url) + 1;
    // if we're reloading the same url to restore a scroll position, don't
    // clobber that saved position by saving the current (pre-restore) one
    bool restoreScrollAfterLoad = restoreHtmlScrollPos && wasSameUrl;
    if (!restoreScrollAfterLoad) {
        SaveHtmlScrollPos();
        skipNextBeforeNavigateScrollSave = true;
    }
    str::ReplaceWithCopy(&currentPageUrl, url);
    if (pageNo > 0) {
        currentPageNo = pageNo;
    }

    PointF savedPos;
    if (GetSavedHtmlScrollPosForUrl(url, &savedPos)) {
        htmlScrollPos = savedPos;
        restoreHtmlScrollPos = true;
    } else if (!restoreScrollAfterLoad) {
        restoreHtmlScrollPos = false;
    }

    // This is a hack that seems to be needed for some chm files where
    // url starts with "..\" even though it's not accepted by ie as
    // a correct its: url. There's a possibility it breaks some other
    // chm files (I don't know such cases, though).
    // A more robust solution would try to match with the actual
    // names of files inside chm package.
    str::TrimPrefix(pageUrl, StrL("..\\"));
    str::TrimPrefix(pageUrl, StrL("/"));

    if (!docView) {
        return false;
    }
    BrowserViewNavigate(docView, pageUrl);
    return true;
}

bool ChmModel::HandleLink(IPageDestination* link, ILinkHandler* /*linkHandler*/) {
    Kind k = link->GetKind();
    if (k != kindDestinationScrollTo) {
        logf("ChmModel::HandleLink: unsupported kind '%s'\n", Str(k));
        ReportIf(link->GetKind() != kindDestinationScrollTo);
    }
    Str url = link->GetName();
    if (DisplayPage(url)) {
        return true;
    }
    int pageNo = PageDestGetPageNo(link);
    GoToPage(pageNo, false);
    return true;
}

// view settings
// for quick type determination and type-safe casting
ChmModel* ChmModel::AsChm() {
    return this;
}

BrowserViewCallback* ChmModel::CreateBrowserCallback() {
    return new BrowserViewHandler(this);
}

TempStr ChmModel::NormalizeScrollUrlTemp(Str url) const {
    return url::GetFullPathTemp(url);
}

TempStr ChmModel::ScrollUrlForPageTemp(int pageNo) const {
    return str::DupTemp(pages[pageNo - 1]);
}

struct ChmTocBuilder : EbookTocVisitor {
    ChmFile* doc = nullptr;

    StrVec* pages = nullptr;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;
    Arena* a = nullptr;
    dict::MapStrToInt urlsSet;

    // We fake page numbers by doing a depth-first traversal of
    // toc tree and considering each unique html page in toc tree
    // as a page
    int CreatePageNoForURL(Str url) {
        if (len(url) == 0 || IsExternalUrl(url)) {
            return 0;
        }

        TempStr plainUrl = url::GetFullPathTemp(url);
        int pageNo = len(*pages) + 1;
        bool inserted = urlsSet.Insert(plainUrl, pageNo, &pageNo);
        if (inserted) {
            pages->Append(plainUrl);
            ReportIf(pageNo != len(*pages));
        } else {
            ReportIf(pageNo == len(*pages) + 1);
        }
        return pageNo;
    }

  public:
    ChmTocBuilder(ChmFile* doc, StrVec* pages, Vec<ChmTocTraceItem>* tocTrace, Arena* a) {
        this->doc = doc;
        this->pages = pages;
        this->tocTrace = tocTrace;
        this->a = a;
        int n = len(*pages);
        for (int i = 0; i < n; i++) {
            Str url = pages->At(i);
            bool inserted = urlsSet.Insert(url, i + 1, nullptr);
            ReportIf(!inserted);
        }
    }

    void Visit(Str name, Str url, int level) override {
        Str nameDup = str::Dup(a, name);
        Str urlDup = str::Dup(a, url);
        int pageNo = CreatePageNoForURL(urlDup);
        ChmTocTraceItem item{nameDup, urlDup, level, pageNo};
        VecAppend(*tocTrace, item);
    }
};

bool ChmModel::Load(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    doc = ChmFile::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    // always make the document's homepage page 1
    TempStr page = strconv::AnsiToUtf8Temp(doc->homePath);
    pages.Append(page);

    // parse the ToC here, since page numbering depends on it
    tocTrace = new Vec<ChmTocTraceItem>();
    ChmTocBuilder tmpTocBuilder(doc, &pages, tocTrace, poolAlloc);
    doc->ParseToc(&tmpTocBuilder);
    ReportIf(len(pages) == 0);
    return len(pages) > 0;
}

struct ChmCacheEntry {
    // owned by ChmModel::poolAllocator
    Str url;
    Str data;

    explicit ChmCacheEntry(Str url);
    ~ChmCacheEntry() { str::Free(data); };
};

ChmCacheEntry::ChmCacheEntry(Str url) {
    this->url = url;
}

ChmCacheEntry* ChmModel::FindDataForUrl(Str url) const {
    int n = len(urlDataCache);
    for (int i = 0; i < n; i++) {
        ChmCacheEntry* e = urlDataCache[i];
        if (str::Eq(url, e->url)) {
            return e;
        }
    }
    return nullptr;
}

// Called after html document has been loaded.
// Sync the state of the ui with the page (show
// the right page number, select the right item in toc tree)
void ChmModel::OnDocumentComplete(Str url) {
    if (len(url) == 0 || IsBlankUrl(url)) {
        return;
    }
    if (url.s[0] == '/') {
        url = Str(url.s + 1, url.len - 1);
    }
    TempStr toFind = url::GetFullPathTemp(url);
    str::ReplaceWithCopy(&currentPageUrl, toFind);
    int pageNo = pages.Find(toFind) + 1;
    if (pageNo > 0) {
        currentPageNo = pageNo;
    }

    PointF savedPos;
    if (GetSavedHtmlScrollPosForUrl(toFind, &savedPos)) {
        htmlScrollPos = savedPos;
        restoreHtmlScrollPos = true;
    }

    // setting zoom before the first page is loaded doesn't work, so the
    // intended zoom is applied here instead. Re-apply it after *every* load:
    // the hosted control is recreated when switching tabs, which resets it
    // to 100%.
    if (IsValidZoom(initZoom)) {
        zoomVirtual = initZoom;
        initZoom = kInvalidZoom;
    }
    BrowserViewSetZoomPercent(docView, (int)zoomVirtual);
    RestoreHtmlScrollPos();

    if (cb && pageNo > 0) {
        cb->PageNoChanged(this, pageNo);
    }

    // finish a pending "jump to a match on another page": the fresh document
    // has no find state, so re-run the search and go to the requested match
    FinishPendingFind();
}

// Called before we start loading html for a given url. Will block
// loading if returns false.
// for BrowserViewCallback (called through browserCb)
bool ChmModel::OnBeforeNavigate(Str url, bool newWindow) {
    // save scroll pos of the page we're leaving, unless DisplayPage() already
    // saved it before triggering this programmatic navigation
    if (skipNextBeforeNavigateScrollSave) {
        skipNextBeforeNavigateScrollSave = false;
    } else {
        // user-initiated navigation (e.g. clicking a link): currentPageUrl still
        // refers to the page being left, so save its live scroll position
        SaveHtmlScrollPos();
    }

    // ensure that JavaScript doesn't keep the focus
    // in the browser view when a new page is loaded
    if (cb) {
        cb->FocusFrame(false);
    }

    // external links and new-window requests leave the embedded browser
    // (same as FixedPageUI / SimpleBrowserWindow; issue #5920 for downloads)
    if (newWindow || IsExternalUrl(url)) {
        if (url && cb) {
            IPageDestination* dest = NewChmNamedDest(nullptr, url, 1);
            cb->GotoLink(dest);
            delete dest;
        }
        return false;
    }

    return true;
}

// best-effort theming for CHM pages: we don't control their HTML, so inject
// a <style> block with !important overrides for the page background and text
// color. Returns null when the effective page colors are the plain default
// (black on white) — i.e. nothing to override.
static TempStr ChmThemeStyleTemp() {
    Color bgCol;
    Color txtCol = ThemePageRenderColors(bgCol);
    bool isDefault = (bgCol == kColWhite) && (txtCol == kColBlack);
    if (isDefault) {
        return {};
    }
    bool dark = !IsLightColor(bgCol);
    TempStr bg = ColorToCssTemp(bgCol);
    TempStr fg = ColorToCssTemp(txtCol);
    Str link = dark ? StrL("#4493f8") : StrL("#0969da");
    TempStr border = ColorToCssTemp(AccentColor(bgCol, 25));
    // force text color on all elements (pages with explicit dark colors would
    // otherwise be invisible on a dark background) but keep links recognizable;
    // clear element backgrounds so the page background shows through
    return fmt(
        "<style>"
        "html,body{background-color:%s !important;}"
        "*{color:%s !important;background-color:transparent !important;border-color:%s !important;}"
        "a,a *{color:%s !important;}"
        "</style>",
        bg, fg, border, link);
}

// insert `style` into `raw` (an HTML page): after the <head> tag when present
// so the doctype stays first (avoids quirks mode), else before <body>, else at
// the start. Returns a heap copy.
static Str ChmInjectStyle(Str raw, Str style) {
    int insertAt = 0;
    int headIdx = str::IndexOfI(raw, StrL("<head"));
    if (headIdx >= 0) {
        for (int i = headIdx; i < raw.len; i++) {
            if (raw.s[i] == '>') {
                insertAt = i + 1;
                break;
            }
        }
    } else {
        int bodyIdx = str::IndexOfI(raw, StrL("<body"));
        if (bodyIdx >= 0) {
            insertAt = bodyIdx;
        }
    }
    str::Builder b;
    b.Append(Str(raw.s, insertAt));
    b.Append(style);
    b.Append(Str(raw.s + insertAt, raw.len - insertAt));
    return b.TakeStr();
}

// returns a heap copy of `raw` (the CHM resource for a url), with a theme
// <style> injected when `raw` is an HTML page and the color mode wants
// non-default page colors
static Str ChmThemeApplyToData(Str raw) {
    TempStr style = ChmThemeStyleTemp();
    if (len(style) == 0) {
        return str::Dup(raw);
    }
    bool isHtml = str::IndexOfI(raw, StrL("<html")) >= 0 || str::IndexOfI(raw, StrL("<head")) >= 0 ||
                  str::IndexOfI(raw, StrL("<body")) >= 0;
    if (!isHtml) {
        return str::Dup(raw);
    }
    return ChmInjectStyle(raw, style);
}

Str ChmModel::GetDataForUrl(Str url) {
    ScopedMutex scope(&docAccess);
    TempStr plainUrl = url::GetFullPathTemp(url);
    ChmCacheEntry* e = FindDataForUrl(plainUrl);
    if (!e) {
        Str raw = doc->GetDataTemp(plainUrl);
        if (len(raw) == 0) {
            return {};
        }
        Str s = str::Dup(poolAlloc, plainUrl);
        e = new ChmCacheEntry(s);
        e->data = ChmThemeApplyToData(raw);
        VecAppend(urlDataCache, e);
    }
    return e->data;
}

// theme colors are baked into the served HTML: drop the cached pages and
// reload the current one with the new colors (a hidden tab has no docView and
// regenerates when re-selected)
void ChmModel::UpdateTheme() {
    {
        ScopedMutex scope(&docAccess);
        DeleteVecMembers(urlDataCache);
        VecReset(urlDataCache);
    }
    if (docView && len(currentPageUrl) > 0) {
        SaveHtmlScrollPos();
        restoreHtmlScrollPos = true;
        DisplayPage(currentPageUrl);
    }
}

// named destinations are either in-document URLs or Alias topic IDs.
// engine-owned; do not delete
IPageDestination* ChmModel::GetNamedDest(Str name) {
    TempStr url = url::GetFullPathTemp(name);
    int pageNo = pages.Find(url) + 1;
    if (pageNo >= 1) {
        return NewChmNamedDest(poolAlloc, url, pageNo);
    }
    if (doc->HasData(url)) {
        return NewChmNamedDest(poolAlloc, url, 1);
    }
    unsigned int topicID;
    if (str::IsNull(str::Parse(name, "%u%$", &topicID))) {
        return nullptr;
    }
    TempStr topicURL = doc->ResolveTopicID(topicID);
    if (len(topicURL) == 0) {
        return nullptr;
    }
    url = topicURL;
    if (!doc->HasData(url)) {
        return nullptr;
    }
    pageNo = pages.Find(url) + 1;
    // some documents use redirection URLs which aren't listed in the ToC
    // return pageNo=1 for these, as HandleLink will ignore that anyway
    // but LinkHandler::ScrollTo doesn't
    pageNo = std::max(pageNo, 1);
    return NewChmNamedDest(poolAlloc, url, pageNo);
}

// table of contents
TocTree* ChmModel::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (len(*tocTrace) == 0) {
        return nullptr;
    }

    TocItem* root = nullptr;
    bool foundRoot = false;
    TocItem** nextChild = &root;
    Vec<TocItem*> levels;
    int idCounter = 0;

    for (ChmTocTraceItem& ti : *tocTrace) {
        TocItem* item = NewChmTocItem(poolAlloc, nullptr, ti.title, ti.pageNo, ti.url);
        item->id = ++idCounter;
        // append the item at the correct level
        ReportIf(ti.level < 1);
        if (ti.level <= len(levels)) {
            VecRemoveAtN(levels, ti.level, len(levels) - ti.level);
            VecLast(levels)->AddSiblingAtEnd(item);
        } else {
            *nextChild = item;
            VecAppend(levels, item);
            foundRoot = true;
        }
        nextChild = &item->child;
    }
    if (!foundRoot) {
        return nullptr;
    }
    auto* realRoot = AllocTocItem(poolAlloc, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(poolAlloc, realRoot);
    return tocTree;
}

// adapted from DisplayModel::NextZoomStep
// Platform thumbnail service renders the CHM home page when supported.
void ChmModel::CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) {
    CreateChmThumbnail(fileName, size, saveThumbnail);
}

bool ChmModel::IsSupportedFileType(FileType kind) {
    return kind == FileType::Chm;
}

ChmModel* ChmModel::Create(Str fileName, DocControllerCallback* cb) {
    ChmModel* cm = new ChmModel(cb);
    if (!cm->Load(fileName)) {
        delete cm;
        return nullptr;
    }
    return cm;
}
