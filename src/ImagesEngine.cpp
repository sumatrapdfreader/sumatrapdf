/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ImagesEngine.h"

#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlPullParser.h"
#include "JsonParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"

#include "../ext/unrar/dll.hpp"

// disable warning C4250 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
#pragma warning(disable: 4250) /* 'class1' : inherits 'class2::member' via dominance */

///// ImagesEngine methods apply to all types of engines handling full-page images /////

class ImagesEngine : public virtual BaseEngine {
public:
    ImagesEngine() : fileName(NULL), fileExt(NULL) { }
    virtual ~ImagesEngine() {
        DeleteVecMembers(pages);
        free(fileName);
    }

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return (int)pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return RectD(0, 0, pages.At(pageNo - 1)->GetWidth(), pages.At(pageNo - 1)->GetHeight());
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool SaveFileAs(const WCHAR *copyFileName);
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_NonContinuous; }
    virtual bool IsImageCollection() const { return true; }

    virtual bool SupportsAnnotation(bool forSaving=false) const { return false; }
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list) { }

    virtual float GetFileDPI() const {
        // TODO: support files with DPIs differing between pages
        //       (or premultiply DPI same as for DjVu documents)
        return pages.At(0)->GetHorizontalResolution();
    }
    virtual const WCHAR *GetDefaultFileExt() const { return fileExt; }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual bool BenchLoadPage(int pageNo) { return LoadImage(pageNo) != NULL; }

protected:
    WCHAR *fileName;
    const WCHAR *fileExt;
    ScopedComPtr<IStream> fileStream;

    Vec<Bitmap *> pages;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    // override for lazily loading images
    virtual Bitmap *LoadImage(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return pages.At(pageNo - 1);
    }
};

RenderedBitmap *ImagesEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target, cookie_out);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

bool ImagesEngine::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    Bitmap *bmp = LoadImage(pageNo);
    if (!bmp)
        return false;

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color white(0xFF, 0xFF, 0xFF);
    Rect screenR(screenRect.ToGdipRect());
    g.SetClip(screenR);
    SolidBrush tmpBrush(white);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    RectI pageRcI = PageMediabox(pageNo).Round();
    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(WrapModeTileFlipXY);
    Status ok = g.DrawImage(bmp, pageRcI.ToGdipRect(), 0, 0, pageRcI.dx, pageRcI.dy, UnitPixel, &imgAttrs);
    return ok == Ok;
}

void ImagesEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotation)
{
    GetBaseTransform(m, PageMediabox(pageNo).ToGdipRectF(), zoom, rotation);
}

PointD ImagesEngine::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD ImagesEngine::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    PointF pts[2] = {
        PointF((REAL)rect.x, (REAL)rect.y),
        PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
    };
    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    if (inverse)
        m.Invert();
    m.TransformPoints(pts, 2);
    rect = RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
    // try to undo rounding errors caused by a rotation
    // (necessary correction determined by experimentation)
    if (rotation != 0)
        rect.Inflate(-0.01, -0.01);
    return rect;
}

class ImageElement : public PageElement {
    Bitmap *bmp;
    int pageNo;

public:
    ImageElement(int pageNo, Bitmap *bmp) : pageNo(pageNo), bmp(bmp) { }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return RectD(0, 0, bmp->GetWidth(), bmp->GetHeight()); }
    virtual WCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        HBITMAP hbmp;
        if (bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok)
            return NULL;
        return new RenderedBitmap(hbmp, SizeI(bmp->GetWidth(), bmp->GetHeight()));
    }
};

Vec<PageElement *> *ImagesEngine::GetElements(int pageNo)
{
    Bitmap *bmp = LoadImage(pageNo);
    if (!bmp)
        return NULL;

    Vec<PageElement *> *els = new Vec<PageElement *>();
    els->Append(new ImageElement(pageNo, bmp));
    return els;
}

PageElement *ImagesEngine::GetElementAtPos(int pageNo, PointD pt)
{
    if (!PageMediabox(pageNo).Contains(pt))
        return NULL;
    Bitmap *bmp = LoadImage(pageNo);
    if (!bmp)
        return NULL;
    return new ImageElement(pageNo, bmp);
}

unsigned char *ImagesEngine::GetFileData(size_t *cbCount)
{
    if (fileStream) {
        void *data = GetDataFromStream(fileStream, cbCount);
        if (data)
            return (unsigned char *)data;
    }
    if (fileName)
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    return NULL;
}

bool ImagesEngine::SaveFileAs(const WCHAR *copyFileName)
{
    if (fileName) {
        BOOL ok = CopyFile(fileName, copyFileName, FALSE);
        if (ok)
            return true;
    }
    size_t dataLen;
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data)
        return file::WriteAll(copyFileName, data.Get(), dataLen);
    return false;
}

///// ImageEngine handles a single image file /////

class ImageEngineImpl : public ImagesEngine, public ImageEngine {
    friend ImageEngine;

public:
    virtual ImageEngine *Clone();

    virtual WCHAR *GetProperty(DocumentProperty prop);

protected:
    bool LoadSingleFile(const WCHAR *fileName);
    bool LoadFromStream(IStream *stream);
    bool FinishLoading(Bitmap *bmp);
};

ImageEngine *ImageEngineImpl::Clone()
{
    Bitmap *bmp = pages.At(0);
    bmp = bmp->Clone(0, 0, bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB);
    if (!bmp)
        return NULL;

    ImageEngineImpl *clone = new ImageEngineImpl();
    clone->fileName = str::Dup(fileName);
    clone->fileExt = fileExt;
    if (fileStream)
        fileStream->Clone(&clone->fileStream);
    clone->FinishLoading(bmp);

    return clone;
}

bool ImageEngineImpl::LoadSingleFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);

    size_t len;
    ScopedMem<char> data(file::ReadAll(file, &len));
    fileExt = GfxFileExtFromData(data, len);

    Bitmap *bmp = BitmapFromData(data, len);
    return FinishLoading(bmp);
}

bool ImageEngineImpl::LoadFromStream(IStream *stream)
{
    if (!stream)
        return false;
    fileStream = stream;
    fileStream->AddRef();

    char header[18];
    if (ReadDataFromStream(stream, header, sizeof(header)))
        fileExt = GfxFileExtFromData(header, sizeof(header));

    Bitmap *bmp;
    if (fileExt && !IsGdiPlusNativeFormat(header, sizeof(header))) {
        size_t len;
        ScopedMem<char> data((char *)GetDataFromStream(stream, &len));
        bmp = BitmapFromData(data, len);
    }
    else
        bmp = Bitmap::FromStream(stream);

    return FinishLoading(bmp);
}

bool ImageEngineImpl::FinishLoading(Bitmap *bmp)
{
    if (!bmp || bmp->GetLastStatus() != Ok)
        return false;
    pages.Append(bmp);
    assert(pages.Count() == 1);

    if (str::Eq(fileExt, L".tif")) {
        // extract all frames from multi-page TIFFs
        UINT frames = bmp->GetFrameCount(&FrameDimensionPage);
        for (UINT i = 1; i < frames; i++) {
            Bitmap *frame = bmp->Clone(0, 0, bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB);
            if (!frame)
                continue;
            Status ok = frame->SelectActiveFrame(&FrameDimensionPage, i);
            if (Ok == ok)
                pages.Append(frame);
            else
                delete frame;
        }
    }

    assert(fileExt);
    return fileExt != NULL;
}

// cf. http://www.universalthread.com/ViewPageArticle.aspx?ID=831
#ifndef PropertyTagXPTitle
#define PropertyTagXPTitle      0x9c9b
#define PropertyTagXPComment    0x9c9c
#define PropertyTagXPAuthor     0x9c9d
#define PropertyTagXPKeywords   0x9c9e
#define PropertyTagXPSubject    0x9c9f
#endif

static WCHAR *GetImageProperty(Bitmap *bmp, PROPID id, PROPID altId=0)
{
    WCHAR *value = NULL;
    UINT size = bmp->GetPropertyItemSize(id);
    PropertyItem *item = (PropertyItem *)malloc(size);
    Status ok = item ? bmp->GetPropertyItem(id, size, item) : OutOfMemory;
    if (Ok != ok)
        /* property didn't exist */;
    else if (PropertyTagTypeASCII == item->type)
        value = str::conv::FromAnsi((char *)item->value);
    else if (PropertyTagTypeByte == item->type && item->length > 0 &&
        0 == (item->length % 2) && !((WCHAR *)item->value)[item->length / 2 - 1]) {
        value = str::Dup((WCHAR *)item->value);
    }
    free(item);
    if (!value && altId)
        return GetImageProperty(bmp, altId);
    return value;
}

WCHAR *ImageEngineImpl::GetProperty(DocumentProperty prop)
{
    switch (prop) {
    case Prop_Title:
        return GetImageProperty(LoadImage(1), PropertyTagImageDescription, PropertyTagXPTitle);
    case Prop_Subject:
        return GetImageProperty(LoadImage(1), PropertyTagXPSubject);
    case Prop_Author:
        return GetImageProperty(LoadImage(1), PropertyTagArtist, PropertyTagXPAuthor);
    case Prop_Copyright:
        return GetImageProperty(LoadImage(1), PropertyTagCopyright);
    case Prop_CreationDate:
        return GetImageProperty(LoadImage(1), PropertyTagDateTime, PropertyTagExifDTDigitized);
    case Prop_CreatorApp:
        return GetImageProperty(LoadImage(1), PropertyTagSoftwareUsed);
    default:
        return NULL;
    }
}

bool ImageEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[9] = { 0 };
        file::ReadAll(fileName, header, sizeof(header));
        fileName = GfxFileExtFromData(header, sizeof(header));
    }

    return str::EndsWithI(fileName, L".png") ||
           str::EndsWithI(fileName, L".jpg") || str::EndsWithI(fileName, L".jpeg") ||
           str::EndsWithI(fileName, L".gif") ||
           str::EndsWithI(fileName, L".tif") || str::EndsWithI(fileName, L".tiff") ||
           str::EndsWithI(fileName, L".bmp") ||
           str::EndsWithI(fileName, L".tga") ||
           str::EndsWithI(fileName, L".jxr") || str::EndsWithI(fileName, L".hdp") ||
                                                str::EndsWithI(fileName, L".wdp");
}

ImageEngine *ImageEngine::CreateFromFile(const WCHAR *fileName)
{
    assert(IsSupportedFile(fileName) || IsSupportedFile(fileName, true));
    ImageEngineImpl *engine = new ImageEngineImpl();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

ImageEngine *ImageEngine::CreateFromStream(IStream *stream)
{
    ImageEngineImpl *engine = new ImageEngineImpl();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// ImageDirEngine handles a directory full of image files /////

class ImageDirEngineImpl : public ImagesEngine, public ImageDirEngine {
    friend ImageDirEngine;

public:
    virtual ImageDirEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }
    virtual RectD PageMediabox(int pageNo);

    virtual unsigned char *GetFileData(size_t *cbCount) { return NULL; }
    virtual bool SaveFileAs(const WCHAR *copyFileName);

    virtual WCHAR *GetProperty(DocumentProperty prop) { return NULL; }

    // TODO: is there a better place to expose pageFileNames than through page labels?
    virtual bool HasPageLabels() const { return true; }
    virtual WCHAR *GetPageLabel(int pageNo) const;
    virtual int GetPageByLabel(const WCHAR *label) const;

    virtual bool HasTocTree() const { return true; }
    virtual DocTocItem *GetTocTree();

protected:
    bool LoadImageDir(const WCHAR *dirName);

    virtual Bitmap *LoadImage(int pageNo);

    Vec<RectD> mediaboxes;
    WStrVec pageFileNames;
};

bool ImageDirEngineImpl::LoadImageDir(const WCHAR *dirName)
{
    fileName = str::Dup(dirName);
    fileExt = L"";

    ScopedMem<WCHAR> pattern(path::Join(dirName, L"*"));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (ImageEngine::IsSupportedFile(fdata.cFileName))
                pageFileNames.Append(path::Join(dirName, fdata.cFileName));
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    if (pageFileNames.Count() == 0)
        return false;
    pageFileNames.SortNatural();

    pages.AppendBlanks(pageFileNames.Count());
    mediaboxes.AppendBlanks(pageFileNames.Count());

    return true;
}

RectD ImageDirEngineImpl::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!mediaboxes.At(pageNo - 1).IsEmpty())
        return mediaboxes.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData) {
        Size size = BitmapSizeFromData(bmpData, len);
        mediaboxes.At(pageNo - 1) = RectD(0, 0, size.Width, size.Height);
    }
    return mediaboxes.At(pageNo - 1);
}

WCHAR *ImageDirEngineImpl::GetPageLabel(int pageNo) const
{
    if (pageNo < 1 || PageCount() < pageNo)
        return BaseEngine::GetPageLabel(pageNo);

    const WCHAR *fileName = path::GetBaseName(pageFileNames.At(pageNo - 1));
    return str::DupN(fileName, path::GetExt(fileName) - fileName);
}

int ImageDirEngineImpl::GetPageByLabel(const WCHAR *label) const
{
    for (size_t i = 0; i < pageFileNames.Count(); i++) {
        const WCHAR *fileName = path::GetBaseName(pageFileNames.At(i));
        const WCHAR *fileExt = path::GetExt(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0'))
            return (int)i + 1;
    }

    return BaseEngine::GetPageByLabel(label);
}

Bitmap *ImageDirEngineImpl::LoadImage(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (pages.At(pageNo - 1))
        return pages.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData)
        pages.At(pageNo - 1) = BitmapFromData(bmpData, len);

    return pages.At(pageNo - 1);
}

class ImageDirTocItem : public DocTocItem {
public:
    ImageDirTocItem(WCHAR *title, int pageNo) : DocTocItem(title, pageNo) { }

    virtual PageDestination *GetLink() { return NULL; }
};

DocTocItem *ImageDirEngineImpl::GetTocTree()
{
    DocTocItem *root = new ImageDirTocItem(GetPageLabel(1), 1);
    root->id = 1;
    for (int i = 2; i <= PageCount(); i++) {
        DocTocItem *item = new ImageDirTocItem(GetPageLabel(i), i);
        item->id = i;
        root->AddSibling(item);
    }
    return root;
}

bool ImageDirEngineImpl::SaveFileAs(const WCHAR *copyFileName)
{
    // only copy the files if the target directory doesn't exist yet
    if (!CreateDirectory(copyFileName, NULL))
        return false;
    bool ok = true;
    for (size_t i = 0; i < pageFileNames.Count(); i++) {
        const WCHAR *filePathOld = pageFileNames.At(i);
        ScopedMem<WCHAR> filePathNew(path::Join(copyFileName, path::GetBaseName(filePathOld)));
        ok = ok && CopyFile(filePathOld, filePathNew, TRUE);
    }
    return ok;
}

bool ImageDirEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

ImageDirEngine *ImageDirEngine::CreateFromFile(const WCHAR *fileName)
{
    assert(dir::Exists(fileName));
    ImageDirEngineImpl *engine = new ImageDirEngineImpl();
    if (!engine->LoadImageDir(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// CbxEngine handles comic book files (either .cbz or .cbr) /////

class CbxEngineImpl : public ImagesEngine, public CbxEngine, public json::ValueVisitor {
    friend CbxEngine;

public:
    CbxEngineImpl() : cbzFile(NULL) {
        InitializeCriticalSection(&fileAccess);
    }
    virtual ~CbxEngineImpl();

    virtual CbxEngine *Clone() {
        if (fileStream) {
            ScopedComPtr<IStream> stm;
            HRESULT res = fileStream->Clone(&stm);
            if (SUCCEEDED(res))
                return CreateFromStream(stm);
        }
        if (fileName)
            return CreateFromFile(fileName);
        return NULL;
    }
    virtual RectD PageMediabox(int pageNo);

    virtual WCHAR *GetProperty(DocumentProperty prop);

    // json::ValueVisitor
    virtual bool Visit(const char *path, const char *value, json::DataType type);

protected:
    bool LoadCbzFile(const WCHAR *fileName);
    bool LoadCbzStream(IStream *stream);
    bool FinishLoadingCbz();
    void ParseComicInfoXml(const char *xmlData);
    bool LoadCbrFile(const WCHAR *fileName);

    virtual Bitmap *LoadImage(int pageNo);
    char *GetImageData(int pageNo, size_t& len);

    Vec<RectD> mediaboxes;

    // extracted metadata
    ScopedMem<WCHAR> propTitle;
    WStrVec propAuthors;
    ScopedMem<WCHAR> propDate;
    ScopedMem<WCHAR> propModDate;
    ScopedMem<WCHAR> propCreator;
    ScopedMem<WCHAR> propSummary;
    // temporary state needed for extracting metadata
    ScopedMem<WCHAR> propAuthorTmp;

    // used for lazily loading page images (only supported for .cbz files)
    CRITICAL_SECTION fileAccess;
    ZipFile *cbzFile;
    Vec<size_t> fileIdxs;
};

CbxEngineImpl::~CbxEngineImpl()
{
    delete cbzFile;

    DeleteCriticalSection(&fileAccess);
}

RectD CbxEngineImpl::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!mediaboxes.At(pageNo - 1).IsEmpty())
        return mediaboxes.At(pageNo - 1);

    if (pages.At(pageNo - 1)) {
        Bitmap *bmp = pages.At(pageNo - 1);
        mediaboxes.At(pageNo - 1) = RectD(0, 0, bmp->GetWidth(), bmp->GetHeight());
        return mediaboxes.At(pageNo - 1);
    }

    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData) {
        Size size = BitmapSizeFromData(bmpData, len);
        mediaboxes.At(pageNo - 1) = RectD(0, 0, size.Width, size.Height);
    }
    return mediaboxes.At(pageNo - 1);
}

Bitmap *CbxEngineImpl::LoadImage(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (pages.At(pageNo - 1))
        return pages.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData)
        pages.At(pageNo - 1) = BitmapFromData(bmpData, len);

    return pages.At(pageNo - 1);
}

bool CbxEngineImpl::LoadCbzFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);

    cbzFile = new ZipFile(fileName);

    return FinishLoadingCbz();
}

bool CbxEngineImpl::LoadCbzStream(IStream *stream)
{
    if (!stream)
        return false;
    fileStream = stream;
    fileStream->AddRef();

    cbzFile = new ZipFile(stream);

    return FinishLoadingCbz();
}

static int cmpAscii(const void *a, const void *b)
{
    return wcscmp(*(const WCHAR **)a, *(const WCHAR **)b);
}

bool CbxEngineImpl::FinishLoadingCbz()
{
    fileExt = L".cbz";

    Vec<const WCHAR *> allFileNames;

    for (size_t idx = 0; idx < cbzFile->GetFileCount(); idx++) {
        const WCHAR *fileName = cbzFile->GetFileName(idx);
        // bail, if we accidentally try to load an XPS file
        if (fileName && str::StartsWith(fileName, L"_rels/.rels"))
            return false;
        if (fileName && ImageEngine::IsSupportedFile(fileName) &&
            // OS X occasionally leaves metadata with image extensions
            !str::StartsWith(path::GetBaseName(fileName), L".")) {
            allFileNames.Append(fileName);
        }
        else {
            allFileNames.Append(NULL);
        }
    }
    assert(allFileNames.Count() == cbzFile->GetFileCount());

    ScopedMem<char> metadata(cbzFile->GetFileDataByName(L"ComicInfo.xml"));
    if (metadata)
        ParseComicInfoXml(metadata);
    metadata.Set(cbzFile->GetComment());
    if (metadata)
        json::Parse(metadata, this);

    Vec<const WCHAR *> pageFileNames;
    for (const WCHAR **fn = allFileNames.IterStart(); fn; fn = allFileNames.IterNext()) {
        if (*fn)
            pageFileNames.Append(*fn);
    }
    pageFileNames.Sort(cmpAscii);
    for (const WCHAR **fn = pageFileNames.IterStart(); fn; fn = pageFileNames.IterNext()) {
        fileIdxs.Append(allFileNames.Find(*fn));
    }
    assert(pageFileNames.Count() == fileIdxs.Count());
    if (fileIdxs.Count() == 0)
        return false;

    pages.AppendBlanks(fileIdxs.Count());
    mediaboxes.AppendBlanks(fileIdxs.Count());

    return true;
}

static char *GetTextContent(HtmlPullParser& parser)
{
    HtmlToken *tok = parser.Next();
    if (!tok || !tok->IsText())
        return NULL;
    return ResolveHtmlEntities(tok->s, tok->sLen);
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void CbxEngineImpl::ParseComicInfoXml(const char *xmlData)
{
    HtmlPullParser parser(xmlData, str::Len(xmlData));
    HtmlToken *tok;
    while ((tok = parser.Next()) != NULL && !tok->IsError()) {
        if (!tok->IsStartTag())
            continue;
        if (tok->NameIs("Title")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/title", value, json::Type_String);
        }
        else if (tok->NameIs("Year")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationYear", value, json::Type_Number);
        }
        else if (tok->NameIs("Month")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationMonth", value, json::Type_Number);
        }
        else if (tok->NameIs("Summary")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/X-summary", value, json::Type_String);
        }
        else if (tok->NameIs("Writer")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[0]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type_Bool);
            }
        }
        else if (tok->NameIs("Penciller")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[1]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type_Bool);
            }
        }
    }
}

// extract ComicBookInfo metadata
// cf. http://code.google.com/p/comicbookinfo/
bool CbxEngineImpl::Visit(const char *path, const char *value, json::DataType type)
{
    if (json::Type_String == type && str::Eq(path, "/ComicBookInfo/1.0/title"))
        propTitle.Set(str::conv::FromUtf8(value));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear"))
        propDate.Set(str::Format(L"%s/%d", propDate ? propDate : L"", atoi(value)));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth"))
        propDate.Set(str::Format(L"%d%s", atoi(value), propDate ? propDate : L""));
    else if (json::Type_String == type && str::Eq(path, "/appID"))
        propCreator.Set(str::conv::FromUtf8(value));
    else if (json::Type_String == type && str::Eq(path, "/lastModified"))
        propModDate.Set(str::conv::FromUtf8(value));
    else if (json::Type_String == type && str::Eq(path, "/X-summary"))
        propSummary.Set(str::conv::FromUtf8(value));
    else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        const char *prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type_String == type && str::Eq(prop, "person"))
                propAuthorTmp.Set(str::conv::FromUtf8(value));
            else if (json::Type_Bool == type && str::Eq(prop, "primary") &&
                propAuthorTmp && !propAuthors.Contains(propAuthorTmp)) {
                propAuthors.Append(propAuthorTmp.StealData());
            }
        }
        return true;
    }
    // stop parsing once we have all desired information
    return !propTitle || propAuthors.Count() == 0 || !propCreator ||
           !propDate || str::FindChar(propDate, '/') <= propDate;
}

WCHAR *CbxEngineImpl::GetProperty(DocumentProperty prop)
{
    switch (prop) {
    case Prop_Title:
        return str::Dup(propTitle);
    case Prop_Author:
        return propAuthors.Count() ? propAuthors.Join(L", ") : NULL;
    case Prop_CreationDate:
        return str::Dup(propDate);
    case Prop_ModificationDate:
        return str::Dup(propModDate);
    case Prop_CreatorApp:
        return str::Dup(propCreator);
    // TODO: replace with Prop_Summary
    case Prop_Subject:
        return str::Dup(propSummary);
    default:
        return NULL;
    }
}

class ImagesPage {
public:
    ScopedMem<WCHAR>fileName; // for sorting image files
    Bitmap *        bmp;

    ImagesPage(const WCHAR *fileName, Bitmap *bmp) : bmp(bmp),
        fileName(str::Dup(fileName)) { }
    ~ImagesPage() { delete bmp; }

    static int cmpPageByName(const void *o1, const void *o2) {
        ImagesPage *p1 = *(ImagesPage **)o1;
        ImagesPage *p2 = *(ImagesPage **)o2;
        return wcscmp(p1->fileName, p2->fileName);
    }
};

struct RarDecompressData {
    unsigned    totalSize;
    char *      buf;
    unsigned    currSize;
};

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed)
{
    if (UCM_PROCESSDATA != msg)
        return -1;

    if (!userData)
        return -1;
    RarDecompressData *rrd = (RarDecompressData*)userData;

    if (rrd->currSize + bytesProcessed > rrd->totalSize)
        return -1;

    char *buf = rrd->buf + rrd->currSize;
    memcpy(buf, (char *)rarBuffer, bytesProcessed);
    rrd->currSize += (unsigned int)bytesProcessed;
    return 1;
}

static char *LoadCurrentCbrFile(HANDLE hArc, RARHeaderDataEx& rarHeader, size_t *lenOut)
{
    ScopedMem<char> data((char *)malloc(rarHeader.UnpSize + sizeof(WCHAR)));

    if (rarHeader.UnpSizeHigh != 0 || rarHeader.UnpSize == 0 ||
        rarHeader.UnpSize + sizeof(WCHAR) < sizeof(WCHAR) || !data) {
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
        return NULL;
    }

    RarDecompressData rdd;
    rdd.totalSize = rarHeader.UnpSize;
    rdd.buf = data;
    rdd.currSize = 0;
    RARSetCallback(hArc, unrarCallback, (LPARAM)&rdd);
    int res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
    if (0 != res || rdd.totalSize != rdd.currSize)
        return NULL;
    // zero-terminate for convenience
    data[rdd.totalSize] = data[rdd.totalSize + 1] = '\0';

    if (lenOut)
        *lenOut = rdd.totalSize;
    return data.StealData();
}

static ImagesPage *LoadCurrentCbrPage(HANDLE hArc, RARHeaderDataEx& rarHeader)
{
    size_t bmpDataSize;
    ScopedMem<char> bmpData(LoadCurrentCbrFile(hArc, rarHeader, &bmpDataSize));
    if (!bmpData)
        return NULL;

    Bitmap *bmp = BitmapFromData(bmpData, bmpDataSize);
    if (!bmp)
        return NULL;

    return new ImagesPage(rarHeader.FileNameW, bmp);
}

bool CbxEngineImpl::LoadCbrFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);
    fileExt = L".cbr";

    RAROpenArchiveDataEx  arcData = { 0 };
    arcData.ArcNameW = (WCHAR *)file;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return false;

    // UnRAR does not seem to support extracting a single file by name,
    // so lazy image loading doesn't seem possible

    Vec<ImagesPage *> found;
    for (;;) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;

        const WCHAR *fileName = rarHeader.FileNameW;
        if (ImageEngine::IsSupportedFile(fileName)) {
            ImagesPage *page = LoadCurrentCbrPage(hArc, rarHeader);
            if (page)
                found.Append(page);
        }
        else if (str::EqI(fileName, L"ComicInfo.xml")) {
            ScopedMem<char> xmlData(LoadCurrentCbrFile(hArc, rarHeader, NULL));
            if (xmlData)
                ParseComicInfoXml(xmlData);
        }
        else
            RARProcessFile(hArc, RAR_SKIP, NULL, NULL);

    }
    RARCloseArchive(hArc);

    if (found.Count() == 0)
        return false;
    found.Sort(ImagesPage::cmpPageByName);

    for (size_t i = 0; i < found.Count(); i++) {
        pages.Append(found.At(i)->bmp);
        found.At(i)->bmp = NULL;
    }
    mediaboxes.AppendBlanks(pages.Count());

    DeleteVecMembers(found);
    return true;
}

char *CbxEngineImpl::GetImageData(int pageNo, size_t& len)
{
    if (cbzFile) {
        ScopedCritSec scope(&fileAccess);
        return cbzFile->GetFileDataByIdx(fileIdxs.At(pageNo - 1), &len);
    }
    return NULL;
}

bool CbxEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        // we don't also sniff for ZIP files, as these could also
        // be broken XPS files for which failure is expected
        return file::StartsWith(fileName, "Rar!\x1A\x07\x00", 7);
    }

    return str::EndsWithI(fileName, L".cbz") ||
           str::EndsWithI(fileName, L".cbr") ||
           str::EndsWithI(fileName, L".zip") && !str::EndsWithI(fileName, L".fb2.zip") ||
           str::EndsWithI(fileName, L".rar");
}

CbxEngine *CbxEngine::CreateFromFile(const WCHAR *fileName)
{
    assert(IsSupportedFile(fileName) || IsSupportedFile(fileName, true));
    CbxEngineImpl *engine = new CbxEngineImpl();
    bool ok = false;
    if (str::EndsWithI(fileName, L".cbz") || str::EndsWithI(fileName, L".zip") ||
        file::StartsWith(fileName, "PK\x03\x04")) {
        ok = engine->LoadCbzFile(fileName);
    }
    if (!ok) {
        // also try again if a .cbz or .zip file failed to load, it might
        // just have been misnamed (which apparently happens occasionally)
        delete engine;
        engine = new CbxEngineImpl();
        if (str::EndsWithI(fileName, L".cbr") || str::EndsWithI(fileName, L".rar") ||
            file::StartsWith(fileName, "Rar!\x1A\x07\x00", 7)) {
            ok = engine->LoadCbrFile(fileName);
        }
    }
    if (!ok) {
        delete engine;
        return NULL;
    }
    return engine;
}

CbxEngine *CbxEngine::CreateFromStream(IStream *stream)
{
    CbxEngineImpl *engine = new CbxEngineImpl();
    // TODO: UnRAR doesn't support reading from arbitrary data streams
    if (!engine->LoadCbzStream(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}
