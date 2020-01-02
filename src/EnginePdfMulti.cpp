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
#include "EnginePdf.h"

struct VbkmFile {
    char* fileName = nullptr;
    char* path = nullptr;
    EngineBase* engine = nullptr;

    ~VbkmFile();
};

VbkmFile::~VbkmFile() {
    free(fileName);
    free(path);
    delete engine;
}

// represents .vbkm file
struct ParsedVbkm {
    AutoFree fileContent;

    Vec<VbkmFile*> files;

    ~ParsedVbkm();
};

ParsedVbkm::~ParsedVbkm() {
    DeleteVecMembers(files);
}

static EngineBase* findEngineForPage(ParsedVbkm* vbkm, int& pageNo) {
    for (auto&& f : vbkm->files) {
        if (!f->engine) {
            continue;
        }
        int nPages = f->engine->PageCount();
        if (pageNo <= nPages) {
            return f->engine;
        }
        pageNo -= nPages;
    }
    CrashIf(true);
    return nullptr;
}

Kind kindEnginePdfMulti = "enginePdfMulti";

class EnginePdfMultiImpl : public EngineBase {
  public:
    EnginePdfMultiImpl();
    virtual ~EnginePdfMultiImpl();
    EngineBase* Clone() override;

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving = false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    DocTocTree* GetTocTree() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    bool Load(const WCHAR* fileName, PasswordUI* pwdUI);

    static EngineBase* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI);

  protected:
    ParsedVbkm* vbkm = nullptr;

    DocTocTree* tocTree = nullptr;
};

EnginePdfMultiImpl::EnginePdfMultiImpl() {
    kind = kindEnginePdfMulti;
    defaultFileExt = L".vbkm";
    fileDPI = 72.0f;
}

EnginePdfMultiImpl::~EnginePdfMultiImpl() {
    delete vbkm;
    if (!tocTree) {
        return;
    }
    // we only own the first level. the rest is owned by their respective
    // engines, so we detach them before freeing
    auto curr = tocTree->root;
    while (curr) {
        curr->child = nullptr;
        curr = curr->next;
    }
    delete tocTree;
}

EngineBase* EnginePdfMultiImpl::Clone() {
    CrashIf(true);
    return nullptr;
}

RectD EnginePdfMultiImpl::PageMediabox(int pageNo) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->PageMediabox(pageNo);
}

RectD EnginePdfMultiImpl::PageContentBox(int pageNo, RenderTarget target) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->PageContentBox(pageNo, target);
}

RenderedBitmap* EnginePdfMultiImpl::RenderBitmap(int pageNo, float zoom, int rotation,
                                                 RectD* pageRect, /* if nullptr: defaults to the page's mediabox */
                                                 RenderTarget target, AbortCookie** cookie_out) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->RenderBitmap(pageNo, zoom, rotation, pageRect, target, cookie_out);
}

PointD EnginePdfMultiImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->Transform(pt, pageNo, zoom, rotation, inverse);
}
RectD EnginePdfMultiImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    auto e = findEngineForPage(vbkm, pageNo);
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
    auto e = findEngineForPage(vbkm, pageNo);
    return e->ExtractPageText(pageNo, coordsOut);
}

bool EnginePdfMultiImpl::HasClipOptimizations(int pageNo) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->HasClipOptimizations(pageNo);
}

WCHAR* EnginePdfMultiImpl::GetProperty(DocumentProperty prop) {
    return nullptr;
}

bool EnginePdfMultiImpl::SupportsAnnotation(bool forSaving) const {
    // TODO: needs to support annotations
    return false;
}

void EnginePdfMultiImpl::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    // TODO: support user annotations
}

bool EnginePdfMultiImpl::BenchLoadPage(int pageNo) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->BenchLoadPage(pageNo);
}

Vec<PageElement*>* EnginePdfMultiImpl::GetElements(int pageNo) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->GetElements(pageNo);
}

PageElement* EnginePdfMultiImpl::GetElementAtPos(int pageNo, PointD pt) {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->GetElementAtPos(pageNo, pt);
}

PageDestination* EnginePdfMultiImpl::GetNamedDest(const WCHAR* name) {
    int n = 0;
    for (auto&& f : vbkm->files) {
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

static void updateTocItemsPageNo(DocTocItem* i, int nPageNoAdd) {
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

DocTocTree* EnginePdfMultiImpl::GetTocTree() {
    if (tocTree) {
        return tocTree;
    }
    DocTocTree* tree = new DocTocTree();
    tree->name = str::Dup("bookmarks");
    int startPageNo = 0;
    for (auto&& f : vbkm->files) {
        auto e = f->engine;
        if (!e) {
            continue;
        }
        WCHAR* title = strconv::Utf8ToWstr(f->fileName);
        auto tocItem = new DocTocItem(nullptr, title, startPageNo + 1);
        free(title);
        if (!tree->root) {
            tree->root = tocItem;
        } else {
            tree->root->AddSibling(tocItem);
        }
        auto subTree = e->GetTocTree();
        if (subTree) {
            tocItem->child = subTree->root;
            tocItem->child->parent = tocItem;
            updateTocItemsPageNo(subTree->root, startPageNo);
        }
        startPageNo += e->PageCount();
    }
    tocTree = tree;
    return tocTree;
}

WCHAR* EnginePdfMultiImpl::GetPageLabel(int pageNo) const {
    auto e = findEngineForPage(vbkm, pageNo);
    return e->GetPageLabel(pageNo);
}

int EnginePdfMultiImpl::GetPageByLabel(const WCHAR* label) const {
    int n = 0;
    for (auto&& f : vbkm->files) {
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

// each logical record starts with "file:" line
// we split s into list of records for each file
// TODO: should we fail if the first line is not "file:" ?
// Currently we ignore everything from the beginning
// until first "file:" line
static Vec<std::string_view> SplitVbkmIntoRecords(std::string_view s) {
    Vec<std::string_view> res;
    auto tmp = s;
    Vec<const char*> addrs;

    // find indexes of lines that start with "file:"
    while (!tmp.empty()) {
        auto line = sv::ParseUntil(tmp, '\n');
        if (sv::StartsWith(line, "file:")) {
            addrs.push_back(line.data());
        }
    }

    size_t n = addrs.size();
    if (n == 0) {
        return res;
    }
    addrs.push_back(s.data() + s.size());
    for (size_t i = 0; i < n; i++) {
        const char* start = addrs[i];
        const char* end = addrs[i + 1];
        size_t size = end - start;
        auto sv = std::string_view{start, size};
        res.push_back(sv);
    }
    return res;
}

static std::string_view ParseLineFile(std::string_view s) {
    auto parts = sv::Split(s, ':', 2);
    if (parts.size() != 2) {
        return {};
    }
    return parts[1];
}

// parse a .vbkm record starting with "file:" line
static VbkmFile* ParseVbkmRecord(std::string_view s) {
    auto line = sv::ParseUntil(s, '\n');
    auto fileName = ParseLineFile(line);
    fileName = sv::TrimSpace(fileName);
    if (fileName.empty()) {
        return nullptr;
    }
    auto res = new VbkmFile();
    res->fileName = str::Dup(fileName);
    // TODO: parse more stuff
    return res;
}

static ParsedVbkm* ParseVbkmFile(std::string_view d) {
    AutoFree s = sv::NormalizeNewlines(d);
    auto records = SplitVbkmIntoRecords(s.as_view());
    auto n = records.size();
    if (n == 0) {
        return nullptr;
    }
    auto res = new ParsedVbkm();
    for (size_t i = 0; i < n; i++) {
        auto file = ParseVbkmRecord(records[i]);
        if (file == nullptr) {
            delete res;
            return nullptr;
        }
        res->files.push_back(file);
    }

    return res;
}

bool EnginePdfMultiImpl::Load(const WCHAR* fileName, PasswordUI* pwdUI) {
    auto sv = file::ReadFile(fileName);
    if (sv.empty()) {
        return false;
    }
    AutoFreeWstr dir = path::GetDir(fileName);
    AutoFree dirA = strconv::WstrToUtf8(dir);
    auto res = ParseVbkmFile(sv);
    res->fileContent = sv;

    // resolve file names to full paths
    for (auto&& vbkm : res->files) {
        char* fileName = vbkm->fileName;
        if (file::Exists(fileName)) {
            vbkm->path = str::Dup(vbkm->fileName);
            continue;
        }
        AutoFree path = path::JoinUtf(dirA, fileName, nullptr);
        if (file::Exists(path.as_view())) {
            vbkm->path = path.StealData();
        }
    }

    int nOpened = 0;
    int nPages = 0;
    for (auto&& vbkm : res->files) {
        if (!vbkm->path) {
            continue;
        }
        AutoFreeWstr path = strconv::Utf8ToWstr(vbkm->path);
        vbkm->engine = EngineManager::CreateEngine(path, pwdUI);
        if (vbkm->engine) {
            nOpened++;
            nPages += vbkm->engine->PageCount();
        }
    }
    if (nOpened == 0) {
        delete res;
        return false;
    }

    vbkm = res;
    pageCount = nPages;
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
