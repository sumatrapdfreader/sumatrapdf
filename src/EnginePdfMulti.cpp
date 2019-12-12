/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Log.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EnginePdf.h"

// represents .vbkm file
struct VBkm {};

Kind kindEnginePdfMulti = "enginePdfMulti";

class EnginePdfMultiImpl : public BaseEngine {
  public:
    EnginePdfMultiImpl();
    virtual ~EnginePdfMultiImpl();
    BaseEngine* Clone() override;

    int PageCount() const override;

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::tuple<char*, size_t> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving = false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    bool AllowsPrinting() const override;
    bool AllowsCopyingText() const override;

    float GetFileDPI() const override;

    bool BenchLoadPage(int pageNo) override;

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    DocTocTree* GetTocTree() override;

    bool HasPageLabels() const override;
    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    bool IsPasswordProtected() const override;
    char* GetDecryptionKey() const override;

    bool Load(const WCHAR* fileName, PasswordUI* pwdUI);

    static BaseEngine* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI);

  protected:
    int pageCount = -1;

    DocTocTree* tocTree = nullptr;
};

EnginePdfMultiImpl::EnginePdfMultiImpl() {
    kind = kindEnginePdfMulti;
    defaultFileExt = L".vbkm";
}

EnginePdfMultiImpl::~EnginePdfMultiImpl() {
}
BaseEngine* EnginePdfMultiImpl::Clone() {
    return nullptr;
}

int EnginePdfMultiImpl::PageCount() const {
    return pageCount;
}

RectD EnginePdfMultiImpl::PageMediabox(int pageNo) {
    return {};
}
RectD EnginePdfMultiImpl::PageContentBox(int pageNo, RenderTarget target) {
    return {};
}

RenderedBitmap* EnginePdfMultiImpl::RenderBitmap(int pageNo, float zoom, int rotation,
                                                 RectD* pageRect, /* if nullptr: defaults to the page's mediabox */
                                                 RenderTarget target, AbortCookie** cookie_out) {
    return nullptr;
}

PointD EnginePdfMultiImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    return {};
}
RectD EnginePdfMultiImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    return {};
}

std::tuple<char*, size_t> EnginePdfMultiImpl::GetFileData() {
    return {};
}

bool EnginePdfMultiImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    return false;
}

bool EnginePdfMultiImpl::SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots) {
    return false;
}

WCHAR* EnginePdfMultiImpl::ExtractPageText(int pageNo, RectI** coordsOut) {
    return nullptr;
}

bool EnginePdfMultiImpl::HasClipOptimizations(int pageNo) {
    return true;
}

WCHAR* EnginePdfMultiImpl::GetProperty(DocumentProperty prop) {
    return nullptr;
}

bool EnginePdfMultiImpl::SupportsAnnotation(bool forSaving) const {
    return false;
}

void EnginePdfMultiImpl::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
}

bool EnginePdfMultiImpl::AllowsPrinting() const {
    return false;
}
bool EnginePdfMultiImpl::AllowsCopyingText() const {
    return true;
}

float EnginePdfMultiImpl::GetFileDPI() const {
    return 96;
}

bool EnginePdfMultiImpl::BenchLoadPage(int pageNo) {
    return false;
}

Vec<PageElement*>* EnginePdfMultiImpl::GetElements(int pageNo) {
    return nullptr;
}

PageElement* EnginePdfMultiImpl::GetElementAtPos(int pageNo, PointD pt) {
    return nullptr;
}

PageDestination* EnginePdfMultiImpl::GetNamedDest(const WCHAR* name) {
    return nullptr;
}
DocTocTree* EnginePdfMultiImpl::GetTocTree() {
    return nullptr;
}

bool EnginePdfMultiImpl::HasPageLabels() const {
    return false;
}

WCHAR* EnginePdfMultiImpl::GetPageLabel(int pageNo) const {
    return nullptr;
}

int EnginePdfMultiImpl::GetPageByLabel(const WCHAR* label) const {
    return -1;
}

bool EnginePdfMultiImpl::IsPasswordProtected() const {
    return false;
}

char* EnginePdfMultiImpl::GetDecryptionKey() const {
    return nullptr;
}

bool EnginePdfMultiImpl::Load(const WCHAR* fileName, PasswordUI* pwdUI) {
    return false;
}

BaseEngine* EnginePdfMultiImpl::CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    if (str::IsEmpty(fileName)) {
        return nullptr;
    }
    EnginePdfMultiImpl* engine = new EnginePdfMultiImpl();
    if (!engine->Load(fileName, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEnginePdfMultiSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        // we don't support sniffing
        return false;
    }
    return str::EndsWithI(fileName, L".vbkm");
}

BaseEngine* CreateEnginePdfMultiFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    return EnginePdfMultiImpl::CreateFromFile(fileName, pwdUI);
}
