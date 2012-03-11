/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Test engine to see how well the BaseEngine API fits a flowed ebook format.
// (pages are layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "EpubEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Scoped.h"
#include "Allocator.h"

namespace str {
    namespace conv {

inline TCHAR *FromUtf8N(const char *s, size_t len)
{
    ScopedMem<char> tmp(str::DupN(s, len));
    return str::conv::FromUtf8(tmp);
}

    }
}

/* epub loading code (was ebooktest2/EpubDoc.cpp and ebooktest2/BaseEbookDoc.h) */

#include "ZipUtil.h"
#include "TrivialHtmlParser.h"
#include "HtmlPullParser.h"
// for ImageData
#include "MobiDoc.h"

struct ImageData2 {
    ImageData base;
    char *  id; // path by which content refers to this image
    size_t  idx; // document specific index at which to find this image
};

class EpubEngineImpl;

class EpubDoc {
    friend EpubEngineImpl;

    ZipFile zip;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    Vec<const char *> props;
    ScopedMem<TCHAR> tocPath;

    bool Load();
    void ParseMetadata(const char *content);

    static bool VerifyEpub(ZipFile& zip) {
        ScopedMem<char> firstFileData(zip.GetFileData(_T("mimetype")));
        // a proper Epub documents has a "mimetype" file with content
        // "application/epub+zip" as the first entry in its ZIP structure
        return str::Eq(zip.GetFileName(0), _T("mimetype")) &&
               str::Eq(firstFileData, "application/epub+zip");
    }

public:
    EpubDoc(const TCHAR *fileName) : zip(fileName) { }
    EpubDoc(IStream *stream) : zip(stream) { }
    ~EpubDoc() {
        for (size_t i = 0; i < images.Count(); i++) {
            free(images.At(i).base.data);
            free(images.At(i).id);
        }
        for (size_t i = 1; i < props.Count(); i += 2) {
            free((void *)props.At(i));
        }
    }

    const char *GetBookHtmlData(size_t *lenOut) {
        *lenOut = htmlData.Size();
        return htmlData.Get();
    }

    ImageData *GetImageData(const char *id) {
        // TODO: paths are relative from the html document to the image
        for (size_t i = 0; i < images.Count(); i++) {
            if (str::EndsWith(id, images.At(i).id))
                return GetImageData(i);
        }
        return NULL;
    }

    ImageData *GetImageData(size_t index) {
        if (index >= images.Count())
            return NULL;
        if (!images.At(index).base.data) {
            images.At(index).base.data = zip.GetFileData(images.At(index).idx, &images.At(index).base.len);
            if (!images.At(index).base.data)
                return NULL;
        }
        return &images.At(index).base;
    }

    TCHAR *GetProperty(const char *name) {
        for (size_t i = 0; i < props.Count(); i += 2) {
            if (str::Eq(props.At(i), name))
                return str::conv::FromUtf8(props.At(i + 1));
        }
        return NULL;
    }

    char *GetToC() {
        if (!tocPath)
            return NULL;
        return zip.GetFileData(tocPath);
    }

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff) {
        if (sniff) {
            return VerifyEpub(ZipFile(fileName));
        }
        return str::EndsWithI(fileName, _T(".epub"));
    }
};

static void UrlDecode(TCHAR *url)
{
    for (TCHAR *src = url; *src; src++, url++) {
        int val;
        if (*src == '%' && str::Parse(src, _T("%%%2x"), &val)) {
            *url = (char)val;
            src += 2;
        } else {
            *url = *src;
        }
    }
    *url = '\0';
}

bool EpubDoc::Load()
{
    if (!VerifyEpub(zip))
        return false;

    ScopedMem<char> container(zip.GetFileData(_T("META-INF/container.xml")));
    HtmlParser parser;
    HtmlElement *node = parser.ParseInPlace(container);
    if (!node)
        return false;
    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByName("rootfile");
    if (!node)
        return false;
    ScopedMem<TCHAR> contentPath(node->GetAttribute("full-path"));
    if (!contentPath)
        return false;

    ScopedMem<char> content(zip.GetFileData(contentPath));
    if (!content)
        return false;
    ParseMetadata(content);
    node = parser.ParseInPlace(content);
    if (!node)
        return false;
    node = parser.FindElementByName("manifest");
    if (!node)
        return false;

    if (str::FindChar(contentPath, '/'))
        *(TCHAR *)(str::FindCharLast(contentPath, '/') + 1) = '\0';
    else
        *contentPath = '\0';
    StrVec idPathMap;

    for (node = node->down; node; node = node->next) {
        ScopedMem<TCHAR> mediatype(node->GetAttribute("media-type"));
        if (str::Eq(mediatype, _T("application/xhtml+xml"))) {
            ScopedMem<TCHAR> htmlPath(node->GetAttribute("href"));
            ScopedMem<TCHAR> htmlId(node->GetAttribute("id"));
            if (htmlPath && htmlId) {
                idPathMap.Append(htmlId.StealData());
                idPathMap.Append(htmlPath.StealData());
            }
        }
        else if (str::Eq(mediatype, _T("image/png"))  ||
                 str::Eq(mediatype, _T("image/jpeg")) ||
                 str::Eq(mediatype, _T("image/gif"))) {
            ScopedMem<TCHAR> imgPath(node->GetAttribute("href"));
            if (!imgPath)
                continue;
            ScopedMem<TCHAR> zipPath(str::Join(contentPath, imgPath));
            UrlDecode(zipPath);
            // load the image lazily
            ImageData2 data = { 0 };
            data.id = str::conv::ToUtf8(imgPath);
            data.idx = zip.GetFileIndex(zipPath);
            images.Append(data);
        }
        else if (str::Eq(mediatype, _T("application/x-dtbncx+xml"))) {
            tocPath.Set(node->GetAttribute("href"));
            if (tocPath)
                tocPath.Set(str::Join(contentPath, tocPath));
        }
    }

    node = parser.FindElementByName("spine");
    if (!node)
        return false;
    for (node = node->down; node; node = node->next) {
        if (!str::Eq(node->name, "itemref"))
            continue;
        ScopedMem<TCHAR> idref(node->GetAttribute("idref"));
        if (!idref)
            continue;
        const TCHAR *htmlPath = NULL;
        for (size_t i = 0; i < idPathMap.Count() && !htmlPath; i += 2) {
            if (str::Eq(idref, idPathMap.At(i)))
                htmlPath = idPathMap.At(i+1);
        }
        if (!htmlPath)
            continue;

        ScopedMem<TCHAR> fullPath(str::Join(contentPath, htmlPath));
        ScopedMem<char> html(zip.GetFileData(fullPath));
        if (!html)
            continue;
        if (htmlData.Count() > 0) {
            // insert explicit page-breaks between sections
            htmlData.Append("<pagebreak />");
        }
        // add an anchor with the file name at the top (for internal links)
        ScopedMem<char> utf8_path(str::conv::ToUtf8(htmlPath));
        htmlData.AppendFmt("<a name=\"%s\" />", utf8_path);
        // TODO: merge/remove <head>s and drop everything else outside of <body>s(?)
        htmlData.Append(html);
    }

    return htmlData.Count() > 0;
}

void EpubDoc::ParseMetadata(const char *content)
{
    const char *metadataMap[] = {
        "dc:title",         "Title",
        "dc:creator",       "Author",
        "dc:date",          "CreationDate",
        "dc:description",   "Subject",
        "dc:rights",        "Copyright",
    };

    HtmlPullParser pullParser(content, str::Len(content));
    int insideMetadata = 0;
    HtmlToken *tok;

    while ((tok = pullParser.Next())) {
        if (tok->IsStartTag() && tok->NameIs("metadata"))
            insideMetadata++;
        else if (tok->IsEndTag() && tok->NameIs("metadata"))
            insideMetadata--;
        if (!insideMetadata)
            continue;
        if (!tok->IsStartTag())
            continue;

        for (int i = 0; i < dimof(metadataMap); i += 2) {
            if (tok->NameIs(metadataMap[i])) {
                tok = pullParser.Next();
                if (!tok->IsText())
                    break;
                ScopedMem<char> value(str::DupN(tok->s, tok->sLen));
                char *text = (char *)ResolveHtmlEntities(value, value + tok->sLen, NULL);
                if (text == value)
                    text = str::Dup(text);
                if (text) {
                    props.Append(metadataMap[i+1]);
                    props.Append(text);
                }
                break;
            }
        }
    }
}

/* PageLayout extensions for Epub (was ebooktest2/PageLayout.cpp) */

#include "PageLayout.h"
#include "MiniMui.h"
#include "GdiPlusUtil.h"

struct PageAnchor {
    const char *s;
    size_t len;
    int pageNo;
    float currY;

    PageAnchor(const char *s=NULL, size_t len=0, int pageNo=-1, float currY=-1) :
        s(s), len(len), pageNo(pageNo), currY(currY) { }
};

class PageLayoutEpub : public PageLayout {
    void EmitImage2(ImageData *img);

    void HandleTagImg2(HtmlToken *t);
    void HandleTagHeader(HtmlToken *t);
    void HandleTagA2(HtmlToken *t);
    void HandleHtmlTag2(HtmlToken *t);

    bool IgnoreText();

    Vec<PageAnchor> *anchors;

public:
    PageLayoutEpub(LayoutInfo *li);

    Vec<PageData*> *Layout(Vec<PageAnchor> *anchors=NULL);
};

PageLayoutEpub::PageLayoutEpub(LayoutInfo* li)
{
    CrashIf(currPage);
    finishedParsing = false;
    layoutInfo = li;
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;
    textAllocator = layoutInfo->textAllocator;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    defaultFontName.Set(str::Dup(layoutInfo->fontName));
    defaultFontSize = layoutInfo->fontSize;
    SetCurrentFont(FontStyleRegular, defaultFontSize);

    coverImage = NULL;
    pageCount = 0;
    inLink = false;

    lineSpacing = currFont->GetHeight(gfx);
    spaceDx = currFontSize / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(gfx, currFont);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    currJustification = Align_Justify;
    currX = 0; currY = 0;
    currPage = new PageData;
    currReparsePoint = NULL;

    currLineTopPadding = 0;
    // Epub documents contain no cover image
    anchors = NULL;
}

void PageLayoutEpub::EmitImage2(ImageData *img)
{
    Rect imgSize = BitmapSizeFromData(img->data, img->len);
    if (imgSize.IsEmptyArea())
        return;

    SizeF newSize((REAL)imgSize.Width, (REAL)imgSize.Height);
    // move overly large images to a new line
    if (!IsCurrLineEmpty() && currX + newSize.Width > pageDx)
        FlushCurrLine(false);
    // move overly large images to a new page
    if (currY > 0 && currY + newSize.Height / 2.f > pageDy)
        ForceNewPage();
    // if image is still bigger than available space, scale it down
    if ((newSize.Width > pageDx) || (currY + newSize.Height) > pageDy) {
        REAL scale = min(pageDx / newSize.Width, (pageDy - currY) / newSize.Height);
        newSize.Width *= scale;
        newSize.Height *= scale;
    }

    RectF bbox(PointF(currX, 0), newSize);
    AppendInstr(DrawInstr::Image(img->data, img->len, bbox));
    // add an empty string in place of the image so that
    // we can make the image flow with the text (as long
    // as justification code doesn't take into account images)
    AppendInstr(DrawInstr::Str("", 0, bbox));
    currX += bbox.Width;
}

void PageLayoutEpub::HandleTagImg2(HtmlToken *t)
{
    if (!layoutInfo->mobiDoc)
        return;
    EpubDoc *doc = (EpubDoc *)layoutInfo->mobiDoc;

    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = doc->GetImageData(src);
    if (img)
        EmitImage2(img);
}

void PageLayoutEpub::HandleTagHeader(HtmlToken *t)
{
    if (t->IsEndTag()) {
        HandleTagP(t);
        currY += currFontSize / 2;
        currFontSize = defaultFontSize;
        ChangeFontStyle(FontStyleBold, false);
    }
    else {
        currJustification = Align_Left;
        HandleTagP(t);
        currFontSize = defaultFontSize * pow(1.1f, '5' - t->s[1]);
        ChangeFontStyle(FontStyleBold, true);
        if (currY > 0)
            currY += currFontSize / 2;
    }
}

void PageLayoutEpub::HandleTagA2(HtmlToken *t)
{
    if (t->IsStartTag() && !inLink) {
        AttrInfo *attr = t->GetAttrByName("href");
        if (attr) {
            DrawInstr i(InstrLinkStart);
            i.str.s = attr->val;
            i.str.len = attr->valLen;
            AppendInstr(i);
            inLink = true;
        }
    }
    else if (t->IsEndTag() && inLink) {
        AppendInstr(DrawInstr(InstrLinkEnd));
        inLink = false;
    }
}

void PageLayoutEpub::HandleHtmlTag2(HtmlToken *t)
{
    if (anchors && t->IsTag()) {
        AttrInfo *attrInfo = t->GetAttrByName("id");
        if (!attrInfo && t->NameIs("a"))
            attrInfo = t->GetAttrByName("name");
        if (attrInfo)
            anchors->Append(PageAnchor(attrInfo->val, attrInfo->valLen,
                                       pagesToSend.Count() + 1, currY));
    }

    HtmlTag tag = FindTag(t);
    switch (tag) {
    case Tag_Img:
        HandleTagImg2(t);
        break;
    case Tag_Pagebreak:
        ForceNewPage();
        break;
    case Tag_Ul: case Tag_Ol: case Tag_Dl:
        currJustification = Align_Left;
        break;
    case Tag_Li: case Tag_Dd: case Tag_Dt:
        FlushCurrLine(false);
        break;
    case Tag_Center:
        currJustification = Align_Center;
        break;
    case Tag_H1: case Tag_H2: case Tag_H3:
    case Tag_H4: case Tag_H5:
        HandleTagHeader(t);
        break;
    case Tag_A:
        HandleTagA2(t);
        break;
    case Tag_P: case Tag_Hr: case Tag_B:
    case Tag_Strong: case Tag_I: case Tag_Em:
    case Tag_U: case Tag_Strike: case Tag_Mbp_Pagebreak:
    case Tag_Br: case Tag_Font: /* case Tag_Img: */
    /* case Tag_A: */ case Tag_Blockquote: case Tag_Div:
    case Tag_Sup: case Tag_Sub: case Tag_Span:
        HandleHtmlTag(t);
        break;
    default:
        // ignore instead of crashing in HandleHtmlTag
        break;
    }
}

bool PageLayoutEpub::IgnoreText()
{
    // ignore the content of <head>, <style> and <title> tags
    return htmlParser->tagNesting.Find(Tag_Head) != -1 ||
           htmlParser->tagNesting.Find(Tag_Style) != -1 ||
           htmlParser->tagNesting.Find(Tag_Title) != -1;
}

Vec<PageData*> *PageLayoutEpub::Layout(Vec<PageAnchor> *anchors)
{
    // optionally extract anchor information
    this->anchors = anchors;

    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag2(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<PageData *> *result = new Vec<PageData *>(pagesToSend);
    pagesToSend.Reset();
    return result;
}

/* actual BaseEngine for handling Epub documents */

class EpubEngineImpl : public EpubEngine {
    friend EpubEngine;

public:
    EpubEngineImpl();
    virtual ~EpubEngineImpl();
    virtual EpubEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages ? pages->Count() : 0; }

    virtual RectD PageMediabox(int pageNo) { return pageRect; }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        RectD mbox = PageMediabox(pageNo);
        mbox.Inflate(-pageBorder, -pageBorder);
        return mbox;
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount) {
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    // make RenderCache request larger tiles than per default
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Book; }

    virtual TCHAR *GetProperty(char *name) {
        return doc ? doc->GetProperty(name) : NULL;
    }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".epub"); }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual PageDestination *GetNamedDest(const TCHAR *name);
    virtual bool HasTocTree() const {
        return doc && ScopedMem<char>(doc->GetToC()) != NULL;
    }
    virtual DocTocItem *GetTocTree();

    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    const TCHAR *fileName;
    EpubDoc *doc;
    Vec<PageData *> *pages;
    Vec<PageAnchor> anchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // needed since pages::IterStart/IterNext aren't thread-safe
    CRITICAL_SECTION iterAccess;
    // needed to undo the DPI specific UnitPoint-UnitPixel conversion
    int currFontDpi;

    RectD pageRect;
    float pageBorder;

    bool Load(const TCHAR *fileName);
    bool Load(IStream *stream);
    bool FinishLoading();

    void GetTransform(Matrix& m, float zoom, int rotation) {
        GetBaseTransform(m, RectF(0, 0, (REAL)pageRect.dx, (REAL)pageRect.dy),
                         zoom, rotation);
    }
    void FixFontSizeForResolution(HDC hDC);
    DocTocItem *BuildTocTree(HtmlPullParser& parser, int& idCounter);

    Vec<DrawInstr> *GetPageData(int pageNo) {
        CrashIf(pageNo < 1 || PageCount() < pageNo);
        if (pageNo < 1 || PageCount() < pageNo)
            return NULL;
        return &pages->At(pageNo - 1)->instructions;
    }
};

class SimpleDest2 : public PageDestination {
    int pageNo;
    RectD rect;
    ScopedMem<TCHAR> value;

public:
    SimpleDest2(int pageNo, RectD rect, TCHAR *value=NULL) :
        pageNo(pageNo), rect(rect), value(value) { }

    virtual const char *GetDestType() const { return value ? "LaunchURL" : "ScrollTo"; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
    virtual TCHAR *GetDestValue() const { return value ? str::Dup(value) : NULL; }
};

class EpubLink : public PageElement, public PageDestination {
    EpubEngineImpl *engine;
    DrawInstr *link; // owned by EpubEngineImpl::pages
    RectI rect;
    int pageNo;
    bool isInternal;

    PageDestination *Resolve() const {
        CrashIf(!isInternal);
        const char *id = (const char *)memchr(link->str.s, '#', link->str.len);
        if (!id)
            id = link->str.s;
        ScopedMem<TCHAR> idt(str::conv::FromUtf8N(id, link->str.len - (id - link->str.s)));
        return engine->GetNamedDest(idt);
    }

public:
    EpubLink() : engine(NULL), link(NULL), pageNo(-1), isInternal(false) { }
    EpubLink(EpubEngineImpl *engine, DrawInstr *link, RectI rect, int pageNo=-1) :
        engine(engine), link(link), rect(rect), pageNo(pageNo) {
        // internal links don't start with a protocol
        isInternal = !memchr(link->str.s, ':', link->str.len);
    }

    virtual PageElementType GetType() const { return Element_Link; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect.Convert<double>(); }
    virtual TCHAR *GetValue() const {
        if (!isInternal)
            return str::conv::FromUtf8N(link->str.s, link->str.len);
        return NULL;
    }
    virtual PageDestination *AsLink() { return this; }

    virtual const char *GetDestType() const {
        if (!link)
            return NULL;
        if (isInternal)
            return "ScrollTo";
        return "LaunchURL";
    }
    virtual int GetDestPageNo() const {
        if (isInternal) {
            PageDestination *dest = Resolve();
            if (dest) {
                int pageNo = dest->GetDestPageNo();
                delete dest;
                return pageNo;
            }
        }
        return 0;
    }
    virtual RectD GetDestRect() const {
        if (isInternal) {
            PageDestination *dest = Resolve();
            if (dest) {
                RectD rect = dest->GetDestRect();
                delete dest;
                return rect;
            }
        }
        return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT,
                     DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    }
    virtual TCHAR *GetDestValue() const {
        if (isInternal)
            return NULL;
        return GetValue();
    }
};

class ImageDataElement : public PageElement {
    int pageNo;
    ImageData *id; // owned by EpubEngineImpl::pages
    RectI bbox;

public:
    ImageDataElement(int pageNo, ImageData *id, RectI bbox) :
        pageNo(pageNo), id(id), bbox(bbox) { }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return bbox.Convert<double>(); }
    virtual TCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        HBITMAP hbmp;
        Bitmap *bmp = BitmapFromData(id->data, id->len);
        if (!bmp || bmp->GetHBITMAP(Color::White, &hbmp) != Ok) {
            delete bmp;
            return NULL;
        }
        SizeI size(bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
        return new RenderedBitmap(hbmp, size);
    }
};

class EpubTocItem : public DocTocItem {
    PageDestination *dest;

public:
    EpubTocItem(TCHAR *title, PageDestination *dest) :
        DocTocItem(title, dest ? dest->GetDestPageNo() : 0), dest(dest) { }
    ~EpubTocItem() { delete dest; }

    virtual PageDestination *GetLink() { return dest; }
};

EpubEngineImpl::EpubEngineImpl() : fileName(NULL), doc(NULL), pages(NULL),
    pageRect(0, 0, 5.12 * GetFileDPI(), 7.8 * GetFileDPI()), // "B Format" paperback
    pageBorder(0.4f * GetFileDPI()), currFontDpi(96)
{
    InitializeCriticalSection(&iterAccess);
}

EpubEngineImpl::~EpubEngineImpl()
{
    EnterCriticalSection(&iterAccess);

    delete doc;
    if (pages)
        DeleteVecMembers(*pages);
    delete pages;
    free((void *)fileName);

    LeaveCriticalSection(&iterAccess);
    DeleteCriticalSection(&iterAccess);
}

bool EpubEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    doc = new EpubDoc(fileName);
    return FinishLoading();
}

bool EpubEngineImpl::Load(IStream *stream)
{
    doc = new EpubDoc(stream);
    return FinishLoading();
}

bool EpubEngineImpl::FinishLoading()
{
    if (!doc || !doc->Load())
        return false;

    LayoutInfo li;
    li.mobiDoc = (MobiDoc *)doc; // hack to allow passing doc through PageLayout
    li.htmlStr = doc->GetBookHtmlData(&li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = PageLayoutEpub(&li).Layout(&anchors);
    if (!pages)
        return false;

    // fix image justification
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
        if (!pageInstrs)
            return false;
        for (size_t k = 0; k < pageInstrs->Count(); k++) {
            DrawInstr *i = &pageInstrs->At(k);
            if (InstrImage != i->type)
                continue;
            CrashIf(k == 0 || k + 1 == pageInstrs->Count());
            DrawInstr *i2 = &pageInstrs->At(k+1);
            CrashIf(i2->type != InstrString || i2->str.len != 0 || i2->bbox.Width != i->bbox.Width);
            i->bbox.X = i2->bbox.X;
            // center images that are alone on a line
            DrawInstr *i3 = NULL, *i4 = NULL;
            for (size_t j = k; j > 0 && !i3; j--) {
                if (InstrString == pageInstrs->At(j).type)
                    i3 = &pageInstrs->At(j);
            }
            for (size_t j = k + 2; j < pageInstrs->Count() && !i4; j++) {
                if (InstrString == pageInstrs->At(j).type)
                    i4 = &pageInstrs->At(j);
            }
            if ((!i3 || i3->bbox.Y != i->bbox.Y) && (!i4 || i4->bbox.Y != i->bbox.Y))
                i2->bbox.X = i->bbox.X = ((REAL)pageRect.dx - 2 * pageBorder - i->bbox.Width) / 2;
        }
    }

    return true;
}

PointD EpubEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD EpubEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    PointF pts[2] = {
        PointF((REAL)rect.x, (REAL)rect.y),
        PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
    };
    Matrix m;
    GetTransform(m, zoom, rotation);
    if (inverse)
        m.Invert();
    m.TransformPoints(pts, 2);
    return RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
}

RenderedBitmap *EpubEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

void EpubEngineImpl::FixFontSizeForResolution(HDC hDC)
{
    int dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    if (dpi == currFontDpi)
        return;

    ScopedCritSec scope(&iterAccess);

    float dpiFactor = 1.0f * currFontDpi / dpi;
    Graphics g(hDC);
    LOGFONTW lfw;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
        for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
            if (InstrSetFont == i->type) {
                Status ok = i->font->GetLogFontW(&g, &lfw);
                if (Ok == ok) {
                    REAL newSize = i->font->GetSize() * dpiFactor;
                    FontStyle newStyle = (FontStyle)i->font->GetStyle();
                    i->font = mui::GetCachedFont(lfw.lfFaceName, newSize, newStyle);
                }
            }
        }
    }
    currFontDpi = dpi;
}

bool EpubEngineImpl::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    Color white(0xFF, 0xFF, 0xFF);
    Rect screenR(screenRect.x, screenRect.y, screenRect.dx, screenRect.dy);
    g.SetClip(screenR);
    screenR.Inflate(1, 1);
    g.FillRectangle(&SolidBrush(white), screenR);

    Matrix m;
    GetTransform(m, zoom, rotation);
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    ScopedCritSec scope(&iterAccess);
    FixFontSizeForResolution(hDC);
    DrawPageLayout(&g, GetPageData(pageNo), pageBorder, pageBorder, false);
    return true;
}

static RectI GetInstrBbox(DrawInstr *instr, float pageBorder)
{
    RectT<float> bbox(instr->bbox.X, instr->bbox.Y, instr->bbox.Width, instr->bbox.Height);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

TCHAR *EpubEngineImpl::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    ScopedCritSec scope(&iterAccess);

    str::Str<TCHAR> content;
    Vec<RectI> coords;
    bool insertSpace = false;

    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        RectI bbox = GetInstrBbox(i, pageBorder);
        switch (i->type) {
        case InstrString:
            if (coords.Count() > 0 && bbox.x <= coords.Last().BR().x) {
                content.Append(lineSep);
                coords.AppendBlanks(str::Len(lineSep));
                CrashIf(*lineSep && !coords.Last().IsEmpty());
            }
            else if (insertSpace && coords.Count() > 0) {
                int swidth = bbox.x - coords.Last().BR().x;
                if (swidth > 0) {
                    content.Append(' ');
                    coords.Append(RectI(bbox.x - swidth, bbox.y, swidth, bbox.dy));
                }
            }
            insertSpace = false;
            {
                ScopedMem<TCHAR> s(str::conv::FromUtf8N(i->str.s, i->str.len));
                content.Append(s);
                size_t len = str::Len(s);
                double cwidth = 1.0 * bbox.dx / len;
                for (size_t k = 0; k < len; k++)
                    coords.Append(RectI((int)(bbox.x + k * cwidth), bbox.y, (int)cwidth, bbox.dy));
            }
            break;
        case InstrElasticSpace:
        case InstrFixedSpace:
            insertSpace = true;
            break;
        }
    }

    if (coords_out) {
        CrashIf(coords.Count() != content.Count());
        *coords_out = new RectI[coords.Count()];
        memcpy(*coords_out, coords.LendData(), coords.Count() * sizeof(RectI));
    }
    return content.StealData();
}

static void AppendTocItem(EpubTocItem *& root, EpubTocItem *item, int level)
{
    if (!root) {
        root = item;
        return;
    }
    // find the last child at each level, until finding the parent of the new item
    DocTocItem *r2 = root;
    while (level-- > 0) {
        for (; r2->next; r2 = r2->next);
        if (r2->child)
            r2 = r2->child;
        else {
            r2->child = item;
            return;
        }
    }
    r2->AddSibling(item);
}

DocTocItem *EpubEngineImpl::BuildTocTree(HtmlPullParser& parser, int& idCounter)
{
    ScopedMem<TCHAR> itemText, itemSrc;
    EpubTocItem *root = NULL;
    int level = -1;

    HtmlToken *tok;
    while ((tok = parser.Next()) && (!tok->IsEndTag() || !tok->NameIs("navMap") && !tok->NameIs("ncx:navMap"))) {
        if (tok->IsTag() && (tok->NameIs("navPoint") || tok->NameIs("ncx:navPoint"))) {
            if (itemText) {
                PageDestination *dest = NULL;
                if (itemSrc && str::FindChar(itemSrc, ':'))
                    dest = new SimpleDest2(0, RectD(), itemSrc.StealData());
                else if (itemSrc && str::FindChar(itemSrc, '#'))
                    dest = GetNamedDest(str::FindChar(itemSrc, '#'));
                else if (itemSrc)
                    dest = GetNamedDest(itemSrc);
                itemSrc.Set(NULL);
                EpubTocItem *item = new EpubTocItem(itemText.StealData(), dest);
                item->id = ++idCounter;
                AppendTocItem(root, item, level);
            }
            if (tok->IsStartTag())
                level++;
            else if (tok->IsEndTag())
                level--;
        }
        else if (tok->IsStartTag() && (tok->NameIs("text") || tok->NameIs("ncx:text"))) {
            tok = parser.Next();
            if (tok->IsText())
                itemText.Set(str::conv::FromUtf8N(tok->s, tok->sLen));
        }
        else if (tok->IsTag() && !tok->IsEndTag() && (tok->NameIs("content") || tok->NameIs("ncx:content"))) {
            AttrInfo *attrInfo = tok->GetAttrByName("src");
            if (attrInfo)
                itemSrc.Set(str::conv::FromUtf8N(attrInfo->val, attrInfo->valLen));
        }
    }

    return root;
}

DocTocItem *EpubEngineImpl::GetTocTree()
{
    ScopedMem<char> tocXml(doc->GetToC());
    if (!tocXml)
        return NULL;

    HtmlPullParser parser(tocXml, str::Len(tocXml));
    HtmlToken *tok;
    // skip to the start of the navMap
    while ((tok = parser.Next())) {
        if (tok->IsStartTag() && (tok->NameIs("navMap") || tok->NameIs("ncx:navMap"))) {
            int idCounter = 0;
            return BuildTocTree(parser, idCounter);
        }
    }
    return NULL;
}

PageDestination *EpubEngineImpl::GetNamedDest(const TCHAR *name)
{
    if ('#' == *name)
        name++;
    ScopedMem<char> name_utf8(str::conv::ToUtf8(name));
    size_t name_len = str::Len(name_utf8);

    for (size_t i = 0; i < anchors.Count(); i++) {
        PageAnchor *anchor = &anchors.At(i);
        if (name_len == anchor->len && str::EqN(name_utf8, anchor->s, name_len)) {
            RectD rect(0, anchor->currY, pageRect.dx, 10 + pageBorder * 2);
            rect.Inflate(-pageBorder, -pageBorder);
            return new SimpleDest2(anchor->pageNo, rect);
        }
    }

    return NULL;
}

Vec<PageElement *> *EpubEngineImpl::GetElements(int pageNo)
{
    ScopedCritSec scope(&iterAccess);

    Vec<PageElement *> *els = new Vec<PageElement *>();

    DrawInstr *linkInstr = NULL;
    RectI linkRect;

    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        if (InstrImage == i->type)
            els->Append(new ImageDataElement(pageNo, &i->img, GetInstrBbox(i, pageBorder)));
        else if (InstrLinkStart == i->type) {
            linkInstr = i;
            linkRect = RectI();
        }
        else if (InstrLinkEnd == i->type && linkInstr) {
            if (!linkInstr)
                /* TODO: link started on a previous page */;
            else if (!linkRect.IsEmpty())
                els->Append(new EpubLink(this, linkInstr, linkRect, pageNo));
            linkInstr = NULL;
        }
        else if (InstrString == i->type && linkInstr) {
            RectI bbox = GetInstrBbox(i, pageBorder);
            // split multi-line links into multiple EpubLinks
            if (!linkRect.IsEmpty() && bbox.x <= linkRect.x) {
                els->Append(new EpubLink(this, linkInstr, linkRect, pageNo));
                linkRect = bbox;
            }
            else
                linkRect = linkRect.Union(bbox);
        }
    }

    return els;
}

PageElement *EpubEngineImpl::GetElementAtPos(int pageNo, PointD pt)
{
    Vec<PageElement *> *els = GetElements(pageNo);
    if (!els)
        return NULL;

    PageElement *el = NULL;
    for (size_t i = 0; i < els->Count() && !el; i++)
        if (els->At(i)->GetRect().Contains(pt))
            el = els->At(i);

    if (el)
        els->Remove(el);
    DeleteVecMembers(*els);
    delete els;

    return el;
}

bool EpubEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return EpubDoc::IsSupportedFile(fileName, sniff);
}

EpubEngine *EpubEngine::CreateFromFile(const TCHAR *fileName)
{
    EpubEngineImpl *engine = new EpubEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

EpubEngine *EpubEngine::CreateFromStream(IStream *stream)
{
    EpubEngineImpl *engine = new EpubEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}
