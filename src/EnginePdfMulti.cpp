/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
#include "EngineManager.h"
#include "ParseBKM.h"
#include "EnginePdfMulti.h"

struct EnginePage {
    int pageNoInEngine = 0;
    EngineBase* engine = nullptr;
};

Kind kindEnginePdfMulti = "enginePdfMulti";

class EnginePdfMultiImpl : public EngineBase {
  public:
    EnginePdfMultiImpl();
    virtual ~EnginePdfMultiImpl();
    EngineBase* Clone() override;

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;
    RenderedBitmap* GetImageForPageElement(PageElement*) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    bool Load(const WCHAR* fileName, PasswordUI* pwdUI);

    static EngineBase* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI);

    EngineBase* PageToEngine(int& pageNo) const;
    VbkmFile vbkm;
    Vec<EnginePage> pageToEngine;

    TocTree* tocTree = nullptr;
};

EngineBase* EnginePdfMultiImpl::PageToEngine(int& pageNo) const {
    EnginePage& ep = pageToEngine[pageNo - 1];
    pageNo = ep.pageNoInEngine;
    return ep.engine;
}

EnginePdfMultiImpl::EnginePdfMultiImpl() {
    kind = kindEnginePdfMulti;
    defaultFileExt = L".vbkm";
    fileDPI = 72.0f;
    supportsAnnotations = false;
    supportsAnnotationsForSaving = false;
}

EnginePdfMultiImpl::~EnginePdfMultiImpl() {
    delete tocTree;
}

EngineBase* EnginePdfMultiImpl::Clone() {
    CrashIf(true);
    return nullptr;
}

RectD EnginePdfMultiImpl::PageMediabox(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->PageMediabox(pageNo);
}

RectD EnginePdfMultiImpl::PageContentBox(int pageNo, RenderTarget target) {
    EngineBase* e = PageToEngine(pageNo);
    return e->PageContentBox(pageNo, target);
}

RenderedBitmap* EnginePdfMultiImpl::RenderPage(RenderPageArgs& args) {
    EngineBase* e = PageToEngine(args.pageNo);
    return e->RenderPage(args);
}

RectD EnginePdfMultiImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    EngineBase* e = PageToEngine(pageNo);
    return e->Transform(rect, pageNo, zoom, rotation, inverse);
}

std::string_view EnginePdfMultiImpl::GetFileData() {
    return {};
}

bool EnginePdfMultiImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    return false;
}

bool EnginePdfMultiImpl::SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots) {
    return false;
}

WCHAR* EnginePdfMultiImpl::ExtractPageText(int pageNo, RectI** coordsOut) {
    EngineBase* e = PageToEngine(pageNo);
    return e->ExtractPageText(pageNo, coordsOut);
}

bool EnginePdfMultiImpl::HasClipOptimizations(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->HasClipOptimizations(pageNo);
}

WCHAR* EnginePdfMultiImpl::GetProperty(DocumentProperty prop) {
    return nullptr;
}

void EnginePdfMultiImpl::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    // TODO: support user annotations
}

bool EnginePdfMultiImpl::BenchLoadPage(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->BenchLoadPage(pageNo);
}

Vec<PageElement*>* EnginePdfMultiImpl::GetElements(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->GetElements(pageNo);
}

PageElement* EnginePdfMultiImpl::GetElementAtPos(int pageNo, PointD pt) {
    EngineBase* e = PageToEngine(pageNo);
    return e->GetElementAtPos(pageNo, pt);
}

RenderedBitmap* EnginePdfMultiImpl::GetImageForPageElement(PageElement* pel) {
    EngineBase* e = PageToEngine(pel->pageNo);
    return e->GetImageForPageElement(pel);
}

PageDestination* EnginePdfMultiImpl::GetNamedDest(const WCHAR* name) {
    int n = 0;
    for (auto&& f : vbkm.vbkms) {
        auto e = f->engine;
        if (!e) {
            continue;
        }
        auto dest = e->GetNamedDest(name);
        if (dest) {
            // TODO: add n to page number in returned destination
            return dest;
        }
        n += e->PageCount();
    }
    return nullptr;
}

static void updateTocItemsPageNo(TocItem* i, int nPageNoAdd) {
    if (nPageNoAdd == 0) {
        return;
    }
    if (!i) {
        return;
    }
    auto curr = i;
    while (curr) {
        if (curr->dest) {
            curr->dest->pageNo += nPageNoAdd;
        }
        updateTocItemsPageNo(curr->child, nPageNoAdd);
        curr->pageNo += nPageNoAdd;
        curr = curr->next;
    }
}

TocTree* EnginePdfMultiImpl::GetToc() {
    CrashIf(!tocTree);
    return tocTree;
}

WCHAR* EnginePdfMultiImpl::GetPageLabel(int pageNo) const {
    if (pageNo < 1 || pageNo >= pageCount) {
        return nullptr;
    }

    EngineBase* e = PageToEngine(pageNo);
    return e->GetPageLabel(pageNo);
}

int EnginePdfMultiImpl::GetPageByLabel(const WCHAR* label) const {
    int n = 0;
    for (auto&& f : vbkm.vbkms) {
        auto e = f->engine;
        if (!e) {
            continue;
        }
        auto pageNo = e->GetPageByLabel(label);
        if (pageNo != -1) {
            return n + pageNo;
        }
        n += e->PageCount();
    }
    return -1;
}

extern void CalcEndPageNo(TocItem* root, int nPages);

static void MarkAsInvisibleRec(TocItem* ti, bool markInvisible, Vec<bool>& visible) {
    while (ti) {
        if (markInvisible) {
            for (int i = ti->pageNo; i < ti->endPageNo; i++) {
                visible[i - 1] = false;
            }
        }
        bool childMarkInvisible = markInvisible;
        if (!childMarkInvisible) {
            childMarkInvisible = ti->isUnchecked;
        }
        MarkAsInvisibleRec(ti->child, childMarkInvisible, visible);
        ti = ti->next;
    }
}

static void CalcRemovedPages(TocItem* root, Vec<bool>& visible) {
    int nPages = (int)visible.size();
    CalcEndPageNo(root, nPages);
    // in the first pass we mark the pages under unchecked nodes as invisible
    MarkAsInvisibleRec(root, root->isUnchecked, visible);

    // in the second pass we mark back pages that are visibles
}

bool EnginePdfMultiImpl::Load(const WCHAR* fileName, PasswordUI* pwdUI) {
    std::string_view sv = file::ReadFile(fileName);
    if (sv.empty()) {
        return false;
    }
    AutoFree svFree = sv;
    bool ok = ParseVbkmFile(sv, vbkm);
    if (!ok) {
        return false;
    }

    // open respective engines
    int nOpened = 0;
    int nTotalPages = 0;
    for (auto&& vbkm : vbkm.vbkms) {
        if (vbkm->filePath.empty()) {
            continue;
        }
        AutoFreeWstr path = strconv::Utf8ToWstr(vbkm->filePath.as_view());
        vbkm->engine = EngineManager::CreateEngine(path, pwdUI);
        if (!vbkm->engine) {
            return false;
        }
        tocTree = CloneTocTree(vbkm->toc);
        EngineBase* engine = vbkm->engine;
        if (!vbkm->engine) {
            return false;
        }
        int nPages = vbkm->engine->PageCount();

        Vec<bool> visiblePages;
        for (int i = 0; i < nPages; i++) {
            visiblePages.Append(true);
        }
        CalcRemovedPages(tocTree->root, visiblePages);

        int nPage = 0;
        for (int i = 0; i < nPages; i++) {
            EnginePage ep{i + 1, engine};
            pageToEngine.push_back(ep);
        }
        nOpened++;
        nTotalPages += nPages;
    }
    if (nOpened == 0) {
        return false;
    }
    pageCount = nTotalPages;
    return true;
}

EngineBase* EnginePdfMultiImpl::CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    if (str::IsEmpty(fileName)) {
        return nullptr;
    }
    EnginePdfMultiImpl* engine = new EnginePdfMultiImpl();
    if (!engine->Load(fileName, pwdUI)) {
        delete engine;
        return nullptr;
    }
    engine->fileName = str::Dup(fileName);
    return engine;
}

bool IsEnginePdfMultiSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        // we don't support sniffing
        return false;
    }
    return str::EndsWithI(fileName, L".vbkm");
}

EngineBase* CreateEnginePdfMultiFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    return EnginePdfMultiImpl::CreateFromFile(fileName, pwdUI);
}
