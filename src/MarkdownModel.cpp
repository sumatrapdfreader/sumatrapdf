/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/HtmlWindow.h"
#include "wingui/ChmDocView.h"
#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"

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

static TempStr RelPathFromBaseTemp(Str filePath, Str baseDir) {
    TempStr normFile = path::NormalizeTemp(filePath);
    TempStr normBase = path::NormalizeTemp(baseDir);
    if (!normBase || !str::StartsWith(normFile, normBase)) {
        return path::GetBaseNameTemp(filePath);
    }
    Str rel = Str(normFile.s + normBase.len, normFile.len - normBase.len);
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

static IPageDestination* NewMarkdownNamedDest(Str url, int pageNo) {
    if (!url) {
        return nullptr;
    }
    IPageDestination* dest = nullptr;
    if (IsMarkdownExternalUrl(url)) {
        dest = new PageDestinationURL(url);
    } else {
        auto pdest = new PageDestination();
        pdest->kind = kindDestinationScrollTo;
        pdest->name = str::Dup(url);
        dest = pdest;
    }
    dest->pageNo = pageNo;
    dest->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    return dest;
}

static TocItem* NewMarkdownTocItem(TocItem* parent, Str title, int pageNo, Str url) {
    auto res = new TocItem(parent, title, pageNo);
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
    return ".md";
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

TempStr MarkdownModel::FileToVirtualUrlTemp(Str filePath) const {
    if (!filePath) {
        return {};
    }
    TempStr rel = RelPathFromBaseTemp(filePath, baseDir);
    if (!rel) {
        rel = path::GetBaseNameTemp(filePath);
    }
    rel = str::ReplaceTemp(rel, StrL("\\"), StrL("/"));
    Str relStr = rel;
    if (str::EndsWithI(relStr, StrL(".markdown"))) {
        relStr.len -= 9;
    } else if (str::EndsWithI(relStr, StrL(".md"))) {
        relStr.len -= 3;
    }
    return fmt("%s%s.html", Str(kMdVirtualHost, kMdVirtualHostLen), relStr);
}

TempStr MarkdownModel::VirtualUrlToFileTemp(Str url) const {
    if (!url || !str::StartsWith(url, kMdVirtualHost)) {
        return {};
    }
    Str pathPart = Str(url.s + kMdVirtualHostLen, url.len - kMdVirtualHostLen);
    Str fragment = str::SliceFromChar(pathPart, '#');
    if (fragment) {
        pathPart = Str(pathPart.s, (int)(fragment.s - pathPart.s));
    }
    if (str::EndsWithI(pathPart, StrL(".html"))) {
        pathPart.len -= 5;
    }
    TempStr rel = str::ReplaceTemp(pathPart, StrL("/"), StrL("\\"));
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
    if (docView || htmlWindowCb) {
        RemoveParentHwnd();
    }
    htmlWindowCb = new MarkdownHtmlWindowHandler(this);
    docView = ChmDocView::Create(hwnd, htmlWindowCb, Str(kMdVirtualHost));
    if (!docView) {
        delete htmlWindowCb;
        htmlWindowCb = nullptr;
        return false;
    }
    return true;
}

void MarkdownModel::RemoveParentHwnd() {
    if (!docView && !htmlWindowCb) {
        return;
    }
    SaveHtmlScrollPos();
    restoreHtmlScrollPos = true;
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

void MarkdownModel::FindStart(Str term, bool matchCase, bool wholeWord, int gen) const {
    if (docView) {
        docView->FindStart(term, matchCase, wholeWord, gen, -1);
    }
}

void MarkdownModel::FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) const {
    if (!docView) {
        return;
    }
    StrVec urls;
    for (Str page : pages) {
        urls.Append(FileToVirtualUrlTemp(page));
    }
    docView->FindAllPages(urls, term, matchCase, wholeWord, gen);
}

void MarkdownModel::FindGoto(int idx) const {
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

void MarkdownModel::FindClear() const {
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

bool MarkdownModel::DisplayPage(Str pageUrl) {
    if (!pageUrl) {
        return false;
    }
    pageUrl = str::DupTemp(pageUrl);
    if (IsMarkdownExternalUrl(pageUrl)) {
        if (cb) {
            auto item = NewMarkdownTocItem(nullptr, nullptr, 1, pageUrl);
            cb->GotoLink(item->dest);
            delete item;
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
        TempStr navUrl = plainUrl;
        if (str::StartsWith(navUrl, kMdVirtualHost)) {
            navUrl = Str(navUrl.s + kMdVirtualHostLen, navUrl.len - kMdVirtualHostLen);
        }
        docView->NavigateToDataUrl(navUrl);
    }
    return true;
}

void MarkdownModel::GoToPage(int pageNo, bool) {
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

bool MarkdownModel::HandleLink(IPageDestination* link, ILinkHandler*) {
    Str url = PageDestGetName(link);
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

void MarkdownModel::SetDisplayMode(DisplayMode, bool) {}

DisplayMode MarkdownModel::GetDisplayMode() const {
    return DisplayMode::SinglePage;
}

void MarkdownModel::SetInPresentation(bool) {}

void MarkdownModel::SetViewPortSize(Size) {}

MarkdownModel* MarkdownModel::AsMarkdown() {
    return this;
}

void MarkdownModel::SetZoomVirtual(float zoom, Point*) {
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
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    docView->SetScrollPos(Point(x, y));
}

void MarkdownModel::ZoomTo(float zoomLevel) const {
    if (docView) {
        docView->SetZoomPercent((int)zoomLevel);
    }
}

float MarkdownModel::GetZoomVirtual(bool) const {
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
    if (iCurrZoom < towardsLevel) {
        for (int i = 0; i < nZoomLevels; i++) {
            int iZoom = (int)zoomLevels[i];
            if (iZoom > iCurrZoom) {
                iNewZoom = iZoom;
                break;
            }
        }
    } else if (iCurrZoom > towardsLevel) {
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
    if (!newWindow) {
        return true;
    }
    TempStr plainUrl = NormalizeMarkdownUrlTemp(url);
    if (plainUrl && !IsMarkdownExternalUrl(plainUrl)) {
        DisplayPage(plainUrl);
        return false;
    }
    if (url && cb) {
        auto item = NewMarkdownTocItem(nullptr, nullptr, 1, url);
        cb->GotoLink(item->dest);
        delete item;
    }
    return false;
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

    TempStr filePath = VirtualUrlToFileTemp(plainUrl);
    Str data;
    if (filePath && (str::EndsWithI(filePath, StrL(".md")) || str::EndsWithI(filePath, StrL(".markdown")) ||
                     str::EndsWithI(filePath, StrL(".html")))) {
        Str md = file::ReadFile(filePath);
        if (md) {
            data = MarkdownToHtmlPage(md);
        }
    } else if (filePath) {
        data = file::ReadFile(filePath);
    }

    if (!data) {
        return {};
    }

    Str urlDup = str::Dup(poolAlloc, plainUrl);
    e = new MarkdownCacheEntry{urlDup, str::Dup(poolAlloc, data)};
    urlDataCache.Append(e);
    return e->data;
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
    if (pageNo < 1) {
        pageNo = 1;
    }
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

void MarkdownModel::CreateThumbnail(Size, const OnBitmapRendered*) {}

bool MarkdownModel::IsSupportedFileType(FileType kind) {
    return kind == FileType::Markdown;
}

bool MarkdownModel::Load(Str fileName) {
    str::ReplaceWithCopy(&this->fileName, fileName);
    str::ReplaceWithCopy(&baseDir, path::GetDirTemp(fileName));

    StrVec mdFiles;
    CollectMarkdownFiles(baseDir, fileName, mdFiles);
    if (len(mdFiles) == 0) {
        return false;
    }

    pages = mdFiles;
    Vec<MarkdownFileToc> fileTocs;
    ParseMarkdownTocsParallel(pages, fileTocs);

    Vec<MarkdownTocTraceItem> tocTrace;
    int idCounter = 0;
    TocItem* root = nullptr;
    Vec<TocItem*> levels;

    for (int i = 0; i < len(fileTocs); i++) {
        MarkdownFileToc& ft = fileTocs[i];
        int pageNo = i + 1;
        TempStr pageUrl = FileToVirtualUrlTemp(ft.filePath);
        Str fileTitle = str::Dup(poolAlloc, path::GetBaseNameTemp(ft.filePath));
        if (i != 0) {
            if (str::EndsWithI(fileTitle, StrL(".markdown"))) {
                fileTitle.len -= 9;
            } else if (str::EndsWithI(fileTitle, StrL(".md"))) {
                fileTitle.len -= 3;
            }
        }

        MarkdownTocTraceItem fileItem;
        fileItem.title = fileTitle;
        fileItem.url = str::Dup(poolAlloc, pageUrl);
        fileItem.level = 1;
        fileItem.pageNo = pageNo;
        tocTrace.Append(fileItem);

        for (MarkdownHeadingItem& hi : ft.headings) {
            TempStr destUrl = pageUrl;
            if (hi.anchor) {
                destUrl = str::JoinTemp(pageUrl, fmt("#%s", hi.anchor));
            }
            MarkdownTocTraceItem hItem;
            hItem.title = str::Dup(poolAlloc, hi.title);
            hItem.url = str::Dup(poolAlloc, destUrl);
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

    TocItem** nextChild = &root;
    bool foundRoot = false;
    levels.Reset();
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

    if (foundRoot) {
        auto realRoot = new TocItem();
        realRoot->child = root;
        tocTree = new TocTree(realRoot);
    }

    int openedIdx = pages.Find(fileName);
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