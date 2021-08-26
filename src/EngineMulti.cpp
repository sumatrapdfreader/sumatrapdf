/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/DirIter.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "AppColors.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineMupdfImpl.h"
#include "EngineAll.h"

#include "utils/Log.h"

struct EngineInfo {
    TocItem* tocRoot = nullptr;
    EngineBase* engine = nullptr;
};

struct EnginePage {
    int pageNoInEngine = 0;
    EngineBase* engine = nullptr;
};

Kind kindEngineMulti = "enginePdfMulti";

class EngineMulti : public EngineBase {
  public:
    EngineMulti();
    ~EngineMulti() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;
    bool SaveFileAsPDF(const char* pdfFileName) override;
    PageText ExtractPageText(int pageNo) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    IPageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    [[nodiscard]] WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    bool Load(const WCHAR* fileName, PasswordUI* pwdUI);
    bool LoadFromFiles(std::string_view dir, VecStr& files);
    void UpdatePagesForEngines(Vec<EngineInfo>& enginesInfo);

    EngineBase* PageToEngine(int& pageNo) const;
    Vec<EnginePage> pageToEngine;
    Vec<EngineInfo> enginesInfo;
    TocTree* tocTree = nullptr;
};

EngineBase* EngineMulti::PageToEngine(int& pageNo) const {
    const EnginePage& ep = pageToEngine[pageNo - 1];
    pageNo = ep.pageNoInEngine;
    return ep.engine;
}

EngineMulti::EngineMulti() {
    kind = kindEngineMulti;
    defaultExt = L""; // TODO: no extension, is it important?
    fileDPI = 72.0f;
}

EngineMulti::~EngineMulti() {
    for (auto&& ei : enginesInfo) {
        delete ei.engine;
    }
    delete tocTree;
}

EngineBase* EngineMulti::Clone() {
    // TODO: support CreateFromFiles()
    CrashIf(true);
    return nullptr;
}

RectF EngineMulti::PageMediabox(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->PageMediabox(pageNo);
}

RectF EngineMulti::PageContentBox(int pageNo, RenderTarget target) {
    EngineBase* e = PageToEngine(pageNo);
    return e->PageContentBox(pageNo, target);
}

RenderedBitmap* EngineMulti::RenderPage(RenderPageArgs& args) {
    EngineBase* e = PageToEngine(args.pageNo);
    return e->RenderPage(args);
}

RectF EngineMulti::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    EngineBase* e = PageToEngine(pageNo);
    return e->Transform(rect, pageNo, zoom, rotation, inverse);
}

ByteSlice EngineMulti::GetFileData() {
    return {};
}

bool EngineMulti::SaveFileAs(const char*) {
    return false;
}

bool EngineMulti::SaveFileAsPDF(const char*) {
    return false;
}

PageText EngineMulti::ExtractPageText(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->ExtractPageText(pageNo);
}

bool EngineMulti::HasClipOptimizations(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->HasClipOptimizations(pageNo);
}

WCHAR* EngineMulti::GetProperty(DocumentProperty prop) {
    return nullptr;
}

bool EngineMulti::BenchLoadPage(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->BenchLoadPage(pageNo);
}

Vec<IPageElement*> EngineMulti::GetElements(int pageNo) {
    EngineBase* e = PageToEngine(pageNo);
    return e->GetElements(pageNo);
}

// don't delete the result
IPageElement* EngineMulti::GetElementAtPos(int pageNo, PointF pt) {
    EngineBase* e = PageToEngine(pageNo);
    return e->GetElementAtPos(pageNo, pt);
}

RenderedBitmap* EngineMulti::GetImageForPageElement(IPageElement* ipel) {
    CrashIf(kindPageElementImage != ipel->GetKind());
    PageElementImage* pel = (PageElementImage*)ipel;
    EngineBase* e = PageToEngine(pel->pageNo);
    return e->GetImageForPageElement(pel);
}

IPageDestination* EngineMulti::GetNamedDest(const WCHAR* name) {
    for (auto&& pe : pageToEngine) {
        EngineBase* e = pe.engine;
        auto dest = e->GetNamedDest(name);
        if (dest) {
            // TODO: fix up page number in returned destination
            return dest;
        }
    }
    return nullptr;
}

static bool IsPageNavigationDestination(IPageDestination* dest) {
    if (!dest) {
        return false;
    }
    if (dest->GetKind() == kindDestinationScrollTo) {
        return true;
    }
    // TODO: possibly more kinds
    return false;
}

static void updateTocItemsPageNo(TocItem* ti, int nPageNoAdd, bool root) {
    if (nPageNoAdd == 0) {
        return;
    }
    if (!ti) {
        return;
    }
    auto curr = ti;
    while (curr) {
        if (IsPageNavigationDestination(curr->dest)) {
            auto dest = (PageDestination*)curr->dest;
            dest->pageNo += nPageNoAdd;
            curr->pageNo += nPageNoAdd;
        }

        updateTocItemsPageNo(curr->child, nPageNoAdd, false);
        if (root) {
            return;
        }
        curr = curr->next;
    }
}

TocTree* EngineMulti::GetToc() {
    CrashIf(!tocTree);
    return tocTree;
}

WCHAR* EngineMulti::GetPageLabel(int pageNo) const {
    if (pageNo < 1 || pageNo >= pageCount) {
        return nullptr;
    }

    EngineBase* e = PageToEngine(pageNo);
    return e->GetPageLabel(pageNo);
}

int EngineMulti::GetPageByLabel(const WCHAR* label) const {
    for (auto&& pe : pageToEngine) {
        EngineBase* e = pe.engine;
        int pageNo = e->GetPageByLabel(label);
        if (pageNo != -1) {
            // TODO: fixup page number
            return pageNo;
        }
    }
    return -1;
}

#if 0
static void CollectTocItemsRecur(TocItem* ti, Vec<TocItem*>& v) {
    while (ti) {
        v.Append(ti);
        CollectTocItemsRecur(ti->child, v);
        ti = ti->next;
    }
}

static bool cmpByPageNo(TocItem* ti1, TocItem* ti2) {
    return ti1->pageNo < ti2->pageNo;
}

static void CalcEndPageNo(TocItem* root, int nPages) {
    Vec<TocItem*> tocItems;
    CollectTocItemsRecur(root, tocItems);
    size_t n = tocItems.size();
    if (n < 1) {
        return;
    }
    std::sort(tocItems.begin(), tocItems.end(), cmpByPageNo);
    TocItem* prev = tocItems[0];
    for (size_t i = 1; i < n; i++) {
        TocItem* next = tocItems[i];
        prev->endPageNo = next->pageNo - 1;
        if (prev->endPageNo < prev->pageNo) {
            prev->endPageNo = prev->pageNo;
        }
        prev = next;
    }
    prev->endPageNo = nPages;
}
#endif

static TocItem* CloneTocItemRecur(TocItem* ti, bool removeUnchecked) {
    if (ti == nullptr) {
        return nullptr;
    }
    if (removeUnchecked && ti->isUnchecked) {
        TocItem* next = ti->next;
        while (next && next->isUnchecked) {
            next = next->next;
        }
        return CloneTocItemRecur(next, removeUnchecked);
    }
    TocItem* res = new TocItem();
    res->parent = ti->parent;
    res->title = str::Dup(ti->title);
    res->isOpenDefault = ti->isOpenDefault;
    res->isOpenToggled = ti->isOpenToggled;
    res->isUnchecked = ti->isUnchecked;
    res->pageNo = ti->pageNo;
    res->id = ti->id;
    res->fontFlags = ti->fontFlags;
    res->color = ti->color;
    res->dest = ti->dest;
    res->destNotOwned = true;
    res->child = CloneTocItemRecur(ti->child, removeUnchecked);

    res->nPages = ti->nPages;
    res->engineFilePath = str::Dup(ti->engineFilePath);

    TocItem* next = ti->next;
    if (removeUnchecked) {
        while (next && next->isUnchecked) {
            next = next->next;
        }
    }
    res->next = CloneTocItemRecur(next, removeUnchecked);
    return res;
}

TocItem* CreateWrapperItem(EngineBase* engine) {
    TocItem* tocFileRoot = nullptr;
    TocTree* tocTree = engine->GetToc();
    // it's ok if engine doesn't have toc
    if (tocTree) {
        tocFileRoot = CloneTocItemRecur(tocTree->root, false);
    }

    int nPages = engine->PageCount();
    const WCHAR* title = path::GetBaseNameTemp(engine->FileName());
    TocItem* tocWrapper = new TocItem(tocFileRoot, title, 0);
    tocWrapper->isOpenDefault = true;
    tocWrapper->child = tocFileRoot;
    char* filePath = (char*)strconv::WstrToUtf8(engine->FileName());
    tocWrapper->engineFilePath = filePath;
    tocWrapper->nPages = nPages;
    tocWrapper->pageNo = 1;
    if (tocFileRoot) {
        tocFileRoot->parent = tocWrapper;
    }
    return tocWrapper;
}

bool EngineMulti::LoadFromFiles(std::string_view dir, VecStr& files) {
    int n = files.Size();
    TocItem* tocFiles = nullptr;
    for (int i = 0; i < n; i++) {
        std::string_view path = files.at(i);
        auto pathW = ToWstrTemp(path);
        EngineBase* engine = CreateEngine(pathW, nullptr, true);
        if (!engine) {
            continue;
        }

        TocItem* wrapper = CreateWrapperItem(engine);
        if (tocFiles == nullptr) {
            tocFiles = wrapper;
        } else {
            tocFiles->AddSiblingAtEnd(wrapper);
        }

        EngineInfo ei;
        ei.engine = engine;
        ei.tocRoot = wrapper;
        enginesInfo.Append(ei);
    }
    if (tocFiles == nullptr) {
        return false;
    }
    UpdatePagesForEngines(enginesInfo);

    auto dirW = ToWstrTemp(dir);
    TocItem* root = new TocItem(nullptr, dirW, 0);
    root->child = tocFiles;
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);

    auto fileName = ToWstrTemp(dir);
    SetFileName(fileName);

    return true;
}

void EngineMulti::UpdatePagesForEngines(Vec<EngineInfo>& enginesInfo) {
    int nTotalPages = 0;
    for (auto&& ei : enginesInfo) {
        TocItem* root = ei.tocRoot;
        if (root->isUnchecked) {
            continue;
        }
        int nPages = ei.engine->PageCount();
        for (int i = 1; i <= nPages; i++) {
            EnginePage ep{i, ei.engine};
            pageToEngine.Append(ep);
        }
        updateTocItemsPageNo(ei.tocRoot, nTotalPages, true);
        nTotalPages += nPages;
    }
    pageCount = nTotalPages;
    CrashIf((size_t)pageCount != pageToEngine.size());

    auto verifyPages = [&nTotalPages](TocItem* ti) -> bool {
        if (!IsPageNavigationDestination(ti->dest)) {
            return true;
        }
        int pageNo = ti->pageNo;
        CrashIf(pageNo > nTotalPages);
        return true;
    };

    for (auto&& ei : enginesInfo) {
        TocItem* root = ei.tocRoot;
        if (root->isUnchecked) {
            continue;
        }
        VisitTocTree(root, verifyPages);
    }
}

bool IsEngineMultiSupportedFileType(Kind kind) {
    return kind == kindDirectory;
}

EngineBase* CreateEngineMultiFromFiles(std::string_view dir, VecStr& files) {
    EngineMulti* engine = new EngineMulti();
    if (!engine->LoadFromFiles(dir, files)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMultiFromDirectory(const WCHAR* dirW) {
    auto isValidFunc = [](std::string_view path) -> bool {
        bool isValid = str::EndsWithI(path.data(), ".pdf");
        return isValid;
    };
    VecStr files;
    auto dir = ToUtf8Temp(dirW);
    bool ok = CollectFilesFromDirectory(dir.AsView(), files, isValidFunc);
    if (!ok) {
        // TODO: show error message
        return nullptr;
    }
    if (files.Size() == 0) {
        // TODO: show error message
        return nullptr;
    }
    EngineBase* engine = CreateEngineMultiFromFiles(dir.AsView(), files);
    if (!engine) {
        // TODO: show error message
        return nullptr;
    }
    return engine;
}
