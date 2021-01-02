/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
#include "utils/PalmDbReader.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineEbook.h"
#include "EbookBase.h"
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

static AutoFreeWstr gDefaultFontName;
static float gDefaultFontSize = 10.f;

static const WCHAR* GetDefaultFontName() {
    return gDefaultFontName.Get() ? gDefaultFontName.Get() : L"Georgia";
}

static float GetDefaultFontSize() {
    // fonts are scaled at higher DPI settings,
    // undo this here for (mostly) consistent results
    return gDefaultFontSize * 96.0f / (float)DpiGetForHwnd(HWND_DESKTOP);
}

void SetDefaultEbookFont(const WCHAR* name, float size) {
    // intentionally don't validate the input
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
    bool abort{false};
    EbookAbortCookie() {
    }
    void Abort() override {
        abort = true;
    }
};

class EngineEbook : public EngineBase {
  public:
    EngineEbook();
    virtual ~EngineEbook();

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;

    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    PageText ExtractPageText(int pageNo) override;
    // make RenderCache request larger tiles than per default
    bool HasClipOptimizations(int pageNo) override;

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
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
    WCHAR* ExtractFontList();

    virtual PageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo);

    Vec<DrawInstr>* GetHtmlPage(int pageNo);
};

static PageElement* newEbookLink(DrawInstr* link, Rect rect, PageDestination* dest, int pageNo = 0,
                                 bool showUrl = false) {
    auto res = new PageElement();
    res->pageNo = pageNo;

    res->kind_ = kindPageElementDest;
    res->rect = ToRectFl(rect);

    if (!dest || showUrl) {
        res->value = strconv::FromHtmlUtf8(link->str.s, link->str.len);
    }

    if (!dest) {
        dest = new PageDestination();
        dest->kind = kindDestinationLaunchURL;
        // TODO: not sure about this
        dest->value = str::Dup(res->value);
        dest->pageNo = 0;
        dest->rect = ToRectFl(rect);
    }
    res->dest = dest;
    return res;
}

static PageElement* newImageDataElement(int pageNo, Rect bbox, int imageID) {
    auto res = new PageElement();
    res->kind_ = kindPageElementImage;
    res->pageNo = pageNo;
    res->rect = ToRectFl(bbox);
    res->imageID = imageID;
    return res;
}

static TocItem* newEbookTocItem(TocItem* parent, const WCHAR* title, PageDestination* dest) {
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
    preferredLayout = Layout_Book;
    InitializeCriticalSection(&pagesAccess);
}

EngineEbook::~EngineEbook() {
    EnterCriticalSection(&pagesAccess);

    if (pages) {
        DeleteVecMembers(*pages);
    }
    delete pages;

    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

RectF EngineEbook::PageMediabox([[maybe_unused]] int pageNo) {
    return pageRect;
}

RectF EngineEbook::PageContentBox(int pageNo, [[maybe_unused]] RenderTarget target) {
    RectF mbox = PageMediabox(pageNo);
    mbox.Inflate(-pageBorder, -pageBorder);
    return mbox;
}

std::span<u8> EngineEbook::GetFileData() {
    const WCHAR* fileName = FileName();
    if (!fileName) {
        return {};
    }
    return file::ReadFile(fileName);
}

bool EngineEbook::SaveFileAs(const char* copyFileName, [[maybe_unused]] bool includeUserAnnots) {
    const WCHAR* fileName = FileName();
    if (!fileName) {
        return false;
    }
    AutoFreeWstr path = strconv::Utf8ToWstr(copyFileName);
    auto res = CopyFileW(fileName, path, FALSE);
    return res != 0;
}

// make RenderCache request larger tiles than per default
bool EngineEbook::HasClipOptimizations([[maybe_unused]] int pageNo) {
    return false;
}

bool EngineEbook::BenchLoadPage([[maybe_unused]] int pageNo) {
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

RectF EngineEbook::Transform(const RectF& rect, [[maybe_unused]] int pageNo, float zoom, int rotation, bool inverse) {
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
    auto pageRect = args.pageRect;

    RectF pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
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

    gAllowAllocFailure++;
    defer {
        gAllowAllocFailure--;
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
                        content.Append(' ');
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
                        content.Append(' ');
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

PageElement* EngineEbook::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    AutoFreeWstr url(strconv::FromHtmlUtf8(link->str.s, link->str.len));
    if (url::IsAbsolute(url)) {
        return newEbookLink(link, rect, nullptr, pageNo);
    }

    DrawInstr* baseAnchor = baseAnchors.at(pageNo - 1);
    if (baseAnchor) {
        AutoFree basePath(str::DupN(baseAnchor->str.s, baseAnchor->str.len));
        AutoFree relPath(ResolveHtmlEntities(link->str.s, link->str.len));
        AutoFree absPath(NormalizeURL(relPath, basePath));
        url.Set(strconv::Utf8ToWstr(absPath.Get()));
    }

    PageDestination* dest = GetNamedDest(url);
    if (!dest) {
        return nullptr;
    }
    return newEbookLink(link, rect, dest, pageNo);
}

Vec<IPageElement*>* EngineEbook::GetElements(int pageNo) {
    auto els = new Vec<IPageElement*>();

    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    size_t n = pageInstrs->size();
    for (size_t idx = 0; idx < n; idx++) {
        DrawInstr& i = pageInstrs->at(idx);
        if (DrawInstrType::Image == i.type) {
            auto box = GetInstrBbox(i, pageBorder);
            auto el = newImageDataElement(pageNo, box, (int)idx);
            els->Append(el);
        } else if (DrawInstrType::LinkStart == i.type && !i.bbox.IsEmpty()) {
            IPageElement* link = CreatePageLink(&i, GetInstrBbox(i, pageBorder), pageNo);
            if (link) {
                els->Append(link);
            }
        }
    }

    return els;
}

static RenderedBitmap* getImageFromData(ImageData imageData) {
    HBITMAP hbmp{nullptr};
    Bitmap* bmp = BitmapFromData(imageData.AsSpan());
    if (!bmp || bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok) {
        delete bmp;
        return nullptr;
    }
    Size size(bmp->GetWidth(), bmp->GetHeight());
    delete bmp;
    return new RenderedBitmap(hbmp, size);
}

RenderedBitmap* EngineEbook::GetImageForPageElement(IPageElement* iel) {
    PageElement* el = (PageElement*)iel;
    int pageNo = el->pageNo;
    int idx = el->imageID;
    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    const DrawInstr& i = pageInstrs->at(idx);
    CrashIf(i.type != DrawInstrType::Image);
    return getImageFromData(i.img);
}

IPageElement* EngineEbook::GetElementAtPos(int pageNo, PointF pt) {
    auto els = GetElements(pageNo);
    if (!els) {
        return nullptr;
    }

    IPageElement* el = nullptr;
    for (size_t i = 0; i < els->size() && !el; i++) {
        if (els->at(i)->GetRect().Contains(pt)) {
            el = els->at(i);
        }
    }

    if (el) {
        els->Remove(el);
    }
    DeleteVecMembers(*els);
    delete els;

    return el;
}

PageDestination* EngineEbook::GetNamedDest(const WCHAR* name) {
    AutoFree name_utf8(strconv::WstrToUtf8(name));
    const char* id = name_utf8.Get();
    if (str::FindChar(id, '#')) {
        id = str::FindChar(id, '#') + 1;
    }

    // if the name consists of both path and ID,
    // try to first skip to the page with the desired
    // path before looking for the ID to allow
    // for the same ID to be reused on different pages
    DrawInstr* baseAnchor = nullptr;
    int basePageNo = 0;
    if (id > name_utf8.Get() + 1) {
        size_t base_len = id - name_utf8.Get() - 1;
        for (size_t i = 0; i < baseAnchors.size(); i++) {
            DrawInstr* anchor = baseAnchors.at(i);
            if (anchor && base_len == anchor->str.len && str::EqNI(name_utf8.Get(), anchor->str.s, base_len)) {
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
            return newSimpleDest(anchor->pageNo, rect);
        }
    }

    // don't fail if an ID doesn't exist in a merged document
    if (basePageNo != 0) {
        RectF rect(0, pageBorder, pageRect.dx, 10);
        rect.Inflate(-pageBorder, 0);
        return newSimpleDest(basePageNo, rect);
    }

    return nullptr;
}

WCHAR* EngineEbook::ExtractFontList() {
    ScopedCritSec scope(&pagesAccess);

    Vec<mui::CachedFont*> seenFonts;
    WStrVec fonts;

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
            WCHAR fontName[LF_FACESIZE];
            ok = family.GetFamilyName(fontName);
            if (ok != Ok || fonts.FindI(fontName) != -1) {
                continue;
            }
            fonts.Append(str::Dup(fontName));
        }
    }
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    return fonts.Join(L"\n");
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

    void Visit(const WCHAR* name, const WCHAR* url, int level) override;

    TocItem* GetRoot() {
        return root;
    }
    void SetIsIndex(bool value) {
        isIndex = value;
    }
};

void EbookTocBuilder::Visit(const WCHAR* name, const WCHAR* url, int level) {
    PageDestination* dest;
    if (!url) {
        dest = nullptr;
    } else if (url::IsAbsolute(url)) {
        dest = newSimpleDest(0, RectF(), str::Dup(url));
    } else {
        dest = engine->GetNamedDest(url);
        if (!dest && str::FindChar(url, '%')) {
            AutoFreeWstr decodedUrl(str::Dup(url));
            url::DecodeInPlace(decodedUrl);
            dest = engine->GetNamedDest(decodedUrl);
        }
    }

    // TODO; send parent to newEbookTocItem
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
    virtual ~EngineEpub();
    EngineBase* Clone() override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    EpubDoc* doc = nullptr;
    IStream* stream = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

EngineEpub::EngineEpub() : EngineEbook() {
    kind = kindEngineEpub;
    defaultFileExt = L".epub";
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
    if (FileName()) {
        return CreateFromFile(FileName());
    }
    return nullptr;
}

bool EngineEpub::Load(const WCHAR* fileName) {
    SetFileName(fileName);
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
    args.textRenderMethod = mui::TextRenderMethodGdiplusQuick;

    pages = EpubFormatter(&args, doc).FormatAllPages(false);

    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    if (doc->IsRTL()) {
        preferredLayout = (PageLayoutType)(Layout_Book | Layout_R2L);
    } else {
        preferredLayout = Layout_Book;
    }

    return pageCount > 0;
}

std::span<u8> EngineEpub::GetFileData() {
    const WCHAR* fileName = FileName();
    return GetStreamOrFileData(stream, fileName);
}

bool EngineEpub::SaveFileAs(const char* copyFileName, [[maybe_unused]] bool includeUserAnnots) {
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);

    if (stream) {
        AutoFree d = GetDataFromStream(stream, nullptr);
        bool ok = !d.empty() && file::WriteFile(dstPath, d.AsSpan());
        if (ok) {
            return true;
        }
    }
    const WCHAR* fileName = FileName();
    if (!fileName) {
        return false;
    }
    return CopyFileW(fileName, dstPath, FALSE);
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
    tocTree = new TocTree(root);
    return tocTree;
}

EngineBase* EngineEpub::CreateFromFile(const WCHAR* fileName) {
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

EngineBase* CreateEpubEngineFromFile(const WCHAR* fileName) {
    return EngineEpub::CreateFromFile(fileName);
}

EngineBase* CreateEpubEngineFromStream(IStream* stream) {
    return EngineEpub::CreateFromStream(stream);
}

/* EngineBase for handling FictionBook2 documents */

class EngineFb2 : public EngineEbook {
  public:
    EngineFb2() : EngineEbook() {
        kind = kindEngineFb2;
        defaultFileExt = L".fb2";
    }
    virtual ~EngineFb2() {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    Fb2Doc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

bool EngineFb2::Load(const WCHAR* fileName) {
    SetFileName(fileName);
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
    args.textRenderMethod = mui::TextRenderMethodGdiplusQuick;

    if (doc->IsZipped()) {
        defaultFileExt = L".fb2z";
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
    tocTree = new TocTree(root);
    return tocTree;
}

EngineBase* EngineFb2::CreateFromFile(const WCHAR* fileName) {
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

EngineBase* CreateFb2EngineFromFile(const WCHAR* fileName) {
    return EngineFb2::CreateFromFile(fileName);
}

EngineBase* CreateFb2EngineFromStream(IStream* stream) {
    return EngineFb2::CreateFromStream(stream);
}

/* EngineBase for handling Mobi documents */

#include "MobiDoc.h"

class EngineMobi : public EngineEbook {
  public:
    EngineMobi() : EngineEbook() {
        kind = kindEngineMobi;
        defaultFileExt = L".mobi";
    }
    ~EngineMobi() override {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    MobiDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
};

bool EngineMobi::Load(const WCHAR* fileName) {
    SetFileName(fileName);
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
    args.textRenderMethod = mui::TextRenderMethodGdiplusQuick;

    pages = MobiFormatter(&args, doc).FormatAllPages();
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }
    return pageCount > 0;
}

PageDestination* EngineMobi::GetNamedDest(const WCHAR* name) {
    int filePos = _wtoi(name);
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

    const std::span<u8> htmlData = doc->GetHtmlData();
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
    return newSimpleDest(pageNo, rect);
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
    tocTree = new TocTree(root);
    return tocTree;
}

EngineBase* EngineMobi::CreateFromFile(const WCHAR* fileName) {
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

EngineBase* CreateMobiEngineFromFile(const WCHAR* fileName) {
    return EngineMobi::CreateFromFile(fileName);
}

EngineBase* CreateMobiEngineFromStream(IStream* stream) {
    return EngineMobi::CreateFromStream(stream);
}

/* EngineBase for handling PalmDOC documents (and extensions such as TealDoc) */

class EnginePdb : public EngineEbook {
  public:
    EnginePdb() : EngineEbook() {
        kind = kindEnginePdb;
        defaultFileExt = L".pdb";
    }
    virtual ~EnginePdb() {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    PalmDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
};

bool EnginePdb::Load(const WCHAR* fileName) {
    SetFileName(fileName);

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
    args.textRenderMethod = mui::TextRenderMethodGdiplusQuick;

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
    tocTree = new TocTree(root);
    return tocTree;
}

EngineBase* EnginePdb::CreateFromFile(const WCHAR* fileName) {
    EnginePdb* engine = new EnginePdb();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreatePdbEngineFromFile(const WCHAR* fileName) {
    return EnginePdb::CreateFromFile(fileName);
}

/* formatting extensions for CHM */

#include "ChmDoc.h"

class ChmDataCache {
    ChmDoc* doc = nullptr; // owned by creator
    AutoFree html;
    Vec<ImageData2> images;

  public:
    ChmDataCache(ChmDoc* doc, char* html) : doc(doc), html(html) {
    }

    ~ChmDataCache() {
        for (size_t i = 0; i < images.size(); i++) {
            free(images.at(i).base.data);
            free(images.at(i).fileName);
        }
    }

    std::span<u8> GetHtmlData() {
        return html.AsSpan();
    }

    ImageData* GetImageData(const char* id, const char* pagePath) {
        AutoFree url(NormalizeURL(id, pagePath));
        for (size_t i = 0; i < images.size(); i++) {
            if (str::Eq(images.at(i).fileName, url)) {
                return &images.at(i).base;
            }
        }

        auto tmp = doc->GetData(url);
        if (tmp.empty()) {
            return nullptr;
        }

        ImageData2 data = {0};
        data.base.data = (char*)tmp.data();
        data.base.len = tmp.size();

        data.fileName = url.Release();
        images.Append(data);
        return &images.Last().base;
    }

    std::span<u8> GetFileData(const char* relPath, const char* pagePath) {
        AutoFree url(NormalizeURL(relPath, pagePath));
        return doc->GetData(url);
    }
};

class ChmFormatter : public HtmlFormatter {
  protected:
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagPagebreak(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;

    ChmDataCache* chmDoc = nullptr;
    AutoFree pagePath;

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
        AutoFree src(str::DupN(attr->val, attr->valLen));
        url::DecodeInPlace(src);
        ImageData* img = chmDoc->GetImageData(src, pagePath);
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
        pagePath.Set(str::DupN(attr->val, attr->valLen));
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

    AutoFree src(str::DupN(attr->val, attr->valLen));
    url::DecodeInPlace(src);
    AutoFree data = chmDoc->GetFileData(src, pagePath);
    if (data.data) {
        ParseStyleSheet(data.data, data.size());
    }
}

/* EngineBase for handling CHM documents */

class EngineChm : public EngineEbook {
  public:
    EngineChm() : EngineEbook() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        kind = kindEngineChm;
        defaultFileExt = L".chm";
    }
    virtual ~EngineChm() {
        delete dataCache;
        delete doc;
        delete tocTree;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    ChmDoc* doc = nullptr;
    ChmDataCache* dataCache = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);

    PageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
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
    ChmDoc* doc{nullptr};
    WStrList added;
    str::Str html;

  public:
    explicit ChmHtmlCollector(ChmDoc* doc) : doc(doc) {
        // can be big
    }

    char* GetHtml() {
        // first add the homepage
        const char* index = doc->GetHomePath();
        AutoFreeWstr url(doc->ToStr(index));
        Visit(nullptr, url, 0);

        // then add all pages linked to from the table of contents
        doc->ParseToc(this);

        // finally add all the remaining HTML files
        Vec<char*>* paths = doc->GetAllPaths();
        for (size_t i = 0; i < paths->size(); i++) {
            char* path = paths->at(i);
            if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html")) {
                if (*path == '/') {
                    path++;
                }
                url.Set(strconv::Utf8ToWstr(path));
                Visit(nullptr, url, -1);
            }
        }
        paths->FreeMembers();
        delete paths;

        return html.StealData();
    }

    void Visit([[maybe_unused]] const WCHAR* name, const WCHAR* url, [[maybe_unused]] int level) override {
        if (!url || url::IsAbsolute(url)) {
            return;
        }
        AutoFreeWstr plainUrl(url::GetFullPath(url));
        if (added.FindI(plainUrl) != -1) {
            return;
        }
        gAllowAllocFailure++;
        defer {
            gAllowAllocFailure--;
        };
        AutoFree urlUtf8(strconv::WstrToUtf8(plainUrl));
        AutoFree pageHtml = doc->GetData(urlUtf8.Get());
        if (!pageHtml) {
            return;
        }
        html.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", urlUtf8.Get());
        auto charset = ExtractHttpCharset((const char*)pageHtml.Get(), pageHtml.size());
        html.AppendAndFree(doc->ToUtf8((const u8*)pageHtml.data, charset));
        added.Append(plainUrl.StealData());
    }
};

bool EngineChm::Load(const WCHAR* fileName) {
    SetFileName(fileName);
    doc = ChmDoc::CreateFromFile(fileName);
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
    args.textRenderMethod = mui::TextRenderMethodGdiplusQuick;

    pages = ChmFormatter(&args, dataCache).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

PageDestination* EngineChm::GetNamedDest(const WCHAR* name) {
    PageDestination* dest = EngineEbook::GetNamedDest(name);
    if (dest) {
        return dest;
    }
    unsigned int topicID;
    if (str::Parse(name, L"%u%$", &topicID)) {
        AutoFree urlUtf8(doc->ResolveTopicID(topicID));
        if (urlUtf8) {
            AutoFreeWstr url = strconv::Utf8ToWstr(urlUtf8.Get());
            dest = EngineEbook::GetNamedDest(url);
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
        builder.Visit(L"Index", nullptr, 1);
        builder.SetIsIndex(true);
        doc->ParseIndex(&builder);
    }
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    tocTree = new TocTree(root);
    return tocTree;
}

static PageDestination* newChmEmbeddedDest(const char* path) {
    auto res = new PageDestination();
    res->kind = kindDestinationLaunchEmbedded;
    res->value = strconv::Utf8ToWstr(path::GetBaseNameNoFree(path));
    return res;
}

PageElement* EngineChm::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    PageElement* linkEl = EngineEbook::CreatePageLink(link, rect, pageNo);
    if (linkEl) {
        return linkEl;
    }

    DrawInstr* baseAnchor = baseAnchors.at(pageNo - 1);
    AutoFree basePath(str::DupN(baseAnchor->str.s, baseAnchor->str.len));
    AutoFree url(str::DupN(link->str.s, link->str.len));
    url.Set(NormalizeURL(url, basePath));
    if (!doc->HasData(url)) {
        return nullptr;
    }

    PageDestination* dest = newChmEmbeddedDest(url);
    return newEbookLink(link, rect, dest, pageNo);
}

EngineBase* EngineChm::CreateFromFile(const WCHAR* fileName) {
    EngineChm* engine = new EngineChm();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateChmEngineFromFile(const WCHAR* fileName) {
    return EngineChm::CreateFromFile(fileName);
}

/* EngineBase for handling HTML documents */
/* (mainly to allow creating minimal regression test testcases more easily) */

class EngineHtml : public EngineEbook {
  public:
    EngineHtml() : EngineEbook() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        defaultFileExt = L".html";
    }
    virtual ~EngineHtml() {
        delete doc;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    HtmlDoc* doc = nullptr;

    bool Load(const WCHAR* fileName);

    PageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
};

bool EngineHtml::Load(const WCHAR* fileName) {
    SetFileName(fileName);

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
    args.textRenderMethod = mui::TextRenderMethodGdiplus;

    pages = HtmlFileFormatter(&args, doc).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = (int)pages->size();
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

static PageDestination* newRemoteHtmlDest(const WCHAR* relativeURL) {
    auto* res = new PageDestination();
    const WCHAR* id = str::FindChar(relativeURL, '#');
    if (id) {
        res->value = str::DupN(relativeURL, id - relativeURL);
        res->name = str::Dup(id);
    } else {
        res->value = str::Dup(relativeURL);
    }
    res->kind = kindDestinationLaunchFile;
    return res;
}

PageElement* EngineHtml::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    if (0 == link->str.len) {
        return nullptr;
    }

    AutoFreeWstr url(strconv::FromHtmlUtf8(link->str.s, link->str.len));
    if (url::IsAbsolute(url) || '#' == *url) {
        return EngineEbook::CreatePageLink(link, rect, pageNo);
    }

    PageDestination* dest = newRemoteHtmlDest(url);
    return newEbookLink(link, rect, dest, pageNo, true);
}

EngineBase* EngineHtml::CreateFromFile(const WCHAR* fileName) {
    EngineHtml* engine = new EngineHtml();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateHtmlEngineFromFile(const WCHAR* fileName) {
    return EngineHtml::CreateFromFile(fileName);
}

/* EngineBase for handling TXT documents */

class EngineTxt : public EngineEbook {
  public:
    EngineTxt() : EngineEbook() {
        kind = kindEngineTxt;
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        defaultFileExt = L".txt";
    }
    virtual ~EngineTxt() {
        delete tocTree;
        delete doc;
    }
    EngineBase* Clone() override {
        const WCHAR* fileName = FileName();
        if (!fileName) {
            return nullptr;
        }
        return CreateFromFile(fileName);
    }

    WCHAR* GetProperty(DocumentProperty prop) override {
        return prop != DocumentProperty::FontList ? doc->GetProperty(prop) : ExtractFontList();
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    TxtDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
};

bool EngineTxt::Load(const WCHAR* fileName) {
    if (!fileName) {
        return false;
    }

    SetFileName(fileName);

    defaultFileExt = path::GetExtNoFree(fileName);

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
    args.textRenderMethod = mui::TextRenderMethodGdiplus;

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

    tocTree = new TocTree(root);
    return tocTree;
}

EngineBase* EngineTxt::CreateFromFile(const WCHAR* fileName) {
    EngineTxt* engine = new EngineTxt();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateTxtEngineFromFile(const WCHAR* fileName) {
    return EngineTxt::CreateFromFile(fileName);
}

void EngineEbookCleanup() {
    gDefaultFontName.Reset();
}
