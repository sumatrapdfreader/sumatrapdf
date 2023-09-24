/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// engines which render flowed ebook formats into fixed pages through the EngineBase API
// (pages are mostly layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "mui/Mui.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "FzImgReader.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "EbookDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

Kind kindEngineEpub = "engineEpub";
Kind kindEngineFb2 = "engineFb2";
Kind kindEngineMobi = "engineMobi";
Kind kindEnginePdb = "enginePdb";
Kind kindEngineChm = "engineChm";
Kind kindEngineHtml = "engineHtml";
Kind kindEngineTxt = "engineTxt";

static AutoFreeStr gDefaultFontName;
static float gDefaultFontSize = 10.f;

static const WCHAR* GetDefaultFontName() {
    char* s = gDefaultFontName.Get();
    if (s) {
        return ToWstrTemp(s);
    }
    return L"Georgia";
}

static float GetDefaultFontSize() {
    // fonts are scaled at higher DPI settings,
    // undo this here for (mostly) consistent results
    if (gDefaultFontSize == 0) {
        gDefaultFontSize = 10;
    }
    return gDefaultFontSize * 96.0f / (float)DpiGetForHwnd(HWND_DESKTOP);
}

void SetDefaultEbookFont(const char* name, float size) {
    // intentionally don't validate the input
    if (str::Eq(name, "default")) {
        // "default" is used for mupdf engine to indicate
        // we should use the font as given in css
        name = "Georgia";
    }
    gDefaultFontName.SetCopy(name);
    // use a somewhat smaller size than in the EbookUI, since fit page/width
    // is likely to be above 100% for the paperback page dimensions
    gDefaultFontSize = size * 0.8f;
}

/* common classes for EPUB, FictionBook2, Mobi, PalmDOC, CHM, HTML and TXT engines */

struct PageAnchor {
    DrawInstr* instr;
    int pageNo;

    explicit PageAnchor(DrawInstr* instr = nullptr, int pageNo = -1) : instr(instr), pageNo(pageNo) {
    }
};

class EbookAbortCookie : public AbortCookie {
  public:
    bool abort = false;
    EbookAbortCookie() {
    }
    void Abort() override {
        abort = true;
    }
};

class EngineEbook : public EngineBase {
  public:
    EngineEbook();
    ~EngineEbook() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;

    bool SaveFileAs(const char* copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    // make RenderCache request larger tiles than per default
    bool HasClipOptimizations(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) override {
        ReportIf(!dest || !linkHandler);
        if (!dest || !linkHandler) {
            return false;
        }
        linkHandler->GotoLink(dest);
        return true;
    }

    IPageDestination* GetNamedDest(const char* name) override;
    RenderedBitmap* GetImageForPageElement(IPageElement* el) override;

    bool BenchLoadPage(int pageNo) override;

  protected:
    Vec<HtmlPage*>* pages = nullptr;
    Vec<PageAnchor> anchors;
    // contains for each page the last anchor indicating
    // a break between two merged documents
    Vec<DrawInstr*> baseAnchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // TODO: still needed?
    CRITICAL_SECTION pagesAccess;
    // page dimensions can vary between filetypes
    RectF pageRect;
    float pageBorder;

    void GetTransform(Matrix& m, float zoom, int rotation);
    bool ExtractPageAnchors();
    char* ExtractFontList();

    virtual IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo);

    Vec<DrawInstr>* GetHtmlPage(int pageNo);
    HtmlPage* GetHtmlPage2(int pageNo);
};

static IPageElement* NewEbookLink(DrawInstr* link, Rect rect, IPageDestination* dest, int pageNo = 0,
                                  bool showUrl = false) {
    if (!dest) {
        // TODO: this doesn't make sense
        dest = new PageDestination();
        dest->kind = kindDestinationLaunchURL;
        // TODO: not sure about this
        // dest->value = str::Dup(res->value);
        dest->rect = ToRectF(rect);
    }

    auto res = new PageElementDestination(dest);
    res->pageNo = pageNo;
    res->rect = ToRectF(rect);

#if 0 // TODO: figure out
    if (showUrl) {
        res->value = strconv::FromHtmlUtf8(link->str.s, link->str.len);
    }
#endif
    return res;
}

static IPageElement* NewImageDataElement(int pageNo, Rect bbox, int imageID) {
    auto res = new PageElementImage();
    res->pageNo = pageNo;
    res->rect = ToRectF(bbox);
    res->imageID = imageID;
    return res;
}

static TocItem* newEbookTocItem(TocItem* parent, const char* title, IPageDestination* dest) {
    auto res = new TocItem(parent, title, 0);
    res->dest = dest;
    if (dest) {
        res->pageNo = dest->GetPageNo();
    }
    return res;
}

EngineEbook::EngineEbook() {
    pageCount = 0;
    // "B Format" paperback
    pageRect = RectF(0, 0, 5.12f * GetFileDPI(), 7.8f * GetFileDPI());
    pageBorder = 0.4f * GetFileDPI();
    preferredLayout = preferredLayout = PageLayout(PageLayout::Type::Single);
    InitializeCriticalSection(&pagesAccess);
}

EngineEbook::~EngineEbook() {
    EnterCriticalSection(&pagesAccess);

    if (pages) {
        for (HtmlPage* page : *pages) {
            DeleteVecMembers(page->elements);
        }
        DeleteVecMembers(*pages);
    }
    delete pages;

    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

RectF EngineEbook::PageMediabox(int) {
    return pageRect;
}

RectF EngineEbook::PageContentBox(int pageNo, RenderTarget) {
    RectF mbox = PageMediabox(pageNo);
    mbox.Inflate(-pageBorder, -pageBorder);
    return mbox;
}

ByteSlice EngineEbook::GetFileData() {
    const char* fileName = FilePath();
    if (!fileName) {
        return {};
    }
    return file::ReadFile(fileName);
}

bool EngineEbook::SaveFileAs(const char* dstPath) {
    const char* srcPath = FilePath();
    if (!srcPath) {
        return false;
    }
    auto res = file::Copy(dstPath, srcPath, false);
    return res != 0;
}

// make RenderCache request larger tiles than per default
bool EngineEbook::HasClipOptimizations(int) {
    return false;
}

bool EngineEbook::BenchLoadPage(int) {
    return true;
}

void EngineEbook::GetTransform(Matrix& m, float zoom, int rotation) {
    GetBaseTransform(m, ToGdipRectF(pageRect), zoom, rotation);
}

Vec<DrawInstr>* EngineEbook::GetHtmlPage(int pageNo) {
    CrashIf(pageNo < 1 || PageCount() < pageNo);
    if (pageNo < 1 || PageCount() < pageNo) {
        return nullptr;
    }
    return &pages->at(pageNo - 1)->instructions;
}

HtmlPage* EngineEbook::GetHtmlPage2(int pageNo) {
    CrashIf(pageNo < 1 || PageCount() < pageNo);
    if (pageNo < 1 || PageCount() < pageNo) {
        return nullptr;
    }
    return pages->at(pageNo - 1);
}

bool EngineEbook::ExtractPageAnchors() {
    ScopedCritSec scope(&pagesAccess);

    DrawInstr* baseAnchor = nullptr;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
        if (!pageInstrs) {
            return false;
        }

        for (size_t k = 0; k < pageInstrs->size(); k++) {
            DrawInstr* i = &pageInstrs->at(k);
            if (DrawInstrType::Anchor != i->type) {
                continue;
            }
            anchors.Append(PageAnchor(i, pageNo));
            if (k < 2 && str::StartsWith(i->str.s + i->str.len, "\" page_marker />")) {
                baseAnchor = i;
            }
        }
        baseAnchors.Append(baseAnchor);
    }

    CrashIf(baseAnchors.size() != pages->size());
    return true;
}

RectF EngineEbook::Transform(const RectF& rect, int, float zoom, int rotation, bool inverse) {
    RectF rcF = rect; // TODO: un-needed conversion
    auto p1 = Gdiplus::PointF(rcF.x, rcF.y);
    auto p2 = Gdiplus::PointF(rcF.x + rcF.dx, rcF.y + rcF.dy);
    Gdiplus::PointF pts[2] = {p1, p2};
    Matrix m;
    GetTransform(m, zoom, rotation);
    if (inverse) {
        m.Invert();
    }
    m.TransformPoints(pts, 2);
    return RectF::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
}

RenderedBitmap* EngineEbook::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;
    auto zoom = args.zoom;
    auto rotation = args.rotation;

    RectF pageRc = args.pageRect ? *args.pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Point screenTL = screen.TL();
    screen.Offset(-screen.x, -screen.y);

    HANDLE hMap = nullptr;
    HBITMAP hbmp = CreateMemoryBitmap(screen.Size(), &hMap);
    HDC hDC = CreateCompatibleDC(nullptr);
    DeleteObject(SelectObject(hDC, hbmp));

    Graphics g(hDC);
    mui::InitGraphicsMode(&g);

    Color white(0xFF, 0xFF, 0xFF);
    SolidBrush tmpBrush(white);
    Gdiplus::Rect screenR(ToGdipRect(screen));
    screenR.Inflate(1, 1);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, zoom, rotation);
    m.Translate((float)-screenTL.x, (float)-screenTL.y, MatrixOrderAppend);
    g.SetTransform(&m);

    EbookAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new EbookAbortCookie();
        *args.cookie_out = cookie;
    }

    ScopedCritSec scope(&pagesAccess);

    mui::ITextRender* textDraw = mui::TextRenderGdiplus::Create(&g);
    DrawHtmlPage(&g, textDraw, GetHtmlPage(pageNo), pageBorder, pageBorder, false, Color((ARGB)Color::Black),
                 cookie ? &cookie->abort : nullptr);
    delete textDraw;
    DeleteDC(hDC);

    if (cookie && cookie->abort) {
        DeleteObject(hbmp);
        CloseHandle(hMap);
        return nullptr;
    }

    return new RenderedBitmap(hbmp, screen.Size(), hMap);
}

static Rect GetInstrBbox(DrawInstr& instr, float pageBorder) {
    RectF bbox(instr.bbox.x, instr.bbox.y, instr.bbox.dx, instr.bbox.dy);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

PageText EngineEbook::ExtractPageText(int pageNo) {
    const WCHAR* lineSep = L"\n";
    ScopedCritSec scope(&pagesAccess);

    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    str::WStr content;
    Vec<Rect> coords;
    bool insertSpace = false;

    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    for (DrawInstr& i : *pageInstrs) {
        Rect bbox = GetInstrBbox(i, pageBorder);
        switch (i.type) {
            case DrawInstrType::String:
                if (coords.size() > 0 &&
                    (bbox.x < coords.Last().BR().x || bbox.y > coords.Last().y + coords.Last().dy * 0.8)) {
                    content.Append(lineSep);
                    coords.AppendBlanks(str::Len(lineSep));
                    CrashIf(*lineSep && !coords.Last().IsEmpty());
                } else if (insertSpace && coords.size() > 0) {
                    int swidth = bbox.x - coords.Last().BR().x;
                    if (swidth > 0) {
                        content.AppendChar(' ');
                        coords.Append(Rect(bbox.x - swidth, bbox.y, swidth, bbox.dy));
                    }
                }
                insertSpace = false;
                {
                    AutoFreeWstr s(strconv::FromHtmlUtf8(i.str.s, i.str.len));
                    content.Append(s);
                    size_t len = str::Len(s);
                    double cwidth = 1.0 * bbox.dx / len;
                    for (size_t k = 0; k < len; k++) {
                        coords.Append(Rect((int)(bbox.x + k * cwidth), bbox.y, (int)cwidth, bbox.dy));
                    }
                }
                break;
            case DrawInstrType::RtlString:
                if (coords.size() > 0 &&
                    (bbox.BR().x > coords.Last().x || bbox.y > coords.Last().y + coords.Last().dy * 0.8)) {
                    content.Append(lineSep);
                    coords.AppendBlanks(str::Len(lineSep));
                    CrashIf(*lineSep && !coords.Last().IsEmpty());
                } else if (insertSpace && coords.size() > 0) {
                    int swidth = coords.Last().x - bbox.BR().x;
                    if (swidth > 0) {
                        content.AppendChar(' ');
                        coords.Append(Rect(bbox.BR().x, bbox.y, swidth, bbox.dy));
                    }
                }
                insertSpace = false;
                {
                    AutoFreeWstr s(strconv::FromHtmlUtf8(i.str.s, i.str.len));
                    content.Append(s);
                    size_t len = str::Len(s);
                    double cwidth = 1.0 * bbox.dx / len;
                    for (size_t k = 0; k < len; k++) {
                        coords.Append(Rect((int)(bbox.x + (len - k - 1) * cwidth), bbox.y, (int)cwidth, bbox.dy));
                    }
                }
                break;
            case DrawInstrType::ElasticSpace:
            case DrawInstrType::FixedSpace:
                insertSpace = true;
                break;
        }
    }
    if (content.size() > 0 && !str::EndsWith(content.Get(), lineSep)) {
        content.Append(lineSep);
        coords.AppendBlanks(str::Len(lineSep));
    }
    CrashIf(coords.size() != content.size());

    PageText res;
    res.len = (int)content.size();
    res.text = content.StealData();
    res.coords = coords.StealData();
    return res;
}

IPageElement* EngineEbook::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    char* url = strconv::FromHtmlUtf8Temp(link->str.s, link->str.len);
    if (url::IsAbsolute(url)) {
        return NewEbookLink(link, rect, nullptr, pageNo);
    }

    DrawInstr* baseAnchor = baseAnchors.at(pageNo - 1);
    if (baseAnchor) {
        char* basePath = str::DupTemp(baseAnchor->str.s, baseAnchor->str.len);
        AutoFreeStr relPath = ResolveHtmlEntities(link->str.s, link->str.len);
        AutoFreeStr absPath = NormalizeURL(relPath, basePath);
        url = str::DupTemp(absPath.Get());
    }

    IPageDestination* dest = GetNamedDest(url);
    if (!dest) {
        return nullptr;
    }
    return NewEbookLink(link, rect, dest, pageNo);
}

Vec<IPageElement*> EngineEbook::GetElements(int pageNo) {
    HtmlPage* pi = GetHtmlPage2(pageNo);
    if (pi->gotElements) {
        return pi->elements;
    }
    pi->gotElements = true;
    Vec<IPageElement*>& els = pi->elements;

    Vec<DrawInstr>* pageInstrs = &pi->instructions;
    size_t n = pageInstrs->size();
    for (size_t idx = 0; idx < n; idx++) {
        DrawInstr& i = pageInstrs->at(idx);
        if (DrawInstrType::Image == i.type) {
            auto box = GetInstrBbox(i, pageBorder);
            auto el = NewImageDataElement(pageNo, box, (int)idx);
            els.Append(el);
        } else if (DrawInstrType::LinkStart == i.type && !i.bbox.IsEmpty()) {
            IPageElement* link = CreatePageLink(&i, GetInstrBbox(i, pageBorder), pageNo);
            if (link) {
                els.Append(link);
            }
        }
    }

    return els;
}

static RenderedBitmap* getImageFromData(const ByteSlice& imageData) {
    HBITMAP hbmp = nullptr;
    Bitmap* bmp = BitmapFromData(imageData);
    if (!bmp || bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok) {
        delete bmp;
        return nullptr;
    }
    Size size(bmp->GetWidth(), bmp->GetHeight());
    delete bmp;
    return new RenderedBitmap(hbmp, size);
}

RenderedBitmap* EngineEbook::GetImageForPageElement(IPageElement* iel) {
    CrashIf(iel->GetKind() != kindPageElementImage);
    PageElementImage* el = (PageElementImage*)iel;
    int pageNo = el->pageNo;
    int idx = el->imageID;
    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    auto&& i = pageInstrs->at(idx);
    CrashIf(i.type != DrawInstrType::Image);
    return getImageFromData(i.GetImage());
}

// don't delete the result
IPageElement* EngineEbook::GetElementAtPos(int pageNo, PointF pt) {
    auto els = GetElements(pageNo);

    for (auto& el : els) {
        if (el->GetRect().Contains(pt)) {
            return el;
        }
    }
    return nullptr;
}

IPageDestination* EngineEbook::GetNamedDest(const char* name) {
    const char* id = name;
    if (str::FindChar(id, '#')) {
        id = str::FindChar(id, '#') + 1;
    }

    // if the name consists of both path and ID,
    // try to first skip to the page with the desired
    // path before looking for the ID to allow
    // for the same ID to be reused on different pages
    DrawInstr* baseAnchor = nullptr;
    int basePageNo = 0;
    if (id > name + 1) {
        size_t base_len = id - name - 1;
        for (size_t i = 0; i < baseAnchors.size(); i++) {
            DrawInstr* anchor = baseAnchors.at(i);
            if (anchor && base_len == anchor->str.len && str::EqNI(name, anchor->str.s, base_len)) {
                baseAnchor = anchor;
                basePageNo = (int)i + 1;
                break;
            }
        }
    }

    size_t id_len = str::Len(id);
    for (size_t i = 0; i < anchors.size(); i++) {
        PageAnchor* anchor = &anchors.at(i);
        if (baseAnchor) {
            if (anchor->instr == baseAnchor) {
                baseAnchor = nullptr;
            }
            continue;
        }
        // note: at least CHM treats URLs as case-independent
        if (id_len == anchor->instr->str.len && str::EqNI(id, anchor->instr->str.s, id_len)) {
            RectF rect(0, anchor->instr->bbox.y + pageBorder, pageRect.dx, 10);
            rect.Inflate(-pageBorder, 0);
            return NewSimpleDest(anchor->pageNo, rect);
        }
    }

    // don't fail if an ID doesn't exist in a merged document
    if (basePageNo != 0) {
        RectF rect(0, pageBorder, pageRect.dx, 10);
        rect.Inflate(-pageBorder, 0);
        return NewSimpleDest(basePageNo, rect);
    }

    return nullptr;
}

char* EngineEbook::ExtractFontList() {
    ScopedCritSec scope(&pagesAccess);

    Vec<mui::CachedFont*> seenFonts;
    StrVec fonts;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
        if (!pageInstrs) {
            continue;
        }

        for (DrawInstr& i : *pageInstrs) {
            if (DrawInstrType::SetFont != i.type || seenFonts.Contains(i.font)) {
                continue;
            }
            seenFonts.Append(i.font);

            FontFamily family;
            if (!i.font->font) {
                // TODO: handle gdi
                CrashIf(!i.font->GetHFont());
                continue;
            }
            Status ok = i.font->font->GetFamily(&family);
            if (ok != Ok) {
                continue;
            }
            WCHAR fontNameW[LF_FACESIZE];
            ok = family.GetFamilyName(fontNameW);
            if (ok != Ok) {
                continue;
            }
            char* fontName = ToUtf8Temp(fontNameW);
            fonts.AppendIfNotExists(fontName);
        }
    }
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    char* res = Join(fonts, "\n");
    return res;
}

static void AppendTocItem(TocItem*& root, TocItem* item, int level) {
    if (!root) {
        root = item;
        return;
    }
    // find the last child at each level, until finding the parent of the new item
    TocItem* r2 = root;
    while (--level > 0) {
        for (; r2->next; r2 = r2->next) {
            ;
        }
        if (r2->child) {
            r2 = r2->child;
        } else {
            r2->child = item;
            return;
        }
    }
    r2->AddSiblingAtEnd(item);
}

class EbookTocBuilder : public EbookTocVisitor {
    EngineBase* engine = nullptr;
    TocItem* root = nullptr;
    int idCounter = 0;
    bool isIndex = false;

  public:
    explicit EbookTocBuilder(EngineBase* engine) {
        this->engine = engine;
    }

    void Visit(const char* name, const char* url, int level) override;

    TocItem* GetRoot() {
        return root;
    }
    void SetIsIndex(bool value) {
        isIndex = value;
    }
};

void EbookTocBuilder::Visit(const char* name, const char* url, int level) {
    IPageDestination* dest;
    if (!url) {
        dest = nullptr;
    } else if (url::IsAbsolute(url)) {
        dest = NewSimpleDest(0, RectF(), 0.f, url);
    } else {
        dest = engine->GetNamedDest(url);
        if (!dest && str::FindChar(url, '%')) {
            char* decodedUrl = str::DupTemp(url);
            url::DecodeInPlace(decodedUrl);
            dest = engine->GetNamedDest(decodedUrl);
        }
    }

    // TODO: send parent to newEbookTocItem
    TocItem* item = newEbookTocItem(nullptr, name, dest);
    item->id = ++idCounter;
    if (isIndex) {
        item->pageNo = 0;
        level++;
    }
    AppendTocItem(root, item, level);
}

/* EngineBase for handling EPUB documents */

class EngineEpub : public EngineEbook {
  public:
    EngineEpub();
    ~EngineEpub() override;
    EngineBase* Clone() override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    EpubDoc* doc = nullptr;
    IStream* stream = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

EngineEpub::EngineEpub() : EngineEbook() {
    kind = kindEngineEpub;
    str::ReplaceWithCopy(&defaultExt, ".epub");
}

EngineEpub::~EngineEpub() {
    delete doc;
    delete tocTree;
    if (stream) {
        stream->Release();
    }
}

EngineBase* EngineEpub::Clone() {
    if (stream) {
        return CreateFromStream(stream);
    }
    const char* path = FilePath();
    if (path) {
        return CreateFromFile(path);
    }
    return nullptr;
}

bool EngineEpub::Load(const char* fileName) {
    SetFilePath(fileName);
    if (dir::Exists(fileName)) {
        // load uncompressed documents as a recompressed ZIP stream
        ScopedComPtr<IStream> zipStream(OpenDirAsZipStream(fileName, true));
        if (!zipStream) {
            return false;
        }
        return Load(zipStream);
    }
    doc = EpubDoc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineEpub::Load(IStream* stream) {
    stream->AddRef();
    this->stream = stream;
    doc = EpubDoc::CreateFromStream(stream);
    return FinishLoading();
}

bool EngineEpub::FinishLoading() {
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args{};
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::GdiplusQuick;

    pages = EpubFormatter(&args, doc).FormatAllPages(false);

    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    preferredLayout = PageLayout(PageLayout::Type::Book);
    if (doc->IsRTL()) {
        preferredLayout.r2l = true;
    }

    return pageCount > 0;
}

ByteSlice EngineEpub::GetFileData() {
    const char* path = FilePath();
    return GetStreamOrFileData(stream, path);
}

bool EngineEpub::SaveFileAs(const char* dstPath) {
    if (stream) {
        ByteSlice d = GetDataFromStream(stream, nullptr);
        bool ok = !d.empty() && file::WriteFile(dstPath, d);
        d.Free();
        if (ok) {
            return true;
        }
    }
    const char* srcPath = FilePath();
    if (!srcPath) {
        return false;
    }
    return file::Copy(dstPath, srcPath, false);
}

TocTree* EngineEpub::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

EngineBase* EngineEpub::CreateFromFile(const char* fileName) {
    EngineEpub* engine = new EngineEpub();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineEpub::CreateFromStream(IStream* stream) {
    EngineEpub* engine = new EngineEpub();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineEpubFromFile(const char* fileName) {
    return EngineEpub::CreateFromFile(fileName);
}

EngineBase* CreateEngineEpubFromStream(IStream* stream) {
    return EngineEpub::CreateFromStream(stream);
}

/* EngineBase for handling FictionBook2 documents */

class EngineFb2 : public EngineEbook {
  public:
    EngineFb2() : EngineEbook() {
        kind = kindEngineFb2;
        str::ReplaceWithCopy(&defaultExt, ".fb2");
    }
    ~EngineFb2() override {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    Fb2Doc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

bool EngineFb2::Load(const char* fileName) {
    SetFilePath(fileName);
    doc = Fb2Doc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineFb2::Load(IStream* stream) {
    doc = Fb2Doc::CreateFromStream(stream);
    return FinishLoading();
}

bool EngineFb2::FinishLoading() {
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetXmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::GdiplusQuick;

    if (doc->IsZipped()) {
        str::ReplaceWithCopy(&defaultExt, ".fb2z");
    }

    pages = Fb2Formatter(&args, doc).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }
    return pageCount > 0;
}

TocTree* EngineFb2::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

EngineBase* EngineFb2::CreateFromFile(const char* fileName) {
    EngineFb2* engine = new EngineFb2();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineFb2::CreateFromStream(IStream* stream) {
    EngineFb2* engine = new EngineFb2();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineFb2FromFile(const char* fileName) {
    return EngineFb2::CreateFromFile(fileName);
}

EngineBase* CreateEngineFb2FromStream(IStream* stream) {
    return EngineFb2::CreateFromStream(stream);
}

/* EngineBase for handling Mobi documents */

#include "MobiDoc.h"

class EngineMobi : public EngineEbook {
  public:
    EngineMobi() : EngineEbook() {
        kind = kindEngineMobi;
        str::ReplaceWithCopy(&defaultExt, ".mobi");
    }
    ~EngineMobi() override {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    MobiDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

bool EngineMobi::Load(const char* fileName) {
    SetFilePath(fileName);
    doc = MobiDoc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineMobi::Load(IStream* stream) {
    doc = MobiDoc::CreateFromStream(stream);
    return FinishLoading();
}

bool EngineMobi::FinishLoading() {
    if (!doc || PdbDocType::Mobipocket != doc->GetDocType()) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::GdiplusQuick;

    pages = MobiFormatter(&args, doc).FormatAllPages();
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }
    return pageCount > 0;
}

IPageDestination* EngineMobi::GetNamedDest(const char* name) {
    int filePos = atoi(name);
    if (filePos < 0 || 0 == filePos && *name != '0') {
        return nullptr;
    }
    int pageNo;
    for (pageNo = 1; pageNo < PageCount(); pageNo++) {
        if (pages->at(pageNo)->reparseIdx > filePos) {
            break;
        }
    }
    CrashIf(pageNo < 1 || pageNo > PageCount());

    ByteSlice htmlData = doc->GetHtmlData();
    size_t htmlLen = htmlData.size();
    const char* start = (const char*)htmlData.data();
    if ((size_t)filePos > htmlLen) {
        return nullptr;
    }

    ScopedCritSec scope(&pagesAccess);
    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    // link to the bottom of the page, if filePos points
    // beyond the last visible DrawInstr of a page
    float currY = (float)pageRect.dy;
    for (DrawInstr& i : *pageInstrs) {
        if ((DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) && i.str.s >= start &&
            i.str.s <= start + htmlLen && i.str.s - start >= filePos) {
            currY = i.bbox.y;
            break;
        }
    }
    RectF rect(0, currY + pageBorder, pageRect.dx, 10);
    rect.Inflate(-pageBorder, 0);
    return NewSimpleDest(pageNo, rect);
}

TocTree* EngineMobi::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

EngineBase* EngineMobi::CreateFromFile(const char* fileName) {
    EngineMobi* engine = new EngineMobi();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineMobi::CreateFromStream(IStream* stream) {
    EngineMobi* engine = new EngineMobi();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMobiFromFile(const char* fileName) {
    return EngineMobi::CreateFromFile(fileName);
}

EngineBase* CreateEngineMobiFromStream(IStream* stream) {
    return EngineMobi::CreateFromStream(stream);
}

/* EngineBase for handling PalmDOC documents (and extensions such as TealDoc) */

class EnginePdb : public EngineEbook {
  public:
    EnginePdb() : EngineEbook() {
        kind = kindEnginePdb;
        str::ReplaceWithCopy(&defaultExt, ".pdb");
    }
    ~EnginePdb() override {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);

  protected:
    PalmDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);
};

bool EnginePdb::Load(const char* fileName) {
    SetFilePath(fileName);

    doc = PalmDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::GdiplusQuick;

    pages = HtmlFormatter(&args).FormatAllPages();
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

TocTree* EnginePdb::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    auto* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

EngineBase* EnginePdb::CreateFromFile(const char* fileName) {
    EnginePdb* engine = new EnginePdb();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEnginePdbFromFile(const char* fileName) {
    return EnginePdb::CreateFromFile(fileName);
}

/* formatting extensions for CHM */

#include "ChmFile.h"

class ChmDataCache {
    ChmFile* doc = nullptr; // owned by creator
    ByteSlice html;
    Vec<ImageData> images;

  public:
    ChmDataCache(ChmFile* doc, char* html) : doc(doc), html(html) {
    }

    ~ChmDataCache() {
        for (auto&& img : images) {
            str::Free(img.base);
            str::Free(img.fileName);
        }
        html.Free();
    }

    ByteSlice GetHtmlData() {
        return html;
    }

    ByteSlice* GetImageData(const char* id, const char* pagePath) {
        AutoFreeStr url = NormalizeURL(id, pagePath);
        for (size_t i = 0; i < images.size(); i++) {
            if (str::Eq(images.at(i).fileName, url)) {
                return &images.at(i).base;
            }
        }

        auto tmp = doc->GetData(url);
        if (tmp.empty()) {
            return nullptr;
        }

        ImageData data;
        data.base = tmp;

        data.fileName = url.Release();
        images.Append(data);
        return &images.Last().base;
    }

    ByteSlice GetFileData(const char* relPath, const char* pagePath) {
        AutoFreeStr url = NormalizeURL(relPath, pagePath);
        return doc->GetData(url);
    }
};

class ChmFormatter : public HtmlFormatter {
  protected:
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagPagebreak(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;

    ChmDataCache* chmDoc = nullptr;
    AutoFreeStr pagePath;

  public:
    ChmFormatter(HtmlFormatterArgs* args, ChmDataCache* doc) : HtmlFormatter(args), chmDoc(doc) {
    }
};

void ChmFormatter::HandleTagImg(HtmlToken* t) {
    CrashIf(!chmDoc);
    if (t->IsEndTag()) {
        return;
    }
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName("src");
    if (attr) {
        AutoFreeStr src = str::Dup(attr->val, attr->valLen);
        url::DecodeInPlace(src);
        ByteSlice* img = chmDoc->GetImageData(src, pagePath);
        needAlt = !img || !EmitImage(img);
    }
    if (needAlt && (attr = t->GetAttrByName("alt")) != nullptr) {
        HandleText(attr->val, attr->valLen);
    }
}

void ChmFormatter::HandleTagPagebreak(HtmlToken* t) {
    AttrInfo* attr = t->GetAttrByName("page_path");
    if (!attr || pagePath) {
        ForceNewPage();
    }
    if (attr) {
        Gdiplus::RectF bbox(0, currY, pageDx, 0);
        currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
        pagePath.Set(str::Dup(attr->val, attr->valLen));
        // reset CSS style rules for the new document
        styleRules.Reset();
    }
}

void ChmFormatter::HandleTagLink(HtmlToken* t) {
    CrashIf(!chmDoc);
    if (t->IsEndTag()) {
        return;
    }
    AttrInfo* attr = t->GetAttrByName("rel");
    if (!attr || !attr->ValIs("stylesheet")) {
        return;
    }
    attr = t->GetAttrByName("type");
    if (attr && !attr->ValIs("text/css")) {
        return;
    }
    attr = t->GetAttrByName("href");
    if (!attr) {
        return;
    }

    char* src = str::DupTemp(attr->val, attr->valLen);
    url::DecodeInPlace(src);
    ByteSlice data = chmDoc->GetFileData(src, pagePath);
    if (data.Get()) {
        ParseStyleSheet(data, data.size());
    }
    data.Free();
}

/* EngineBase for handling CHM documents */

class EngineChm : public EngineEbook {
  public:
    EngineChm() : EngineEbook() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        kind = kindEngineChm;
        str::ReplaceWithCopy(&defaultExt, ".chm");
    }
    ~EngineChm() override {
        delete dataCache;
        delete doc;
        delete tocTree;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);

  protected:
    ChmFile* doc = nullptr;
    ChmDataCache* dataCache = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);

    IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
};

// cf. http://www.w3.org/TR/html4/charset.html#h-5.2.2
static uint ExtractHttpCharset(const char* html, size_t htmlLen) {
    if (!strstr(html, "charset=")) {
        return 0;
    }

    HtmlPullParser parser(html, std::min(htmlLen, (size_t)1024));
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (tok->tag != Tag_Meta) {
            continue;
        }
        AttrInfo* attr = tok->GetAttrByName("http-equiv");
        if (!attr || !attr->ValIs("Content-Type")) {
            continue;
        }
        attr = tok->GetAttrByName("content");
        AutoFree mimetype, charset;
        if (!attr || !str::Parse(attr->val, attr->valLen, "%S;%_charset=%S", &mimetype, &charset)) {
            continue;
        }

        static struct {
            const char* name;
            uint codepage;
        } codepages[] = {
            {"ISO-8859-1", 1252},  {"Latin1", 1252},   {"CP1252", 1252},   {"Windows-1252", 1252},
            {"ISO-8859-2", 28592}, {"Latin2", 28592},  {"CP1251", 1251},   {"Windows-1251", 1251},
            {"KOI8-R", 20866},     {"shift-jis", 932}, {"x-euc", 932},     {"euc-kr", 949},
            {"Big5", 950},         {"GB2312", 936},    {"UTF-8", CP_UTF8},
        };
        for (int i = 0; i < dimof(codepages); i++) {
            if (str::EqI(charset, codepages[i].name)) {
                return codepages[i].codepage;
            }
        }
        break;
    }

    return 0;
}

class ChmHtmlCollector : public EbookTocVisitor {
    ChmFile* doc = nullptr;
    StrVec added;
    str::Str html;

  public:
    explicit ChmHtmlCollector(ChmFile* doc) : doc(doc) {
        // can be big
    }

    char* GetHtml() {
        // first add the homepage
        const char* index = doc->GetHomePath();
        AutoFreeWstr urlW(doc->ToWstr(index));
        char* url = ToUtf8Temp(urlW);
        Visit(nullptr, url, 0);

        // then add all pages linked to from the table of contents
        doc->ParseToc(this);

        // finally add all the remaining HTML files
        StrVec paths;
        doc->GetAllPaths(&paths);
        for (char* path : paths) {
            if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html")) {
                if (*path == '/') {
                    path++;
                }
                urlW.Set(ToWstr(path));
                url = ToUtf8Temp(urlW);
                Visit(nullptr, url, -1);
            }
        }
        return html.StealData();
    }

    void Visit(const char*, const char* url, int) override {
        if (!url || url::IsAbsolute(url)) {
            return;
        }
        char* plainUrl = url::GetFullPathTemp(url);
        if (added.FindI(plainUrl) != -1) {
            return;
        }
        InterlockedIncrement(&gAllowAllocFailure);
        defer {
            InterlockedDecrement(&gAllowAllocFailure);
        };
        ByteSlice pageHtml = doc->GetData(plainUrl);
        if (!pageHtml) {
            return;
        }
        html.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", plainUrl);
        uint charset = ExtractHttpCharset((const char*)pageHtml.Get(), pageHtml.size());
        html.AppendAndFree(doc->ToUtf8((const u8*)pageHtml.Get(), charset));
        added.Append(plainUrl);
        pageHtml.Free();
    }
};

bool EngineChm::Load(const char* fileName) {
    SetFilePath(fileName);
    doc = ChmFile::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    char* html = ChmHtmlCollector(doc).GetHtml();
    dataCache = new ChmDataCache(doc, html);

    HtmlFormatterArgs args;
    args.htmlStr = dataCache->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::GdiplusQuick;

    pages = ChmFormatter(&args, dataCache).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

IPageDestination* EngineChm::GetNamedDest(const char* name) {
    IPageDestination* dest = EngineEbook::GetNamedDest(name);
    if (dest) {
        return dest;
    }
    unsigned int topicID;
    if (str::Parse(name, "%u%$", &topicID)) {
        char* url = doc->ResolveTopicID(topicID);
        if (url) {
            dest = EngineEbook::GetNamedDest(url);
            str::Free(url);
        }
    }
    return dest;
}

TocTree* EngineChm::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    if (doc->HasIndex()) {
        // TODO: ToC code doesn't work too well for displaying an index,
        //       so this should really become a tree of its own (which
        //       doesn't rely on entries being in the same order as pages)
        builder.Visit("Index", nullptr, 1);
        builder.SetIsIndex(true);
        doc->ParseIndex(&builder);
    }
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

static IPageDestination* newChmEmbeddedDest(const char* path) {
    auto res = new PageDestination();
    res->kind = kindDestinationLaunchEmbedded;
    res->value = str::Dup(path::GetBaseNameTemp(path));
    return res;
}

IPageElement* EngineChm::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    IPageElement* linkEl = EngineEbook::CreatePageLink(link, rect, pageNo);
    if (linkEl) {
        return linkEl;
    }

    DrawInstr* baseAnchor = baseAnchors.at(pageNo - 1);
    AutoFreeStr basePath = str::Dup(baseAnchor->str.s, baseAnchor->str.len);
    AutoFreeStr url = str::Dup(link->str.s, link->str.len);
    url.Set(NormalizeURL(url, basePath));
    if (!doc->HasData(url)) {
        return nullptr;
    }

    IPageDestination* dest = newChmEmbeddedDest(url);
    return NewEbookLink(link, rect, dest, pageNo);
}

EngineBase* EngineChm::CreateFromFile(const char* fileName) {
    EngineChm* engine = new EngineChm();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineChmFromFile(const char* fileName) {
    return EngineChm::CreateFromFile(fileName);
}

/* EngineBase for handling HTML documents */
/* (mainly to allow creating minimal regression test testcases more easily) */

class EngineHtml : public EngineEbook {
  public:
    EngineHtml() : EngineEbook() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        str::ReplaceWithCopy(&defaultExt, ".html");
    }
    ~EngineHtml() override {
        delete doc;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    static EngineBase* CreateFromFile(const char* fileName);

  protected:
    HtmlDoc* doc = nullptr;

    bool Load(const char* fileName);

    IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
};

bool EngineHtml::Load(const char* fileName) {
    SetFilePath(fileName);

    doc = HtmlDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::Gdiplus;

    pages = HtmlFileFormatter(&args, doc).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

static IPageDestination* newRemoteHtmlDest(const char* relativeURL) {
    auto* res = new PageDestination();
    const char* id = str::FindChar(relativeURL, '#');
    if (id) {
        res->value = str::Dup(relativeURL, id - relativeURL);
        res->name = str::Dup(id);
    } else {
        res->value = str::Dup(relativeURL);
    }
    res->kind = kindDestinationLaunchFile;
    return res;
}

IPageElement* EngineHtml::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    if (0 == link->str.len) {
        return nullptr;
    }

    char* url = strconv::FromHtmlUtf8Temp(link->str.s, link->str.len);
    if (url::IsAbsolute(url) || '#' == *url) {
        return EngineEbook::CreatePageLink(link, rect, pageNo);
    }

    IPageDestination* dest = newRemoteHtmlDest(url);
    return NewEbookLink(link, rect, dest, pageNo, true);
}

EngineBase* EngineHtml::CreateFromFile(const char* fileName) {
    EngineHtml* engine = new EngineHtml();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineHtmlFromFile(const char* fileName) {
    return EngineHtml::CreateFromFile(fileName);
}

/* EngineBase for handling TXT documents */

class EngineTxt : public EngineEbook {
  public:
    EngineTxt() : EngineEbook() {
        kind = kindEngineTxt;
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        str::ReplaceWithCopy(&defaultExt, ".txt");
    }
    ~EngineTxt() override {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const char* fileName = FilePath();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    char* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);

  protected:
    TxtDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const char* fileName);
};

bool EngineTxt::Load(const char* fileName) {
    if (!fileName) {
        return false;
    }

    SetFilePath(fileName);

    str::ReplaceWithCopy(&defaultExt, path::GetExtTemp(fileName));

    doc = TxtDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    if (doc->IsRFC()) {
        // RFCs are targeted at letter size pages
        pageRect = RectF(0, 0, 8.5f * GetFileDPI(), 11.f * GetFileDPI());
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = &allocator;
    args.textRenderMethod = mui::TextRenderMethod::Gdiplus;

    pages = TxtFormatter(&args).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

TocTree* EngineTxt::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    auto* root = builder.GetRoot();

    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

EngineBase* EngineTxt::CreateFromFile(const char* fileName) {
    EngineTxt* engine = new EngineTxt();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineTxtFromFile(const char* fileName) {
    return EngineTxt::CreateFromFile(fileName);
}

void EngineEbookCleanup() {
    gDefaultFontName.Reset();
}
