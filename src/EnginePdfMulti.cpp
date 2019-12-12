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
    PageLayoutType PreferredLayout() override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving = false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    bool AllowsPrinting() const override;
    bool AllowsCopyingText() const override;

    float GetFileDPI() const override;
    const WCHAR* GetDefaultFileExt() const override;

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

    static BaseEngine* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI);
    static BaseEngine* CreateFromStream(IStream* stream, PasswordUI* pwdUI);

  protected:
    int pageCount = -1;

    DocTocTree* tocTree = nullptr;
};

bool IsEnginePdfMultiSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!sniff) {
        return str::EndsWithI(fileName, L".vbkm");
    }
    // we don't support sniffing
    return false;
}

BaseEngine* CreateEnginePdfMultiFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    return nullptr;
}

BaseEngine* CreateEnginePdfMultiFromStream(IStream* stream, PasswordUI* pwdUI) {
    return nullptr;
}
