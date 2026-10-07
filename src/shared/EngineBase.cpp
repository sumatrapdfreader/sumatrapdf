
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

bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://")) ||
           str::StartsWithI(url, StrL("mailto:"));
}

static void EnsurePageText(PageText* pageText) {
    if (pageText->text) {
        if (pageText->nCodepoints == 0) {
            pageText->nCodepoints = Utf8CodepointCount(pageText->text);
        }
        return;
    }
    // TakeStr()/Vec::Take() can allocate backing storage even for empty pages.
    FreePageText(pageText);
}

void FreePageText(PageText* pageText) {
    str::Free(pageText->text);
    free((void*)pageText->coords);
    free((void*)pageText->quads);
    *pageText = {};
}

PageDestination::~PageDestination() {
    str::Free(value);
    str::Free(name);
}

// string value associated with the destination (e.g. a path or a URL)
Str PageDestination::GetValue() {
    return value;
}

// the name of this destination (reverses EngineBase::GetNamedDest) or nullptr
// (mainly applicable for links of type "LaunchFile" to PDF documents)
Str PageDestination::GetName() {
    return name;
}

static void SkipJsWs(const char*& p, const char* end) {
    while (p < end && str::IsWs(*p)) {
        p++;
    }
}

static bool IsJsIdentStart(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$';
}

static bool IsJsIdentChar(char c) {
    return IsJsIdentStart(c) || (c >= '0' && c <= '9');
}

static bool IsJsReservedCallName(Str ident) {
    static constexpr SeqStrings keywords =
        "function\0if\0for\0while\0switch\0catch\0with\0return\0"
        "typeof\0void\0delete\0new\0throw\0else\0do\0try\0";
    return SeqStrIndex(keywords, ident) >= 0;
}

// Decode one JS '...' or "..." string at p. Advances p past the closing quote.
static bool ParseJsQuotedString(const char*& p, const char* end, str::Builder& b) {
    if (p >= end || (*p != '"' && *p != '\'')) {
        return false;
    }
    char quote = *p++;
    while (p < end && *p != quote) {
        char c = *p++;
        if (c != '\\') {
            b.AppendChar(c);
            continue;
        }
        if (p >= end) {
            break;
        }
        char e = *p++;
        static const Str kEscapeChars = StrL("nrtbfv0");
        static const Str kEscapeValues = StrL("\n\r\t\b\f\v\0");
        int escapeIdx = str::IndexOfChar(kEscapeChars, e);
        if (escapeIdx >= 0) {
            b.AppendChar(kEscapeValues.s[escapeIdx]);
            continue;
        }
        if (e != 'x' && e != 'u') {
            b.AppendChar(e);
            continue;
        }
        int digits = e == 'x' ? 2 : 4;
        if (end - p < digits) {
            b.AppendChar(e);
            continue;
        }
        int cp = 0;
        int i = 0;
        for (; i < digits; i++) {
            int h = str::HexDigitVal(p[i]);
            if (h < 0) {
                break;
            }
            cp = (cp << 4) | h;
        }
        if (i != digits) {
            b.AppendChar(e);
            continue;
        }
        p += digits;
        if (e == 'x') {
            b.AppendChar((char)cp);
            continue;
        }
        char utf8[4];
        int off = 0;
        str::Utf8Encode(utf8, off, cp);
        b.Append(Str(utf8, off));
    }
    if (p >= end || *p != quote) {
        return false;
    }
    p++;
    return true;
}

static bool SkipJsNested(const char*& p, const char* end, char open, char close) {
    if (p >= end || *p != open) {
        return false;
    }
    int depth = 1;
    p++;
    while (p < end && depth > 0) {
        if (*p == '"' || *p == '\'') {
            str::Builder ignored;
            if (!ParseJsQuotedString(p, end, ignored)) {
                return false;
            }
            continue;
        }
        if (*p == open) {
            depth++;
        } else if (*p == close) {
            depth--;
        }
        p++;
    }
    return depth == 0;
}

// Collect the quoted arguments of app.popUpMenu(...) / app.popUpMenuEx(...).
bool ParseJsPopUpMenuItems(Str js, StrVec& items) {
    const Str menuCall = StrL("popUpMenu");
    int idx = str::IndexOf(js, menuCall);
    if (idx < 0) {
        return false;
    }
    const char* p = js.s + idx + len(menuCall);
    const char* end = js.s + len(js);
    if (p + 2 <= end && p[0] == 'E' && p[1] == 'x') {
        p += 2;
    }
    SkipJsWs(p, end);
    if (p >= end || *p != '(') {
        return false;
    }
    p++;
    while (p < end) {
        SkipJsWs(p, end);
        if (p >= end || *p == ')') {
            break;
        }
        if (*p == '[') {
            if (!SkipJsNested(p, end, '[', ']')) {
                break;
            }
            continue;
        }
        if (*p == '"' || *p == '\'') {
            str::Builder item;
            if (!ParseJsQuotedString(p, end, item)) {
                break;
            }
            items.Append(ToStr(item));
            continue;
        }
        p++;
    }
    return len(items) > 0;
}

// First identifier that is followed by '(', skipping JS keywords.
Str ExtractJsCallName(Str js) {
    if (len(js) == 0) {
        return {};
    }
    const char* p = js.s;
    const char* end = js.s + len(js);
    while (p < end) {
        SkipJsWs(p, end);
        if (p >= end) {
            break;
        }
        if (!IsJsIdentStart(*p)) {
            p++;
            continue;
        }
        const char* start = p;
        p++;
        while (p < end && IsJsIdentChar(*p)) {
            p++;
        }
        Str ident{start, (int)(p - start)};
        SkipJsWs(p, end);
        if (p < end && *p == '(' && !IsJsReservedCallName(ident)) {
            return ident;
        }
    }
    return {};
}

PageDestinationJsMenu::PageDestinationJsMenu() {
    kind = kindDestinationJsMenu;
    pageNo = -1;
}

PageDestinationJsMenu::~PageDestinationJsMenu() {
    str::Free(tooltip);
}

// Hover text: one menu line per row, skipping "-" separators.
Str PageDestinationJsMenu::GetValue() {
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

void TocItem::AddSiblingAtEnd(TocItem* sibling) {
    TocItem* item = this;
    while (item->next) {
        item = item->next;
    }
    item->next = sibling;
    sibling->parent = item->parent;
}

int TocItem::ChildCount() {
    return ListLen(child);
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
    return child && (isOpenDefault != isOpenToggled);
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

void EnsureFullLayout(EngineBase* engine) {
    if (!engine) {
        return;
    }
    engine->EnsureAllChaptersLaidOut();
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

enum class TextExtractionState {
    NotExtracted,
    Pending,
    Finished,
};

struct TextCacheEntry {
    PageText data;
    TextExtractionState state = TextExtractionState::NotExtracted;

    // Consume text; a concurrent extraction may have filled this slot.
    void StoreText(PageText text) {
        if (state == TextExtractionState::Finished) {
            FreePageText(&text);
            return;
        }
        FreePageText(&data);
        data = text;
        state = TextExtractionState::Finished;
    }
};

// Cache each chapter separately so later layout cannot shift cached pages.
struct ChapterTextCache {
    Vec<TextCacheEntry> pages;

    ~ChapterTextCache() {
        for (TextCacheEntry& page : pages) {
            FreePageText(&page.data);
        }
    }
};

struct PageTextCache {
    Vec<ChapterTextCache*> chapters; // index = chapter - 1; entries lazily created

    ~PageTextCache() { DeleteVecMembers(chapters); }

    // Existing page entry, without creating or growing its chapter cache.
    TextCacheEntry* Peek(Location loc) {
        if (!loc.IsValid() || loc.chapter > len(chapters)) {
            return nullptr;
        }
        ChapterTextCache* ct = chapters[loc.chapter - 1];
        return ct && loc.page <= len(ct->pages) ? &ct->pages[loc.page - 1] : nullptr;
    }

    // Create or grow the chapter cache, then return the requested page entry.
    TextCacheEntry* Ensure(Location loc, int count) {
        if (!loc.IsValid()) {
            return nullptr;
        }
        if (loc.chapter > len(chapters)) {
            VecResize(chapters, loc.chapter);
        }
        ChapterTextCache*& ct = chapters[loc.chapter - 1];
        if (!ct) {
            ct = new ChapterTextCache();
        }
        count = count < 1 ? 1 : count;
        if (len(ct->pages) < count) {
            VecResize(ct->pages, count);
        }
        return Peek(loc);
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

// Initialize plain engines lazily and keep their page counts in sync.
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

// background chapter layout must not resync the view after every chapter: a
// long book would relayout hundreds of times. the thread bumps this and the
// one SetPageCountFromChapters() after the loop notifies
static thread_local int gChapterLayoutQuiet = 0;

struct ChapterLayoutQuiet {
    ChapterLayoutQuiet() { gChapterLayoutQuiet++; }
    ~ChapterLayoutQuiet() { gChapterLayoutQuiet--; }
};

// keeps the flat pageCount total in sync with the chapter table and notifies
// onLayoutChanged (if set) when the generation actually moved, so a
// DisplayModel resyncs even when the layout happened on a render thread
void EngineBase::SetPageCountFromChapters() {
    pageCount = chapters.TotalPages();
    if (gChapterLayoutQuiet > 0) {
        return;
    }
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

Location EngineBase::LastLocation() {
    int c = ChapterCount();
    int n = ChapterPageCount(c);
    return {c, n};
}

Location EngineBase::ClampLocation(Location loc) {
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
    int n = ChapterCount();
    for (int c = 1; c <= n; c++) {
        ChapterPageCount(c);
    }
}

int EngineBase::ChaptersLaidOut() {
    int n = ChapterCount();
    int laid = 0;
    for (int c = 1; c <= n; c++) {
        if (IsChapterLaidOut(c)) {
            laid++;
        }
    }
    return laid;
}

struct ChapterLayoutJob {
    EngineBase* engine = nullptr;
    int job = 0;
};

bool EngineBase::LayoutJobCurrent(int id) {
    return AtomicIntGet(&layoutJob) == id;
}

void EngineBase::FlushPageCount() {
    SetPageCountFromChapters();
}

void EngineBase::ReportLayoutProgress(int done, int total, bool finished) {
    if (!onChapterLayoutProgress.IsValid()) {
        return;
    }
    ChapterLayoutProgress prog;
    prog.done = done;
    prog.total = total;
    prog.finished = finished;
    onChapterLayoutProgress.Call(&prog);
}

// one chapter at a time, then a single page-count notification. stops when a
// newer job starts (the document closed, or a restyle reset the chapters)
static void ChapterLayoutThread(ChapterLayoutJob* job) {
    EngineBase* engine = job->engine;
    int id = job->job;
    delete job;

    int total = engine->ChapterCount();
    int done = 0;
    bool cancelled = false;
    {
        ChapterLayoutQuiet quiet;
        for (int c = 1; c <= total; c++) {
            if (!engine->LayoutJobCurrent(id)) {
                cancelled = true;
                break;
            }
            if (!engine->IsChapterLaidOut(c)) {
                // count only. publishing here shifts flat page numbers under
                // whatever the UI thread is doing with them (GoToPage, render)
                engine->WarmChapter(c);
            }
            if (!engine->LayoutJobCurrent(id)) {
                cancelled = true;
                break;
            }
            done++;
            engine->ReportLayoutProgress(done, total, false);
        }
    }
    if (!cancelled && engine->LayoutJobCurrent(id)) {
        // the UI thread publishes the counts (LayOutChapter is cheap once
        // WarmChapter has paginated) and then resyncs the page total
        engine->ReportLayoutProgress(done, total, true);
    }
    engine->Release();
}

// the open path lays out the chapter being read first; this counts the rest
// so the flat page total can update without blocking open
void EngineBase::StartBackgroundChapterLayout() {
    if (!HasChapters()) {
        return;
    }
    int total = ChapterCount();
    if (ChaptersLaidOut() >= total) {
        return;
    }
    int id = AtomicIntInc(&layoutJob);
    AddRef();
    auto* job = new ChapterLayoutJob();
    job->engine = this;
    job->job = id;
    RunAsync(MkFunc0(ChapterLayoutThread, job), StrL("ChapterLayout"));
}

void EngineBase::CancelBackgroundChapterLayout() {
    AtomicIntInc(&layoutJob);
}

// default: do the full layout. MuPDF overrides this with a count that does
// not publish, so a background thread can't shift flat page numbers
void EngineBase::WarmChapter(int chapter) {
    LayOutChapter(chapter);
}

void EngineBase::PublishWarmedChapters() {
    {
        ChapterLayoutQuiet quiet;
        EnsureAllChaptersLaidOut();
    }
    FlushPageCount();
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

Func1<EngineBase*> gOnEngineDestroyed;

EngineBase::~EngineBase() {
    gOnEngineDestroyed.Call(this);
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
    TextCacheEntry* page = pageTextCache->Peek(loc);
    return page && (bool)page->data.text;
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
        TextCacheEntry* page = pageTextCache->Ensure(loc, count);
        if (!page || page->data.text || page->state != TextExtractionState::NotExtracted) {
            return;
        }
        page->state = TextExtractionState::Pending;
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
        TextCacheEntry* page = pageTextCache->Ensure(loc, count);
        if (page && len(page->data.text) == 0) {
            page->state = TextExtractionState::NotExtracted;
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

static Str ReturnPageText(const PageText& pt, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    if (lenOut) {
        *lenOut = pt.nCodepoints;
    }
    if (coordsOut) {
        *coordsOut = pt.coords;
    }
    if (quadsOut) {
        *quadsOut = pt.quads;
    }
    Str text = pt.text;
    if (text.s) {
        // str::Builder-backed buffers reserve a NUL slot at .len
        if (text.len >= 0) {
            text.s[text.len] = 0;
        }
    }
    return text;
}

bool EngineBase::ReadPageText(int pageNo, TextReadMode mode, Str& text, int* lenOut, Rect** coordsOut,
                              QuadF** quadsOut) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        text = ReturnPageText({}, lenOut, coordsOut, quadsOut);
        return true;
    }
    Location loc = LocationFromPageNo(pageNo);
    if (!loc.IsValid()) {
        text = ReturnPageText({}, lenOut, coordsOut, quadsOut);
        return true;
    }
    int count = ChapterPageCount(loc.chapter);

    bool extract;
    {
        ScopedMutex scope(&textCacheLock);
        TextCacheEntry* page = pageTextCache->Ensure(loc, count);
        // Finished includes textless pages. Pending still allows synchronous extraction.
        extract = page->state != TextExtractionState::Finished;
        if (extract && mode == TextReadMode::Blocking) {
            page->state = TextExtractionState::Pending;
        }
    }

    if (extract) {
        PageText extracted;
        if (mode == TextReadMode::Blocking) {
            extracted = ExtractPageText(pageNo);
        } else if (!TryExtractPageText(pageNo, &extracted)) {
            text = ReturnPageText({}, lenOut, coordsOut, quadsOut);
            return false;
        }
        EnsurePageText(&extracted);

        ScopedMutex scope(&textCacheLock);
        pageTextCache->Ensure(loc, count)->StoreText(extracted);
    }

    ScopedMutex scope(&textCacheLock);
    TextCacheEntry* page = pageTextCache->Ensure(loc, count);
    text = ReturnPageText(page->data, lenOut, coordsOut, quadsOut);
    return true;
}

// Returns false with empty outputs when extraction would block.
bool EngineBase::TryGetTextForPage(int pageNo, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    Str text;
    return ReadPageText(pageNo, TextReadMode::Nonblocking, text, lenOut, coordsOut, quadsOut);
}

Str EngineBase::GetTextForPage(int pageNo, int* lenOut, Rect** coordsOut, QuadF** quadsOut) {
    Str text;
    ReadPageText(pageNo, TextReadMode::Blocking, text, lenOut, coordsOut, quadsOut);
    return text;
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
    TextCacheEntry* page = pageTextCache->Peek(loc);
    if (!page) {
        return;
    }
    FreePageText(&page->data);
    page->state = TextExtractionState::NotExtracted;
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
    // Names follow PdfPageBoxKind order.
    static constexpr const char* names[] = {"media", "crop", "bleed", "trim", "art"};
    int idx = (int)kind;
    return idx < dimofi(names) ? names[idx] : "";
}

// Non-PDF engines have no page boxes.
void EngineBase::GetPdfPageBoxes(int /*pageNo*/, Vec<PdfPageBox>& out) {
    VecReset(out);
}

// TODO: needs a more general interface
// whether it is allowed to print the current document
bool EngineBase::AllowsPrinting() const {
    return allowsPrinting;
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

// Append nonempty properties in standard order, preserving existing values.
void EngineBase::GetProperties(Props& propsOut) {
    for (int i = 0; gAllProps[i] != DocProp::None; i++) {
        DocProp prop = gAllProps[i];
        // font list is loaded asynchronously in ShowProperties()
        if (prop == DocProp::FontList) {
            continue;
        }
        TempStr val = GetPropertyTemp(prop);
        if (len(val) == 0) continue;
        AddProp(propsOut, prop, val);
    }
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
    if (srcFilePath && file::Copy(dstFilePath, srcFilePath, false)) {
        return true;
    }
    return len(data) != 0 && file::WriteFile(dstFilePath, data);
}
