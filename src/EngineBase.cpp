
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "DocProperties.h"
#include "EngineBase.h"

Kind kindPageElementDest = "dest";
Kind kindPageElementImage = "image";
Kind kindPageElementComment = "comment";

Kind kindDestinationNone = "none";
Kind kindDestinationScrollTo = "scrollTo";
Kind kindDestinationLaunchURL = "launchURL";
Kind kindDestinationLaunchEmbedded = "launchEmbedded";
Kind kindDestinationAttachment = "launchAttachment";
Kind kindDestinationLaunchFile = "launchFile";
Kind kindDestinationDjVu = "destinationDjVu";
Kind kindDestinationMupdf = "destinationMupdf";
Kind kindDestinationJsMenu = "jsMenu";

// clang-format off
static Kind destKinds[] = {
    kindDestinationNone,
    kindDestinationScrollTo,
    kindDestinationLaunchURL,
    kindDestinationLaunchEmbedded,
    kindDestinationAttachment,
    kindDestinationLaunchFile,
    kindDestinationDjVu,
    kindDestinationMupdf,
    kindDestinationJsMenu
};
// clang-format on

bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://")) ||
           str::StartsWithI(url, StrL("mailto:"));
}

static void EnsurePageText(PageText* pageText) {
    if (pageText->text) {
        if (pageText->len == 0) {
            pageText->len = pageText->text.len;
        }
        if (pageText->nCodepoints == 0) {
            pageText->nCodepoints = Utf8CodepointCount(pageText->text);
        }
        return;
    }
    // TakeStr()/Vec::Take() can allocate backing storage even for empty pages.
    str::Free(pageText->text);
    free((void*)pageText->coords);
    free((void*)pageText->quads);
    pageText->text = {};
    pageText->coords = nullptr;
    pageText->quads = nullptr;
    pageText->len = 0;
    pageText->nCodepoints = 0;
}

void FreePageText(PageText* pageText) {
    str::Free(pageText->text);
    free((void*)pageText->coords);
    free((void*)pageText->quads);
    pageText->text = {};
    pageText->coords = nullptr;
    pageText->quads = nullptr;
    pageText->len = 0;
    pageText->nCodepoints = 0;
}

PageDestination::~PageDestination() {
    str::Free(value);
    str::Free(name);
}

// string value associated with the destination (e.g. a path or a URL)
Str PageDestination::GetValue2() {
    return value;
}

// the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
// (mainly applicable for links of type "LaunchFile" to PDF documents)
Str PageDestination::GetName2() {
    return name;
}

PageDestinationJsMenu::PageDestinationJsMenu() {
    kind = kindDestinationJsMenu;
    pageNo = -1;
}

PageDestinationJsMenu::~PageDestinationJsMenu() {
    str::Free(tooltip);
}

// Hover text: one menu line per row, skipping "-" separators.
Str PageDestinationJsMenu::GetValue2() {
    if (tooltip) {
        return tooltip;
    }
    if (len(items) == 0) {
        return {};
    }
    str::Builder b;
    for (int i = 0; i < len(items); i++) {
        Str it = items[i];
        if (str::Eq(it, StrL("-"))) {
            continue;
        }
        if (len(b) > 0) {
            b.AppendChar('\n');
        }
        b.Append(it);
    }
    tooltip = b.TakeStr();
    return tooltip;
}

IPageDestination* NewSimpleDest(Arena* arena, int pageNo, RectF rect, float zoom, Str value) {
    if (value) {
        return arena ? New<PageDestinationURL>(arena, value) : new PageDestinationURL(value);
    }
    auto* res = arena ? New<PageDestination>(arena) : new PageDestination();
    res->pageNo = pageNo;
    res->rect = rect;
    res->kind = kindDestinationScrollTo;
    res->zoom = zoom;
    return res;
}

IPageDestination* NewSimpleDest(int pageNo, RectF rect, float zoom, Str value) {
    return NewSimpleDest(nullptr, pageNo, rect, zoom, value);
}

bool IPageElement::Is(Kind expectedKind) {
    return kind == expectedKind;
}

Kind kindTocFzOutline = "tocFzOutline";
Kind kindTocFzOutlineAttachment = "tocFzOutlineAttachment";
Kind kindTocFzLink = "tocFzLink";
Kind kindTocDjvu = "tocDjvu";

// Sanitize a string for display in a single-line tree-view control (e.g. a
// bookmark/TOC label): drop soft hyphens and turn control chars / line
// separators into spaces, so they don't render as a stray hyphen or as
// boxes (#2647).
static TempStr CleanupTreeViewControlStringTemp(Str s) {
    if (len(s) == 0) {
        return {};
    }
    TempWStr ws = ToWStrTemp(s);
    // soft hyphen (U+00AD): an invisible line-break hint, but rendered as a
    // visible hyphen by some fonts
    wstr::RemoveCharsInPlace(ws, L"\x00ad");
    // control chars (incl. embedded newlines/tabs) and the Unicode line and
    // paragraph separators render as boxes in a single-line label
    for (int i = 0; i < ws.len; i++) {
        wchar_t c = ws.s[i];
        if (c < 0x20 || c == 0x7f || c == 0x2028 || c == 0x2029) {
            ws.s[i] = L' ';
        }
    }
    // collapse the runs of whitespace we just introduced (and trim)
    wstr::NormalizeWSInPlace(ws);
    return ToUtf8Temp(ws);
}

TocItem* AllocTocItem(Arena* arena, Str title, int pageNo) {
    auto* item = (TocItem*)AllocZero(arena, sizeof(TocItem));
    item->title = str::Dup(arena, CleanupTreeViewControlStringTemp(title));
    item->pageNo = pageNo;
    item->color = kColorUnset;
    return item;
}

void FreeTocItemRec(Arena* arena, TocItem* item) {
    if (!item) {
        return;
    }
    FreeTocItemRec(arena, item->child);
    // arena dests: destructor only; heap dests: delete
    if (!item->destNotOwned && item->dest) {
        if (arena) {
            item->dest->~IPageDestination();
        } else {
            delete item->dest;
        }
        item->dest = nullptr;
    }
    FreeTocItemRec(arena, item->next);
    Free(arena, item->title.s);
    Free(arena, item);
}

void TocItem::AddSibling(TocItem* sibling) {
    TocItem* currNext = next;
    next = sibling;
    sibling->next = currNext;
    sibling->parent = parent;
}

void TocItem::AddSiblingAtEnd(TocItem* sibling) {
    TocItem* item = this;
    while (item->next) {
        item = item->next;
    }
    item->next = sibling;
    sibling->parent = item->parent;
}

void TocItem::AddChild(TocItem* newChild) {
    TocItem* curr = child;
    child = newChild;
    newChild->parent = this;
    newChild->next = curr;
}

// returns the destination this ToC item points to or nullptr
// (the result is owned by the TocItem and MUST NOT be deleted)
// TODO: rename to GetDestination()
IPageDestination* TocItem::GetPageDestination() const {
    return dest;
}

int TocItem::ChildCount() {
    int n = 0;
    auto* node = child;
    while (node) {
        n++;
        node = node->next;
    }
    return n;
}

TocItem* TocItem::ChildAt(int n) {
    if (n == 0) {
        currChild = child;
        currChildNo = 0;
        return child;
    }
    // speed up sequential iteration over children
    if (currChild != nullptr && n == currChildNo + 1) {
        currChild = currChild->next;
        ++currChildNo;
        return currChild;
    }
    auto* node = child;
    while (n > 0) {
        n--;
        node = node->next;
    }
    return node;
}

bool TocItem::IsExpanded() {
    // leaf items cannot be expanded
    if (child == nullptr) {
        return false;
    }
    // item is expanded when:
    // - expanded by default, not toggled (true, false)
    // - not expanded by default, toggled (false, true)
    // which boils down to:
    return isOpenDefault != isOpenToggled;
}

bool TocItem::PageNumbersMatch() const {
    int destPageNo = PageDestGetPageNo(dest);
    if (destPageNo <= 0) {
        return true; // TODO: should be false?
    }
    if (pageNo != destPageNo) {
        logf("pageNo: %d, dest->pageNo: %d\n", pageNo, destPageNo);
        return false;
    }
    return true;
}

TocTree* AllocTocTree(Arena* arena, TocItem* root) {
    return New<TocTree>(arena, root, arena);
}

void DestroyTocTree(TocTree* tree) {
    if (tree) {
        tree->~TocTree();
    }
}

TocTree::TocTree(TocItem* root, Arena* arena) {
    this->root = root;
    this->arena = arena;
}

// arena items are not heap-freed; dests still run their destructor
TocTree::~TocTree() {
    FreeTocItemRec(arena, root);
    root = nullptr;
}

// TreeModel
TreeItem TocTree::Root() {
    return (TreeItem)root;
}

Str TocTree::Text(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->title;
}

TreeItem TocTree::Parent(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->parent;
}

int TocTree::ChildCount(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->ChildCount();
}

TreeItem TocTree::ChildAt(TreeItem ti, int idx) {
    auto* tocItem = (TocItem*)ti;
    return (TreeItem)tocItem->ChildAt(idx);
}

bool TocTree::IsExpanded(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return tocItem->IsExpanded();
}

bool TocTree::IsChecked(TreeItem ti) {
    auto* tocItem = (TocItem*)ti;
    return !tocItem->isUnchecked;
}

void TocTree::SetUserData(TreeItem ti, uintptr_t userData) {
    ReportIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    tocItem->userData = userData;
}

uintptr_t TocTree::GetUserData(TreeItem ti) {
    ReportIf(ti < 0);
    TocItem* tocItem = (TocItem*)ti;
    return tocItem->userData;
}

static void ResolveTocPagesRec(EngineBase* engine, TocItem* item) {
    for (; item; item = item->next) {
        if (item->pageNo < 1) {
            IPageDestination* dest = item->GetPageDestination();
            if (dest && dest->loc.chapter >= 1) {
                item->loc = engine->ResolveDest(dest);
                item->pageNo = engine->PageNoFromLocation(item->loc);
            }
        }
        ResolveTocPagesRec(engine, item->child);
    }
}

// resolves lazy chaptered TOC destinations (pageNo == -1 until clicked) to
// real page numbers; for callers that need every item's page up front, after
// the caller has already laid out every chapter (dump, full-document search)
void ResolveTocPages(EngineBase* engine, TocTree* toc) {
    if (!engine || !toc) {
        return;
    }
    ResolveTocPagesRec(engine, toc->root);
}

void EnsureFullLayout(EngineBase* engine) {
    if (!engine) {
        return;
    }
    engine->EnsureAllChaptersLaidOut();
}

// TODO: speed up by removing recursion
static bool VisitTocTree(TocItem* ti, const VisitTocTreeCb& f) {
    bool cont;
    VisitTocTreeData d;
    while (ti) {
        d.ti = ti;
        f.Call(&d);
        cont = !d.stopTraversal;
        if (cont && ti->child) {
            cont = VisitTocTree(ti->child, f);
        }
        if (!cont) {
            return false;
        }
        ti = ti->next;
    }
    return true;
}

static bool VisitTocTreeWithParentRecursive(TocItem* ti, TocItem* parent, const VisitTocTreeCb& f) {
    bool cont;
    VisitTocTreeData d;
    while (ti) {
        d.ti = ti;
        d.parent = parent;
        f.Call(&d);
        cont = !d.stopTraversal;
        if (cont && ti->child) {
            cont = VisitTocTreeWithParentRecursive(ti->child, ti, f);
        }
        if (!cont) {
            return false;
        }
        ti = ti->next;
    }
    return true;
}

RenderPageArgs::RenderPageArgs(int pageNo, float zoom, int rotation, RectF* pageRect, RenderTarget target,
                               AbortCookie** cookie_out) {
    this->pageNo = pageNo;
    this->zoom = zoom;
    this->rotation = rotation;
    this->pageRect = pageRect;
    this->target = target;
    this->cookie_out = cookie_out;
}

// per-chapter cached text, indexed [chapter - 1][page - 1]. A chapter's
// vectors are (re)sized to ChapterPageCount(chapter) on first use for that
// chapter, so a chapter laid out later never re-associates cached text with
// the wrong page.
struct ChapterTextCache {
    Vec<PageText> text;
    Vec<TextExtractionState> state;
};

struct PageTextCache {
    Vec<ChapterTextCache*> chapters; // index = chapter - 1; entries lazily created

    ~PageTextCache() {
        for (int i = 0; i < len(chapters); i++) {
            ChapterTextCache* ct = chapters[i];
            if (!ct) {
                continue;
            }
            for (int j = 0; j < len(ct->text); j++) {
                FreePageText(&ct->text[j]);
            }
            delete ct;
        }
    }

    // existing chapter cache, or nullptr if the chapter has never been touched
    ChapterTextCache* Peek(int chapter) {
        int idx = chapter - 1;
        if (idx < 0 || idx >= len(chapters)) {
            return nullptr;
        }
        return chapters[idx];
    }

    // creates the chapter's cache if needed and grows it to at least count
    // entries; nullptr for an out-of-range chapter
    ChapterTextCache* Ensure(int chapter, int count) {
        if (chapter < 1) {
            return nullptr;
        }
        if (chapter > len(chapters)) {
            VecResize(chapters, chapter);
        }
        ChapterTextCache*& ct = chapters[chapter - 1];
        if (!ct) {
            ct = new ChapterTextCache();
        }
        count = count < 1 ? 1 : count;
        if (len(ct->text) < count) {
            VecResize(ct->text, count);
            VecResize(ct->state, count);
        }
        return ct;
    }
};

int EngineBase::AddRef() {
    return AtomicRefCountAdd(&refCount);
}

// return true if deleted the object
bool EngineBase::Release() {
    int rc = AtomicRefCountDec(&refCount);
    if (rc == 0) {
        delete this;
        return true;
    }
    return false;
}

EngineBase::EngineBase() {
    arena = ArenaNew();
    pageTextCache = new PageTextCache();
}

// EnsureChapterTable() is called by every non-virtual chapter method below. It
// lazily Init()s the table on first use, and for a plain (non-chaptered)
// engine that just sets pageCount directly, keeps chapter 1's count in sync
// with it so ChapterCount() == 1, LocationFromPageNo(n) == {1, n} always hold.
void EngineBase::EnsureChapterTable() {
    if (chapters.ChapterCount() == 0) {
        chapters.Init(1);
        chapters.SetPageCount(1, pageCount < 1 ? 1 : pageCount);
        return;
    }
    if (!HasChapters() && pageCount >= 1 && chapters.PageCount(1) != pageCount) {
        chapters.SetPageCount(1, pageCount);
    }
}

// keeps the flat pageCount total in sync with the chapter table and notifies
// onLayoutChanged (if set) when the generation actually moved, so a
// DisplayModel resyncs even when the layout happened on a render thread
void EngineBase::SetPageCountFromChapters() {
    pageCount = chapters.TotalPages();
    int gen = chapters.Generation();
    if (gen == notifiedGeneration) {
        return;
    }
    notifiedGeneration = gen;
    onLayoutChanged.Call();
}

int EngineBase::ChapterCount() {
    EnsureChapterTable();
    return chapters.ChapterCount();
}

bool EngineBase::HasChapters() {
    return chapters.ChapterCount() > 1;
}

int EngineBase::ChapterPageCount(int chapter) {
    EnsureChapterTable();
    if (chapter < 1 || chapter > chapters.ChapterCount()) {
        ReportIf(true);
        return 0;
    }
    if (!chapters.IsLaidOut(chapter)) {
        LayOutChapter(chapter);
        SetPageCountFromChapters();
    }
    return chapters.PageCount(chapter);
}

bool EngineBase::IsChapterLaidOut(int chapter) {
    EnsureChapterTable();
    return chapters.IsLaidOut(chapter);
}

Location EngineBase::LocationFromPageNo(int pageNo) {
    EnsureChapterTable();
    return chapters.LocationFromPageNo(pageNo);
}

int EngineBase::PageNoFromLocation(Location loc) {
    EnsureChapterTable();
    if (loc.chapter >= 1 && loc.chapter <= chapters.ChapterCount()) {
        ChapterPageCount(loc.chapter); // lay out loc.chapter first
    }
    return chapters.PageNoFromLocation(loc);
}

// next page, crossing into the following chapter at a chapter's last page;
// unchanged at the last page of the last chapter
Location EngineBase::NextLocation(Location loc) {
    loc = ClampLocation(loc);
    int n = ChapterPageCount(loc.chapter);
    if (loc.page < n) {
        return {loc.chapter, loc.page + 1};
    }
    if (loc.chapter < ChapterCount()) {
        return {loc.chapter + 1, 1};
    }
    return loc;
}

// previous page, crossing into the prior chapter's last page (laying it out
// if needed); unchanged at page 1 of chapter 1
Location EngineBase::PrevLocation(Location loc) {
    loc = ClampLocation(loc);
    if (loc.page > 1) {
        return {loc.chapter, loc.page - 1};
    }
    if (loc.chapter > 1) {
        int n = ChapterPageCount(loc.chapter - 1);
        return {loc.chapter - 1, n};
    }
    return loc;
}

Location EngineBase::FirstLocation() {
    EnsureChapterTable();
    return {1, 1};
}

Location EngineBase::LastLocation() {
    EnsureChapterTable();
    int c = ChapterCount();
    int n = ChapterPageCount(c);
    return {c, n};
}

Location EngineBase::ClampLocation(Location loc) {
    EnsureChapterTable();
    int c = limitValue(loc.chapter, 1, ChapterCount());
    int n = ChapterPageCount(c);
    int p = limitValue(loc.page, 1, n);
    return {c, p};
}

int EngineBase::LayoutGeneration() {
    EnsureChapterTable();
    return chapters.Generation();
}

// print / dump / full-document search / PDF export / stress test: today's open
// cost, paid only when the caller actually needs every chapter laid out
void EngineBase::EnsureAllChaptersLaidOut() {
    EnsureChapterTable();
    int n = ChapterCount();
    for (int c = 1; c <= n; c++) {
        ChapterPageCount(c);
    }
}

// default: single-chapter (or already laid-out) engines have nothing to do
int EngineBase::LayOutChapter(int chapter) {
    int n = chapters.PageCount(chapter);
    chapters.SetPageCount(chapter, n);
    return n;
}

TempStr EngineBase::MakeBookmarkTemp(Location loc) {
    int n = ChapterPageCount(loc.chapter);
    return fmt("%d:%d:%d", loc.chapter, loc.page, n);
}

// "chapter:page:pagesInChapterWhenSaved"; scales page proportionally when the
// chapter's page count changed since the bookmark was made (re-pagination)
Location EngineBase::LookupBookmark(Str s) {
    int chapter = 0;
    int page = 0;
    int savedCount = 0;
    Str end = str::Parse(s, "%d:%d:%d%$", &chapter, &page, &savedCount);
    if (str::IsNull(end) || chapter < 1 || page < 1 || savedCount < 1) {
        return kInvalidLocation;
    }
    // the document may have changed since the bookmark was made and lost chapters
    int cnt = ChapterCount();
    chapter = chapter > cnt ? cnt : chapter;
    int newCount = ChapterPageCount(chapter);
    if (newCount >= 1 && savedCount != newCount) {
        page = (int)roundf((float)page * (float)newCount / (float)savedCount);
        page = page < 1 ? 1 : page;
    }
    return ClampLocation({chapter, page});
}

Location EngineBase::ResolveDest(IPageDestination* dest) {
    if (!dest) {
        return kInvalidLocation;
    }
    if (dest->loc.IsValid()) {
        return dest->loc;
    }
    if (dest->pageNo >= 1) {
        dest->loc = LocationFromPageNo(dest->pageNo);
        return dest->loc;
    }
    return kInvalidLocation;
}

// document errors (mupdf warnings/errors may arrive from render threads)
void EngineBase::AppendError(Str msg) {
    ScopedMutex scope(&errorsLock);
    errors.Append(msg);
}

bool EngineBase::HasErrors() {
    ScopedMutex scope(&errorsLock);
    return len(errors) > 0;
}

// internal builder buffer (no copy); valid until next AppendError or engine
// destruction — do not free or keep beyond the current frame
TempStr EngineBase::GetErrorsTextTemp() {
    ScopedMutex scope(&errorsLock);
    return ToStr(errors);
}

EngineBase::~EngineBase() {
    delete pageTextCache;
    str::Free(defaultExt);
    LogArenaStats(StrL("engine"), arena);
    ArenaDelete(arena);
}

struct TextExtractionThreadData {
    EngineBase* engine = nullptr;
    int pageNo = 0;
};

static void ExtractTextThread(TextExtractionThreadData* data) {
    data->engine->GetTextForPage(data->pageNo);
    data->engine->ReleaseTextExtractionThreadContext();
    data->engine->Release();
    delete data;
    AtomicIntDec(&gDangerousThreadCount);
}

// cached per-page text. First call on a page extracts text and caches it,
// subsequent calls return the cached copy. The returned pointers are owned
// by EngineBase and remain valid for the lifetime of the engine.
bool EngineBase::HasTextForPage(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return false;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        return false;
    }
    ScopedMutex scope(&textCacheLock);
    ChapterTextCache* ct = pageTextCache->Peek(loc.chapter);
    if (!ct || loc.page > len(ct->text)) {
        return false;
    }
    return (bool)ct->text[loc.page - 1].text;
}

TextExtractionState EngineBase::GetTextExtractionState(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return TextExtractionState::Finished;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        return TextExtractionState::NotExtracted;
    }
    ScopedMutex scope(&textCacheLock);
    ChapterTextCache* ct = pageTextCache->Peek(loc.chapter);
    if (!ct || loc.page > len(ct->state)) {
        return TextExtractionState::NotExtracted;
    }
    return ct->state[loc.page - 1];
}

void EngineBase::RequestTextExtraction(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        return;
    }
    int count = ChapterPageCount(loc.chapter);

    {
        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        if (!ct || loc.page > len(ct->text)) {
            return;
        }
        PageText* pt = &ct->text[loc.page - 1];
        TextExtractionState* state = &ct->state[loc.page - 1];
        if (pt->text || *state != TextExtractionState::NotExtracted) {
            return;
        }
        *state = TextExtractionState::Pending;
    }

    AddRef();
    AtomicIntInc(&gDangerousThreadCount);
    auto* data = new TextExtractionThreadData();
    data->engine = this;
    data->pageNo = pageNo;
    auto fn = MkFunc0<TextExtractionThreadData>(ExtractTextThread, data);
    ThreadHandle thread = StartThread(fn, StrL("ExtractPageText"));
    if (thread) {
        SafeCloseThreadHandle(&thread);
        return;
    }

    {
        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        if (ct && loc.page <= len(ct->text) && len(ct->text[loc.page - 1].text) == 0) {
            ct->state[loc.page - 1] = TextExtractionState::NotExtracted;
        }
    }
    AtomicIntDec(&gDangerousThreadCount);
    Release();
    delete data;
}

// default always succeeds; EngineMupdf fails when locks are contended
bool EngineBase::TryExtractPageText(int pageNo, PageText* out) {
    *out = ExtractPageText(pageNo);
    return true;
}

// like GetElements but returns false (and no elements) if the engine can't
// acquire locks without blocking. Default always succeeds; EngineMupdf fails
// when a render thread holds them
bool EngineBase::TryGetElements(int pageNo, Vec<IPageElement*>* out) {
    *out = GetElements(pageNo);
    return true;
}

static Str ReturnCachedPageText(PageText* pt, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    if (lenOut) {
        *lenOut = pt->nCodepoints;
    }
    if (coordsOut) {
        *coordsOut = pt->coords;
    }
    if (quadsOut) {
        *quadsOut = pt->quads;
    }
    Str text = pt->text;
    if (text.s) {
        text.len = pt->len;
        // str::Builder-backed buffers reserve a NUL slot at .len
        if (text.len >= 0) {
            text.s[text.len] = 0;
        }
    }
    return text;
}

// like GetTextForPage but returns false (and empty text) if the engine
// can't acquire locks without blocking (e.g. render thread is busy)
bool EngineBase::TryGetTextForPage(int pageNo, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        if (quadsOut) {
            *quadsOut = nullptr;
        }
        return true;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        if (quadsOut) {
            *quadsOut = nullptr;
        }
        return true;
    }
    int count = ChapterPageCount(loc.chapter);

    bool extract = false;
    {
        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        if (ct->state[loc.page - 1] != TextExtractionState::Finished) {
            extract = true;
        }
    }

    if (extract) {
        PageText extracted;
        if (!TryExtractPageText(pageNo, &extracted)) {
            if (lenOut) {
                *lenOut = 0;
            }
            if (coordsOut) {
                *coordsOut = nullptr;
            }
            if (quadsOut) {
                *quadsOut = nullptr;
            }
            return false;
        }
        EnsurePageText(&extracted);

        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        PageText* pt = &ct->text[loc.page - 1];
        if (ct->state[loc.page - 1] != TextExtractionState::Finished) {
            FreePageText(pt);
            *pt = extracted;
            extracted = PageText();
            ct->state[loc.page - 1] = TextExtractionState::Finished;
        }
        FreePageText(&extracted);
    }

    ScopedMutex scope(&textCacheLock);
    ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
    PageText* pt = &ct->text[loc.page - 1];
    ReturnCachedPageText(pt, lenOut, coordsOut, quadsOut);
    return true;
}

Str EngineBase::GetTextForPage(int pageNo, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        if (quadsOut) {
            *quadsOut = nullptr;
        }
        return {};
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        if (lenOut) {
            *lenOut = 0;
        }
        if (coordsOut) {
            *coordsOut = nullptr;
        }
        if (quadsOut) {
            *quadsOut = nullptr;
        }
        return {};
    }
    int count = ChapterPageCount(loc.chapter);

    bool extract = false;
    {
        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        // Finished covers textless pages too (the page's text can stay empty). Pending
        // means a background thread was started by RequestTextExtraction but
        // selection still needs a synchronous extract here.
        if (ct->state[loc.page - 1] != TextExtractionState::Finished) {
            ct->state[loc.page - 1] = TextExtractionState::Pending;
            extract = true;
        }
    }

    if (extract) {
        PageText extracted = ExtractPageText(pageNo);
        EnsurePageText(&extracted);

        ScopedMutex scope(&textCacheLock);
        ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
        PageText* pt = &ct->text[loc.page - 1];
        if (ct->state[loc.page - 1] != TextExtractionState::Finished) {
            FreePageText(pt);
            *pt = extracted;
            extracted = PageText();
            ct->state[loc.page - 1] = TextExtractionState::Finished;
        }
        FreePageText(&extracted);
    }

    ScopedMutex scope(&textCacheLock);
    ChapterTextCache* ct = pageTextCache->Ensure(loc.chapter, count);
    PageText* pt = &ct->text[loc.page - 1];
    return ReturnCachedPageText(pt, lenOut, coordsOut, quadsOut);
}

void EngineBase::InvalidateTextForPage(int pageNo) {
    if (pageNo < 1 || pageNo > pageCount) {
        return;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        return;
    }
    ScopedMutex scope(&textCacheLock);
    ChapterTextCache* ct = pageTextCache->Peek(loc.chapter);
    if (!ct || loc.page > len(ct->text)) {
        return;
    }
    FreePageText(&ct->text[loc.page - 1]);
    ct->state[loc.page - 1] = TextExtractionState::NotExtracted;
}

// number of pages the loaded document contains. Comes from the chapter table
// (same source as LayoutGeneration()), so a caller that reads count then
// generation never sees them disagree about a concurrent chapter layout.
int EngineBase::PageCount() {
    EnsureChapterTable();
    return chapters.TotalPages();
}

// the box inside PageMediabox that actually contains any relevant content
// (used for auto-cropping in Fit Content mode, can be PageMediabox)
RectF EngineBase::PageContentBox(int pageNo, RenderTarget /*target*/) {
    return PageMediabox(pageNo);
}

const char* PdfPageBoxName(PdfPageBoxKind kind) {
    switch (kind) {
        case PdfPageBoxKind::Media:
            return "media";
        case PdfPageBoxKind::Crop:
            return "crop";
        case PdfPageBoxKind::Bleed:
            return "bleed";
        case PdfPageBoxKind::Trim:
            return "trim";
        case PdfPageBoxKind::Art:
            return "art";
    }
    return "";
}

// Non-PDF engines have no page boxes.
void EngineBase::GetPdfPageBoxes(int /*pageNo*/, Vec<PdfPageBox>& out) {
    VecReset(out);
}

// the layout type this document's author suggests (if the user doesn't care)
// whether the content should be displayed as images instead of as document pages
// (e.g. with a black background and less padding in between and without search UI)
bool EngineBase::IsImageCollection() const {
    return isImageCollection;
}

// TODO: needs a more general interface
// whether it is allowed to print the current document
bool EngineBase::AllowsPrinting() const {
    return allowsPrinting;
}

// whether it is allowed to extract text from the current document
// (except for searching an accessibility reasons)
bool EngineBase::AllowsCopyingText() const {
    return allowsCopyingText;
}

// the DPI for a file is needed when converting internal measures to physical ones
float EngineBase::GetFileDPI() const {
    return fileDPI;
}

// named dest; engine-owned, do not delete
IPageDestination* EngineBase::GetNamedDest(Str /*name*/) {
    return nullptr;
}

// checks whether this document has an associated Table of Contents
bool EngineBase::HasToc() {
    TocTree* tree = GetToc();
    return tree != nullptr;
}

// returns the root element for the loaded document's Table of Contents
// caller must delete the result (when no longer needed)
TocTree* EngineBase::GetToc() {
    return nullptr;
}

// default implementation that just sets wanted keys
// keys are names of properties the caller wants. If given, we append those
// proerties in this order and potentially add more
// if keys are empty, we put them in order we want
void EngineBase::GetProperties(Props& propsOut) {
    for (int i = 0;; i++) {
        DocProp prop = gAllProps[i];
        if (prop == DocProp::None) {
            break;
        }
        // font list is loaded asynchronously in ShowProperties()
        if (prop == DocProp::FontList) {
            continue;
        }
        TempStr val = GetPropertyTemp(prop);
        if (len(val) == 0) continue;
        AddProp(propsOut, prop, val);
    }
}

// checks whether this document has explicit labels for pages (such as
// roman numerals) instead of the default plain arabic numbering
bool EngineBase::HasPageLabels() const {
    return hasPageLabels;
}

int EngineBase::LogicalPageCount() {
    if (logicalPageCount > 0) {
        return logicalPageCount;
    }
    return PageCount();
}

// returns a label to be displayed instead of the page number
// caller must free() the result
TempStr EngineBase::GetPageLabeTemp(int pageNo) const {
    return fmt("%d", pageNo);
}

// reverts GetPageLabel by returning the first page number having the given label
int EngineBase::GetPageByLabel(Str label) const {
    return ParseInt(label);
}

// whether this document required a password in order to be loaded
bool EngineBase::IsPasswordProtected() const {
    return isPasswordProtected;
}

// the name of the file this engine handles
Str EngineBase::FilePath() const {
    return fileNameBase;
}

RenderedBitmap* EngineBase::GetImageForPageElement(IPageElement* /*ipel*/) {
    CrashMe();
    return nullptr;
}

// Encoded file bytes of a page-element image, when the engine still has them
// (a JPEG stream in a PDF, a page of a CBZ, …). Empty if the image only
// exists as decoded pixels. Caller must str::Free.
Str EngineBase::GetImageDataForPageElement(IPageElement*) {
    return {};
}

// protected:
void EngineBase::SetFilePath(Str s) {
    fileNameBase = s ? str::Dup(arena, s) : Str();
}

// applies zoom and rotation to a point in user/page space converting
// it into device/screen space - or in the inverse direction
PointF EngineBase::Transform(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    RectF rc = RectF(pt, SizeF());
    RectF rect = Transform(rc, pageNo, zoom, rotation, inverse);
    return rect.TL();
}

// returns false if didn't perform action (temporary until we move
// all code there)
bool EngineBase::HandleLink(IPageDestination* /*dest*/, ILinkHandler* /*linkHandler*/) {
    // if not implemented in derived classes
    return false;
}

bool SaveFileOrData(Str srcFilePath, Str data, Str dstFilePath) {
    if (srcFilePath) {
        bool ok = file::Copy(dstFilePath, srcFilePath, false);
        if (ok) {
            return true;
        }
    }
    if (len(data) == 0) {
        return false;
    }
    return file::WriteFile(dstFilePath, data);
}
