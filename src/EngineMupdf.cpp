/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"

#include "AppColors.h"
#include "Annotation.h"
#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EngineMupdfImpl.h"
#include "EngineMupdf.h"

#include "utils/Log.h"

// I'm a monkey, no idea why kZoom level, just know it's good
// maybe for reflowable should reflow at desired pixel size with
// identity ctm when rendering
constexpr float kZoom = 2.5f;
static float layoutDx = 420.f * kZoom;
static float layoutDy = 595.f * kZoom;
static float layoutFontEm = 11.f * kZoom;

// in mupdf_load_system_font.c
extern "C" void drop_cached_fonts_for_ctx(fz_context*);
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

Kind kindEngineMupdf = "enginePdf";

EngineMupdf* AsEngineMupdf(EngineBase* engine) {
    if (!engine || !IsOfKind(engine, kindEngineMupdf)) {
        return nullptr;
    }
    return (EngineMupdf*)engine;
}

static fz_link* FixupPageLinks(fz_link* root) {
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    fz_link* new_root = nullptr;
    while (root) {
        fz_link* tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link* link = new_root;
        if (link->rect.x0 > link->rect.x1) {
            std::swap(link->rect.x0, link->rect.x1);
        }
        if (link->rect.y0 > link->rect.y1) {
            std::swap(link->rect.y0, link->rect.y1);
        }
        CrashIf(link->rect.x1 < link->rect.x0);
        CrashIf(link->rect.y1 < link->rect.y0);
    }
    return new_root;
}

pdf_obj* PdfCopyStrDict(fz_context* ctx, pdf_document* doc, pdf_obj* dict) {
    pdf_obj* copy = pdf_copy_dict(ctx, dict);
    for (int i = 0; i < pdf_dict_len(ctx, copy); i++) {
        pdf_obj* val = pdf_dict_get_val(ctx, copy, i);
        // resolve all indirect references
        if (pdf_is_indirect(ctx, val)) {
            auto s = pdf_to_str_buf(ctx, val);
            auto slen = pdf_to_str_len(ctx, val);
            pdf_obj* val2 = pdf_new_string(ctx, s, slen);
            pdf_dict_put(ctx, copy, pdf_dict_get_key(ctx, copy, i), val2);
            pdf_drop_obj(ctx, val2);
        }
    }
    return copy;
}

static int PdfStreamNo(fz_context* ctx, pdf_obj* ref) {
    pdf_document* doc = pdf_get_indirect_document(ctx, ref);
    if (doc) {
        return pdf_to_num(ctx, ref);
    }
    return 0;
}

// Note: make sure to only call with ctxAccess
static fz_outline* PdfLoadAttachments(fz_context* ctx, pdf_document* doc) {
    pdf_obj* dict = pdf_load_name_tree(ctx, doc, PDF_NAME(EmbeddedFiles));
    if (!dict) {
        return nullptr;
    }

    fz_outline root = {0};
    fz_outline* curr = &root;
    for (int i = 0; i < pdf_dict_len(ctx, dict); i++) {
        pdf_obj* dest = pdf_dict_get_val(ctx, dict, i);

        int is_embedded = pdf_is_embedded_file(ctx, dest);
        if (is_embedded == 0) {
            continue;
        }
        pdf_obj* fs = pdf_embedded_file_stream(ctx, dest);
        int streamNo = PdfStreamNo(ctx, fs);
        const char* nameStr = pdf_embedded_file_name(ctx, dest);
        if (str::IsEmpty(nameStr)) {
            continue;
        }
        // int streamNo = pdf_to_num(ctx, embedded);
        fz_outline* link = fz_new_outline(ctx);
        link->title = fz_strdup(ctx, nameStr);
        link->uri = fz_strdup(ctx, nameStr); // TODO: maybe make file:// ?
        // TODO: a hack: re-using page as stream number
        // Could construct PageDestination here instead of delaying
        // until BuildToc
        link->page = streamNo;
        curr->next = link;
        curr = link;
    }
    pdf_drop_obj(ctx, dict);
    return root.next;
}

struct PageLabelInfo {
    int startAt = 0;
    int countFrom = 0;
    const char* type = nullptr;
    pdf_obj* prefix = nullptr;
};

int CmpPageLabelInfo(const void* a, const void* b) {
    return ((PageLabelInfo*)a)->startAt - ((PageLabelInfo*)b)->startAt;
}

WCHAR* FormatPageLabel(const char* type, int pageNo, const WCHAR* prefix) {
    if (str::Eq(type, "D")) {
        return str::Format(L"%s%d", prefix, pageNo);
    }
    if (str::EqI(type, "R")) {
        // roman numbering style
        AutoFreeWstr number(str::FormatRomanNumeral(pageNo));
        if (*type == 'r') {
            str::ToLowerInPlace(number.Get());
        }
        return str::Format(L"%s%s", prefix, number.Get());
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::WStr number;
        number.Append('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++) {
            number.Append(number.at(0));
        }
        if (*type == 'a') {
            str::ToLowerInPlace(number.Get());
        }
        return str::Format(L"%s%s", prefix, number.Get());
    }
    return str::Dup(prefix);
}

void BuildPageLabelRec(fz_context* ctx, pdf_obj* node, int pageCount, Vec<PageLabelInfo>& data) {
    pdf_obj* obj;
    if ((obj = pdf_dict_gets(ctx, node, "Kids")) != nullptr && !pdf_mark_obj(ctx, node)) {
        int n = pdf_array_len(ctx, obj);
        for (int i = 0; i < n; i++) {
            auto arr = pdf_array_get(ctx, obj, i);
            BuildPageLabelRec(ctx, arr, pageCount, data);
        }
        pdf_unmark_obj(ctx, node);
        return;
    }
    obj = pdf_dict_gets(ctx, node, "Nums");
    if (obj == nullptr) {
        return;
    }
    int n = pdf_array_len(ctx, obj);
    for (int i = 0; i < n; i += 2) {
        pdf_obj* info = pdf_array_get(ctx, obj, i + 1);
        PageLabelInfo pli;
        pli.startAt = pdf_to_int(ctx, pdf_array_get(ctx, obj, i)) + 1;
        if (pli.startAt < 1) {
            continue;
        }

        pli.type = pdf_to_name(ctx, pdf_dict_gets(ctx, info, "S"));
        pli.prefix = pdf_dict_gets(ctx, info, "P");
        pli.countFrom = pdf_to_int(ctx, pdf_dict_gets(ctx, info, "St"));
        if (pli.countFrom < 1) {
            pli.countFrom = 1;
        }
        data.Append(pli);
    }
}

WStrVec* BuildPageLabelVec(fz_context* ctx, pdf_obj* root, int pageCount) {
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(ctx, root, pageCount, data);
    data.Sort(CmpPageLabelInfo);

    size_t n = data.size();
    if (n == 0) {
        return nullptr;
    }

    PageLabelInfo& pli = data.at(0);
    if (n == 1 && pli.startAt == 1 && pli.countFrom == 1 && !pli.prefix && str::Eq(pli.type, "D")) {
        // this is the default case, no need for special treatment
        return nullptr;
    }

    WStrVec* labels = new WStrVec();
    labels->AppendBlanks(pageCount);

    for (size_t i = 0; i < n; i++) {
        pli = data.at(i);
        if (pli.startAt > pageCount) {
            break;
        }
        int secLen = pageCount + 1 - pli.startAt;
        if (i < n - 1 && data.at(i + 1).startAt <= pageCount) {
            secLen = data.at(i + 1).startAt - pli.startAt;
        }
        AutoFreeWstr prefix(PdfToWstr(ctx, data.at(i).prefix));
        for (int j = 0; j < secLen; j++) {
            int idx = pli.startAt + j - 1;
            free(labels->at(idx));
            WCHAR* label = FormatPageLabel(pli.type, pli.countFrom + j, prefix);
            labels->at(idx) = label;
        }
    }

    for (int idx = 0; (idx = labels->Find(nullptr, idx)) != -1; idx++) {
        labels->at(idx) = str::Dup(L"");
    }

    // ensure that all page labels are unique (by appending a number to duplicates)
    WStrVec dups(*labels);
    dups.Sort();
    int nDups = dups.isize();
    for (int i = 1; i < nDups; i++) {
        if (!str::Eq(dups.at(i), dups.at(i - 1))) {
            continue;
        }
        int idx = labels->Find(dups.at(i)), counter = 0;
        while ((idx = labels->Find(dups.at(i), idx + 1)) != -1) {
            AutoFreeWstr unique;
            do {
                unique.Set(str::Format(L"%s.%d", dups.at(i), ++counter));
            } while (labels->Contains(unique));
            str::ReplaceWithCopy(&labels->at(idx), unique);
        }
        nDups = dups.isize();
        for (; i + 1 < nDups && str::Eq(dups.at(i), dups.at(i + 1)); i++) {
            // no-op
        }
    }

    return labels;
}
struct PageTreeStackItem {
    pdf_obj* kids = nullptr;
    int i = -1;
    int len = 0;
    int next_page_no = 0;

    PageTreeStackItem() = default;
    ;
    explicit PageTreeStackItem(fz_context* ctx, pdf_obj* kids, int next_page_no = 0) {
        this->kids = kids;
        this->len = pdf_array_len(ctx, kids);
        this->next_page_no = next_page_no;
    }
};

// https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
bool PdfLink::SaveEmbedded(LinkSaverUI& saveUI) {
    CrashIf(!outline || !isAttachment);

    ScopedCritSec scope(engine->ctxAccess);
    // TODO: hack, we stored stream number in outline->page
    return engine->SaveEmbedded(saveUI, outline->page);
}
#endif

static void fz_lock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
    if (!str::EndsWith(msg, "\n")) {
        log("\n");
    }
}

static void InstallFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

EngineMupdf::EngineMupdf() {
    kind = kindEngineMupdf;
    defaultFileExt = L".pdf";
    fileDPI = 72.0f;

    for (size_t i = 0; i < dimof(mutexes); i++) {
        InitializeCriticalSection(&mutexes[i]);
    }
    InitializeCriticalSection(&pagesAccess);
    ctxAccess = &mutexes[FZ_LOCK_ALLOC];

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);
    InstallFitzErrorCallbacks(ctx);

    pdf_install_load_system_font_funcs(ctx);
    fz_register_document_handlers(ctx);
}

EngineMupdf::~EngineMupdf() {
    EnterCriticalSection(&pagesAccess);

    // TODO: remove this lock and see what happens
    EnterCriticalSection(ctxAccess);

    for (auto& piRef : _pages) {
        FzPageInfo* pi = &piRef;
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
    }

    fz_drop_outline(ctx, outline);
    fz_drop_outline(ctx, attachments);

    if (_info) {
        pdf_drop_obj(ctx, _info);
    }

    if (pdfdoc) {
        pdf_drop_page_tree(ctx, pdfdoc);
    }

    fz_drop_document(ctx, _doc);
    drop_cached_fonts_for_ctx(ctx);
    fz_drop_context(ctx);

    delete _pageLabels;
    delete tocTree;

    for (size_t i = 0; i < dimof(mutexes); i++) {
        LeaveCriticalSection(&mutexes[i]);
        DeleteCriticalSection(&mutexes[i]);
    }
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

class PasswordCloner : public PasswordUI {
    u8* cryptKey = nullptr;

  public:
    explicit PasswordCloner(u8* cryptKey) {
        this->cryptKey = cryptKey;
    }

    WCHAR* GetPassword(__unused const WCHAR* fileName, __unused u8* fileDigest, u8 decryptionKeyOut[32],
                       bool* saveKey) override {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return nullptr;
    }
};

EngineBase* EngineMupdf::Clone() {
    ScopedCritSec scope(ctxAccess);
    if (!FileName()) {
        // before port we could clone streams but it's no longer possible
        return nullptr;
    }

    // use this document's encryption key (if any) to load the clone
    PasswordCloner* pwdUI = nullptr;
    if (pdfdoc) {
        if (pdf_crypt_key(ctx, pdfdoc->crypt)) {
            pwdUI = new PasswordCloner(pdf_crypt_key(ctx, pdfdoc->crypt));
        }
    }

    EngineMupdf* clone = new EngineMupdf();
    bool ok = clone->Load(FileName(), pwdUI);
    if (!ok) {
        delete clone;
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    if (!decryptionKey && pdfdoc && pdfdoc->crypt) {
        free(clone->decryptionKey);
        clone->decryptionKey = nullptr;
    }

    return clone;
}

// File names ending in :<digits> are interpreted as containing
// embedded PDF documents (the digits is stream number of the embedded file stream)
// the caller must free()
const WCHAR* ParseEmbeddedStreamNumber(const WCHAR* path, int* streamNoOut) {
    int streamNo = -1;
    WCHAR* path2 = str::Dup(path);
    WCHAR* streamNoStr = (WCHAR*)FindEmbeddedPdfFileStreamNo(path2);
    if (streamNoStr) {
        WCHAR* rest = (WCHAR*)str::Parse(streamNoStr, L":%d", &streamNo);
        // there shouldn't be any left unparsed data
        CrashIf(!rest);
        if (!rest) {
            streamNo = -1;
        }
        // replace ':' with 0 to create a filesystem path
        *streamNoStr = 0;
    }
    *streamNoOut = streamNo;
    return path2;
}

// <filePath> should end with embed marks, which is a stream number
// inside pdf file
// TODO: provide PasswordUI?
std::span<u8> LoadEmbeddedPDFFile(const WCHAR* filePath) {
    EngineMupdf* engine = new EngineMupdf();
    auto res = engine->LoadStreamFromPDFFile(filePath);
    delete engine;
    return res;
}

std::span<u8> EngineMupdf::LoadStreamFromPDFFile(const WCHAR* filePath) {
    int streamNo = -1;
    AutoFreeWstr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);
    if (streamNo < 0) {
        return {};
    }
    fz_stream* file = nullptr;
    fz_try(ctx) {
        file = FzOpenFile2(ctx, fnCopy);
    }
    fz_catch(ctx) {
        file = nullptr;
    }

    if (!LoadFromStream(file, nullptr)) {
        return {};
    }

    pdf_document* doc = (pdf_document*)_doc;
    if (!pdf_obj_num_is_stream(ctx, doc, streamNo)) {
        return {};
    }

    fz_buffer* buffer = nullptr;
    fz_var(buffer);
    fz_try(ctx) {
        buffer = pdf_load_stream_number(ctx, doc, streamNo);
    }
    fz_catch(ctx) {
        return {};
    }
    auto dataSize = buffer->len;
    if (dataSize == 0) {
        return {};
    }
    auto data = (u8*)memdup(buffer->data, dataSize);
    fz_drop_buffer(ctx, buffer);

    fz_drop_document(ctx, _doc);
    _doc = nullptr;
    return {data, dataSize};
}

bool EngineMupdf::Load(const WCHAR* filePath, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
    SetFileName(filePath);
    if (!ctx) {
        return false;
    }

    int streamNo = -1;
    AutoFreeWstr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);

    fz_stream* file = nullptr;
    fz_try(ctx) {
        file = FzOpenFile2(ctx, fnCopy);
    }
    fz_catch(ctx) {
        file = nullptr;
    }

    if (!LoadFromStream(file, pwdUI)) {
        return false;
    }

    if (streamNo < 0) {
        return FinishLoading();
    }

    pdfdoc = pdf_specifics(ctx, _doc);
    if (pdfdoc) {
        if (!pdf_obj_num_is_stream(ctx, pdfdoc, streamNo)) {
            return false;
        }

        fz_buffer* buffer = nullptr;
        fz_var(buffer);
        fz_try(ctx) {
            buffer = pdf_load_stream_number(ctx, pdfdoc, streamNo);
            file = fz_open_buffer(ctx, buffer);
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buffer);
        }
        fz_catch(ctx) {
            return false;
        }
    }

    fz_drop_document(ctx, _doc);
    _doc = nullptr;

    if (!LoadFromStream(file, pwdUI)) {
        return false;
    }

    return FinishLoading();
}

bool EngineMupdf::Load(IStream* stream, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
    if (!ctx) {
        return false;
    }

    fz_stream* stm = nullptr;
    fz_try(ctx) {
        stm = FzOpenIStream(ctx, stream);
    }
    fz_catch(ctx) {
        return false;
    }
    if (!LoadFromStream(stm, pwdUI)) {
        return false;
    }
    return FinishLoading();
}

bool EngineMupdf::LoadFromStream(fz_stream* stm, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }

    _doc = nullptr;
    fz_try(ctx) {
        char* fileNameA = ToUtf8Temp(FileName());
        _doc = fz_open_document_with_stream(ctx, fileNameA, stm);
        pdfdoc = pdf_specifics(ctx, _doc);
        // TODO: maybe delay this call?
        fz_layout_document(ctx, _doc, layoutDx, layoutDy, layoutFontEm);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }

    _docStream = stm;

    isPasswordProtected = fz_needs_password(ctx, _doc);
    if (!isPasswordProtected) {
        return true;
    }

    if (!pwdUI) {
        return false;
    }

    // TODO: make this work for non-PDF formats?
    u8 digest[16 + 32] = {0};
    if (pdfdoc) {
        FzStreamFingerprint(ctx, pdfdoc->file, digest);
    }

    bool ok = false;
    bool saveKey = false;
    while (!ok) {
        u8* decryptKey = nullptr;
        if (pdfdoc) {
            decryptKey = pdf_crypt_key(ctx, pdfdoc->crypt);
        }
        AutoFreeWstr pwd(pwdUI->GetPassword(FileName(), digest, decryptKey, &saveKey));
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        // MuPDF expects passwords to be UTF-8 encoded
        AutoFree pwdA(strconv::WstrToUtf8(pwd));
        ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
        // according to the spec (1.7 ExtensionLevel 3), the password
        // for crypt revisions 5 and above are in SASLprep normalization
        if (!ok) {
            // TODO: this is only part of SASLprep
            pwd.Set(NormalizeString(pwd, 5 /* NormalizationKC */));
            if (pwd) {
                pwdA = strconv::WstrToUtf8(pwd);
                ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
            }
        }
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
        if (!ok && GetACP() != 1252) {
            AutoFree pwd_ansi(strconv::WstrToAnsiV(pwd));
            AutoFreeWstr pwd_cp1252(strconv::StrToWstr(pwd_ansi.Get(), 1252));
            pwdA = strconv::WstrToUtf8(pwd_cp1252);
            ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
        }
    }

    if (pdfdoc && ok && saveKey) {
        memcpy(digest + 16, pdf_crypt_key(ctx, pdfdoc->crypt), 32);
        decryptionKey = _MemToHex(&digest);
    }

    return ok;
}

static PageLayoutType GetPreferredLayout(fz_context* ctx, fz_document* doc) {
    PageLayoutType layout = Layout_Single;
    pdf_document* pdfdoc = pdf_specifics(ctx, doc);
    if (!pdfdoc) {
        return layout;
    }

    pdf_obj* root = nullptr;
    fz_try(ctx) {
        root = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Root");
    }
    fz_catch(ctx) {
        return layout;
    }

    fz_try(ctx) {
        const char* name = pdf_to_name(ctx, pdf_dict_gets(ctx, root, "PageLayout"));
        if (str::EndsWith(name, "Right")) {
            layout = Layout_Book;
        } else if (str::StartsWith(name, "Two")) {
            layout = Layout_Facing;
        }
    }
    fz_catch(ctx) {
    }

    fz_try(ctx) {
        pdf_obj* prefs = pdf_dict_gets(ctx, root, "ViewerPreferences");
        const char* direction = pdf_to_name(ctx, pdf_dict_gets(ctx, prefs, "Direction"));
        if (str::Eq(direction, "R2L")) {
            layout = (PageLayoutType)(layout | Layout_R2L);
        }
    }
    fz_catch(ctx) {
    }

    return layout;
}

static bool IsLinearizedFile(EngineMupdf* e) {
    if (!e->pdfdoc) {
        return false;
    }

    ScopedCritSec scope(e->ctxAccess);
    int isLinear = 0;
    fz_try(e->ctx) {
        isLinear = pdf_doc_was_linearized(e->ctx, e->pdfdoc);
    }
    fz_catch(e->ctx) {
        isLinear = 0;
    }
    return isLinear;
}

static void FinishNonPDFLoading(EngineMupdf* e) {
    ScopedCritSec scope(e->ctxAccess);

    auto ctx = e->ctx;
    for (int i = 0; i < e->pageCount; i++) {
        fz_rect mbox{};
        fz_matrix page_ctm{};
        fz_try(ctx) {
            fz_page* page = fz_load_page(ctx, e->_doc, i);
            mbox = fz_bound_page(ctx, page);
        }
        fz_catch(ctx) {
        }
        if (fz_is_empty_rect(mbox)) {
            fz_warn(ctx, "cannot find page size for page %d", i);
            mbox.x0 = 0;
            mbox.y0 = 0;
            mbox.x1 = 612;
            mbox.y1 = 792;
        }
        FzPageInfo* pageInfo = &e->_pages.at(i);
        pageInfo->mediabox = ToRectFl(mbox);
        pageInfo->pageNo = i + 1;
    }

    fz_try(ctx) {
        e->outline = fz_load_outline(ctx, e->_doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        fz_warn(ctx, "Couldn't load outline");
    }
}

bool EngineMupdf::FinishLoading() {
    pdfdoc = pdf_specifics(ctx, _doc);

    pageCount = 0;
    fz_try(ctx) {
        // this call might throw the first time
        pageCount = fz_count_pages(ctx, _doc);
    }
    fz_catch(ctx) {
        return false;
    }
    if (pageCount == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    preferredLayout = GetPreferredLayout(ctx, _doc);
    allowsPrinting = fz_has_permission(ctx, _doc, FZ_PERMISSION_PRINT);
    allowsCopyingText = fz_has_permission(ctx, _doc, FZ_PERMISSION_COPY);

    _pages.AppendBlanks(pageCount);
    if (!pdfdoc) {
        FinishNonPDFLoading(this);
        return true;
    }

    ScopedCritSec scope(ctxAccess);

    bool loadPageTreeFailed = false;

    fz_try(ctx) {
        pdf_load_page_tree(ctx, pdfdoc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "pdf_load_page_tree() failed");
        loadPageTreeFailed = true;
    }

    int nPages = pdfdoc->rev_page_count;
    if (nPages != pageCount) {
        fz_warn(ctx, "mismatch between fz_count_pages() and doc->rev_page_count");
        return false;
    }

    if (loadPageTreeFailed) {
        for (int pageNo = 0; pageNo < nPages; pageNo++) {
            FzPageInfo* pageInfo = &_pages[pageNo];
            pageInfo->pageNo = pageNo + 1;
            fz_rect mbox{};
            fz_try(ctx) {
                pdf_page* page = pdf_load_page(ctx, pdfdoc, pageNo);
                pageInfo->page = (fz_page*)page;
                mbox = pdf_bound_page(ctx, page);
            }
            fz_catch(ctx) {
            }

            if (fz_is_empty_rect(mbox)) {
                fz_warn(ctx, "cannot find page size for page %d", pageNo);
                mbox.x0 = 0;
                mbox.y0 = 0;
                mbox.x1 = 612;
                mbox.y1 = 792;
            }
            pageInfo->mediabox = ToRectFl(mbox);
        }
    } else {
        // this does the job of pdf_bound_page but without doing pdf_load_page()
        pdf_rev_page_map* map = pdfdoc->rev_page_map;
        for (int i = 0; i < nPages; i++) {
            int pageNo = map[i].page;
            int objNo = map[i].object;
            fz_rect mbox{};
            fz_matrix page_ctm{};
            fz_try(ctx) {
                pdf_obj* pageref = pdf_load_object(ctx, pdfdoc, objNo);
                pdf_page_obj_transform(ctx, pageref, &mbox, &page_ctm);
                mbox = fz_transform_rect(mbox, page_ctm);
                pdf_drop_obj(ctx, pageref);
            }
            fz_catch(ctx) {
            }
            if (fz_is_empty_rect(mbox)) {
                fz_warn(ctx, "cannot find page size for page %d", i);
                mbox.x0 = 0;
                mbox.y0 = 0;
                mbox.x1 = 612;
                mbox.y1 = 792;
            }
            FzPageInfo* pageInfo = &_pages[pageNo];
            pageInfo->mediabox = ToRectFl(mbox);
            pageInfo->pageNo = pageNo + 1;
        }
    }

    fz_try(ctx) {
        outline = fz_load_outline(ctx, _doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        fz_warn(ctx, "Couldn't load outline");
    }

    fz_try(ctx) {
        attachments = PdfLoadAttachments(ctx, pdfdoc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load attachments");
    }

    pdf_obj* orig_info = nullptr;
    fz_try(ctx) {
        // keep a copy of the Info dictionary, as accessing the original
        // isn't thread safe and we don't want to block for this when
        // displaying document properties
        orig_info = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Info");

        if (orig_info) {
            _info = PdfCopyStrDict(ctx, pdfdoc, orig_info);
        }
        if (!_info) {
            _info = pdf_new_dict(ctx, pdfdoc, 4);
        }
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile(this)) {
            pdf_dict_puts_drop(ctx, _info, "Linearized", PDF_TRUE);
        }
        pdf_obj* trailer = pdf_trailer(ctx, pdfdoc);
        pdf_obj* marked = pdf_dict_getp(ctx, trailer, "Root/MarkInfo/Marked");
        bool isMarked = pdf_to_bool(ctx, marked);
        if (isMarked) {
            pdf_dict_puts_drop(ctx, _info, "Marked", PDF_TRUE);
        }
        // also remember known output intents (PDF/X, etc.)
        pdf_obj* intents = pdf_dict_getp(ctx, trailer, "Root/OutputIntents");
        if (pdf_is_array(ctx, intents)) {
            int n = pdf_array_len(ctx, intents);
            pdf_obj* list = pdf_new_array(ctx, pdfdoc, n);
            for (int i = 0; i < n; i++) {
                pdf_obj* intent = pdf_dict_gets(ctx, pdf_array_get(ctx, intents, i), "S");
                if (pdf_is_name(ctx, intent) && !pdf_is_indirect(ctx, intent) &&
                    str::StartsWith(pdf_to_name(ctx, intent), "GTS_PDF")) {
                    pdf_array_push(ctx, list, intent);
                }
            }
            pdf_dict_puts_drop(ctx, _info, "OutputIntents", list);
        }
        // also note common unsupported features (such as XFA forms)
        pdf_obj* xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, pdfdoc), "Root/AcroForm/XFA");
        if (pdf_is_array(ctx, xfa)) {
            pdf_dict_puts_drop(ctx, _info, "Unsupported_XFA", PDF_TRUE);
        }
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
        pdf_drop_obj(ctx, _info);
        _info = nullptr;
    }

    fz_try(ctx) {
        pdf_obj* pageLabels = pdf_dict_getp(ctx, pdf_trailer(ctx, pdfdoc), "Root/PageLabels");
        if (pageLabels) {
            _pageLabels = BuildPageLabelVec(ctx, pageLabels, PageCount());
        }
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load page labels");
    }
    if (_pageLabels) {
        hasPageLabels = true;
    }

    // TODO: support javascript
    CrashIf(pdf_js_supported(ctx, pdfdoc));

    return true;
}

static IPageDestination* DestFromAttachment(EngineMupdf* engine, fz_outline* outline) {
    PageDestination* dest = new PageDestination();
    dest->kind = kindDestinationLaunchEmbedded;
    // WCHAR* path = strconv::Utf8ToWstr(outline->uri);
    dest->name = strconv::Utf8ToWstr(outline->title);
    // page is really a stream number
    dest->value = str::Format(L"%s:%d", engine->FileName(), outline->page);
    return dest;
}

TocItem* EngineMupdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment) {
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    while (outline) {
        WCHAR* name = nullptr;
        if (outline->title) {
            name = strconv::Utf8ToWstr(outline->title);
            name = PdfCleanString(name);
        }
        if (!name) {
            name = str::Dup(L"");
        }
        int pageNo = outline->page + 1;

        IPageDestination* dest = nullptr;
        Kind kindRaw = nullptr;
        if (isAttachment) {
            kindRaw = kindTocFzOutlineAttachment;
            dest = DestFromAttachment(this, outline);
        } else {
            kindRaw = kindTocFzOutline;
            dest = NewPageDestinationMupdf(nullptr, outline);
        }

        TocItem* item = NewTocItemWithDestination(parent, name, dest);
        item->kindRaw = kindRaw;
        item->rawVal1 = str::Dup(outline->title);
        item->rawVal2 = str::Dup(outline->uri);

        free(name);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = outline->flags;
        item->pageNo = pageNo;
        CrashIf(!item->PageNumbersMatch());

        if (outline->n_color > 0) {
            item->color = ColorRefFromPdfFloat(ctx, outline->n_color, outline->color);
        }

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter, isAttachment);
        }

        if (!root) {
            root = item;
            curr = item;
        } else {
            CrashIf(!curr);
            if (curr) {
                curr->next = item;
            }
            curr = item;
        }

        outline = outline->next;
    }

    return root;
}

// TODO: maybe build in FinishLoading
TocTree* EngineMupdf::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (outline == nullptr && attachments == nullptr) {
        return nullptr;
    }

    int idCounter = 0;

    TocItem* root = nullptr;
    TocItem* att = nullptr;
    if (outline) {
        root = BuildTocTree(nullptr, outline, idCounter, false);
    }
    if (!attachments) {
        goto MakeTree;
    }
    att = BuildTocTree(nullptr, attachments, idCounter, true);
    if (root) {
        root->AddSiblingAtEnd(att);
    } else {
        root = att;
    }
MakeTree:
    if (!root) {
        return nullptr;
    }
    TocItem* realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

IPageDestination* EngineMupdf::GetNamedDest(const WCHAR* name) {
    if (!pdfdoc) {
        return nullptr;
    }

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    auto nameA(ToUtf8Temp(name));
    pdf_obj* dest = nullptr;

    fz_var(dest);
    fz_try(ctx) {
        pdf_obj* nameobj = pdf_new_string(ctx, nameA.Get(), (int)nameA.size());
        dest = pdf_lookup_dest(ctx, pdfdoc, nameobj);
        pdf_drop_obj(ctx, nameobj);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!dest) {
        return nullptr;
    }

    IPageDestination* pageDest = nullptr;
    char* uri = nullptr;

    fz_var(uri);
    fz_try(ctx) {
        uri = pdf_parse_link_dest(ctx, pdfdoc, dest);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!uri) {
        return nullptr;
    }

    float x, y, zoom = 0;
    int pageNo = ResolveLink(uri, &x, &y, &zoom);

    RectF r{x, y, 0, 0};
    pageDest = NewSimpleDest(pageNo, r, zoom);
    fz_free(ctx, uri);
    return pageDest;
}

// return a page but only if is fully loaded
FzPageInfo* EngineMupdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = &_pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static IPageElement* NewFzComment(const WCHAR* comment, int pageNo, RectF rect) {
    auto res = new PageElementComment(comment);
    res->pageNo = pageNo;
    res->rect = rect;
    return res;
}

static IPageElement* MakePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_annot_rect(ctx, annot);
    auto tp = pdf_annot_type(ctx, annot);
    const char* contents = pdf_annot_contents(ctx, annot);
    const char* label = pdf_annot_field_label(ctx, annot);
    const char* s = contents;
    // TODO: use separate classes for comments and tooltips?
    if (str::IsEmpty(contents) && PDF_ANNOT_WIDGET == tp) {
        s = label;
    }
    auto ws = ToWstrTemp(s);
    RectF rd = ToRectFl(rect);
    return NewFzComment(ws, pageNo, rd);
}

static void MakePageElementCommentsFromAnnotations(fz_context* ctx, FzPageInfo* pageInfo) {
    Vec<IPageElement*>& comments = pageInfo->comments;

    auto page = pageInfo->page;
    if (!page) {
        return;
    }
    auto pdfpage = pdf_page_from_fz_page(ctx, page);
    int pageNo = pageInfo->pageNo;

    pdf_annot* annot;
    for (annot = pdf_first_annot(ctx, pdfpage); annot; annot = pdf_next_annot(ctx, annot)) {
        auto tp = pdf_annot_type(ctx, annot);
        const char* contents = pdf_annot_contents(ctx, annot); // don't free
        bool isContentsEmpty = str::IsEmpty(contents);
        const char* label = pdf_annot_field_label(ctx, annot); // don't free
        bool isLabelEmpty = str::IsEmpty(label);
        int flags = pdf_annot_field_flags(ctx, annot);

        if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
            logf("found file attachment annotation\n");

            pdf_obj* fs = pdf_dict_get(ctx, pdf_annot_obj(ctx, annot), PDF_NAME(FS));
            const char* attname = pdf_embedded_file_name(ctx, fs);
            fz_rect rect = pdf_annot_rect(ctx, annot);
            if (str::IsEmpty(attname) || fz_is_empty_rect(rect) || !pdf_is_embedded_file(ctx, fs)) {
                continue;
            }

            logf("attachement: %s\n", attname);

            auto dest = new PageDestination();
            dest->kind = kindDestinationLaunchEmbedded;
            dest->value = strconv::Utf8ToWstr(attname);

            auto el = new PageElementDestination(dest);
            el->pageNo = pageNo;
            el->rect = ToRectFl(rect);

            comments.Append(el);
            // TODO: need to implement https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
            // for saving the attachment to a file
            // TODO: expose /Contents in addition to the file path
            continue;
        }

        if (!isContentsEmpty && tp != PDF_ANNOT_FREE_TEXT) {
            auto comment = MakePdfCommentFromPdfAnnot(ctx, pageNo, annot);
            comments.Append(comment);
            continue;
        }

        if (PDF_ANNOT_WIDGET == tp && !isLabelEmpty) {
            if (!(flags & PDF_FIELD_IS_READ_ONLY)) {
                auto comment = MakePdfCommentFromPdfAnnot(ctx, pageNo, annot);
                comments.Append(comment);
            }
        }
    }

    // re-order list into top-to-bottom order (i.e. last-to-first)
    comments.Reverse();
}

// Maybe: handle FZ_ERROR_TRYLATER, which can happen when parsing from network.
// (I don't think we read from network now).
// Maybe: when loading fully, cache extracted text in FzPageInfo
// so that we don't have to re-do fz_new_stext_page_from_page() when doing search
FzPageInfo* EngineMupdf::GetFzPageInfo(int pageNo, bool loadQuick) {
    // TODO: minimize time spent under pagesAccess when fully loading
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = &_pages[pageIdx];

    ScopedCritSec ctxScope(ctxAccess);
    if (!pageInfo->page) {
        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, _doc, pageIdx);
        }
        fz_catch(ctx) {
        }
    }

    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    if (pageInfo->commentsNeedRebuilding) {
        CrashIf(!pdfdoc);
        DeleteVecMembers(pageInfo->comments);
        MakePageElementCommentsFromAnnotations(ctx, pageInfo);
        pageInfo->commentsNeedRebuilding = false;
    }

    if (loadQuick || pageInfo->fullyLoaded) {
        return pageInfo;
    }

    CrashIf(pageInfo->pageNo != pageNo);

    pageInfo->fullyLoaded = true;

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, page, &opts);
    }
    fz_catch(ctx) {
    }

    auto links = fz_load_links(ctx, page);

    pageInfo->links = FixupPageLinks(links);
    if (pdfdoc) {
        MakePageElementCommentsFromAnnotations(ctx, pageInfo);
    }
    if (!stext) {
        return pageInfo;
    }

    FzLinkifyPageText(pageInfo, stext);
    FzFindImagePositions(ctx, pageNo, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectF EngineMupdf::PageMediabox(int pageNo) {
    FzPageInfo* pi = &_pages[pageNo - 1];
    return pi->mediabox;
}

RectF EngineMupdf::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        // maybe should return a dummy size. not sure how this
        // will play with layout. The page should fail to render
        // since the doc is broken and page is missing
        return RectF();
    }

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie = {};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    fz_display_list* list = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);
    fz_var(list);

    RectF mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        list = fz_new_display_list_from_page(ctx, pageInfo->page);
        if (list) {
            dev = fz_new_bbox_device(ctx, &rect);
            fz_run_display_list(ctx, list, dev, fz_identity, pagerect, &fzcookie);
            fz_close_device(ctx, dev);
        }
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        if (list) {
            fz_drop_display_list(ctx, list);
        }
    }
    fz_catch(ctx) {
        return mediabox;
    }

    if (!list) {
        return mediabox;
    }

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectF rect2 = ToRectFl(rect);
    return rect2.Intersect(mediabox);
}

RectF EngineMupdf::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    if (zoom <= 0) {
        char* name = str::Dup("");
        const WCHAR* nameW = FileName();
        if (nameW) {
            name = strconv::WstrToUtf8(nameW);
        }
        logf("doc: %s, pageNo: %d, zoom: %.2f\n", name, pageNo, zoom);
        free(name);
    }
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        zoom = 1;
    }
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = To_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return ToRectFl(rect2);
}

RenderedBitmap* EngineMupdf::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    // TODO(port): I don't see why this lock is needed
    ScopedCritSec cs(ctxAccess);

    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto rotation = args.rotation;
    fz_rect pRect;
    if (pageRect) {
        pRect = To_fz_rect(*pageRect);
    } else {
        // TODO(port): use pageInfo->mediabox?
        pRect = fz_bound_page(ctx, page);
    }
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_irect bbox = fz_round_rect(fz_transform_rect(pRect, ctm));

    fz_colorspace* csRgb = fz_device_rgb(ctx);
    fz_irect ibounds = bbox;

    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    const char* usage = "View";
    switch (args.target) {
        case RenderTarget::Print:
            usage = "Print";
            break;
    }

    if (pdfdoc) {
        fz_try(ctx) {
            pdf_page* pdfpage = pdf_page_from_fz_page(ctx, page);
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
            // or "Print". "Export" is not used
            dev = fz_new_draw_device(ctx, ctm, pix);
            pdf_run_page_with_usage(ctx, pdfpage, dev, fz_identity, usage, fzcookie);
            bitmap = NewRenderedFzPixmap(ctx, pix);
            fz_close_device(ctx, dev);
        }
        fz_always(ctx) {
            if (dev) {
                fz_drop_device(ctx, dev);
            }
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            delete bitmap;
            return nullptr;
        }
    } else {
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            // TODO: how is mupdf getting away with fz_clear_pixmap()?
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            dev = fz_new_draw_device(ctx, ctm, pix);
            fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
            bitmap = NewRenderedFzPixmap(ctx, pix);
        }
        fz_always(ctx) {
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            delete bitmap;
            return nullptr;
        }
    }

    return bitmap;
}

IPageElement* EngineMupdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

Vec<IPageElement*>* EngineMupdf::GetElements(int pageNo) {
    auto pageInfo = GetFzPageInfoFast(pageNo);
    auto res = new Vec<IPageElement*>();
    FzGetElements(res, pageInfo);
    if (res->IsEmpty()) {
        delete res;
        return nullptr;
    }
    return res;
}

#if 0
static char* PdfLinkGetURI(fz_link* link, fz_outline* outline) {
    if (link) {
        return link->uri;
    }
    if (outline) {
        return outline->uri;
    }
    return nullptr;
}

Kind CalcDestKind(fz_link* link, fz_outline* outline) {
    // outline entries with page set to -1 go nowhere
    // see https://github.com/sumatrapdfreader/sumatrapdf/issues/1352
    if (outline && outline->page == -1) {
        return kindDestinationNone;
    }
    char* uri = PdfLinkGetURI(link, outline);
    // some outline entries are bad (issue 1245)
    if (!uri) {
        return kindDestinationNone;
    }
    if (!IsExternalLink(uri)) {
        float x = 0, y = 0, zoom = 0;
        int pageNo = ResolveLink(uri, &x, &y, &zoom);
        if (pageNo == -1) {
            // TODO: figure out what it could be
            logf("CalcDestKind(): unknown uri: '%s'\n", uri);
            // ReportIf(true);
            return nullptr;
        }
        return kindDestinationScrollTo;
    }
    if (str::StartsWith(uri, "file:")) {
        // TODO: investigate more, happens in pier-EsugAwards2007.pdf
        return kindDestinationLaunchFile;
    }
    // TODO: hackish way to detect uris of various kinds
    // like http:, news:, mailto:, tel: etc.
    if (str::FindChar(uri, ':') != nullptr) {
        return kindDestinationLaunchURL;
    }

    logf("CalcDestKind(): unknown uri: '%s'\n", uri);
    // TODO: kindDestinationLaunchEmbedded, kindDestinationLaunchURL, named destination
    // ReportIf(true);
    return nullptr;
}

WCHAR* CalcValue(fz_link* link, fz_outline* outline) {
    char* uri = PdfLinkGetURI(link, outline);
    if (!uri) {
        return nullptr;
    }
    if (!IsExternalLink(uri)) {
        // other values: #1,115,208
        return nullptr;
    }
    WCHAR* path = strconv::Utf8ToWstr(uri);
    return path;
}

WCHAR* CalcDestName(fz_link* link, fz_outline* outline) {
    char* uri = PdfLinkGetURI(link, outline);
    if (!uri) {
        return nullptr;
    }
    if (IsExternalLink(uri)) {
        return nullptr;
    }
    // TODO(port): test with more stuff
    // figure out what PDF_NAME(GoToR) ends up being
    return strconv::Utf8ToWstr(uri);
}

int CalcDestPageNo(fz_link* link, fz_outline* outline) {
    char* uri = PdfLinkGetURI(link, outline);
    // TODO: happened in ug_logodesign.pdf. investigate
    // CrashIf(!uri);
    if (!uri) {
        return 0;
    }
    if (IsExternalLink(uri)) {
        return 0;
    }
    float x, y;
    int pageNo = ResolveLink(uri, &x, &y, nullptr);
    if (pageNo == -1) {
        return 0;
    }
    return pageNo + 1; // TODO(port): or is it just pageNo?

    return 0;
}

RectF CalcDestRect(fz_link* link, fz_outline* outline) {
    RectF result(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    char* uri = PdfLinkGetURI(link, outline);
    // TODO: this happens in pdf/ug_logodesign.pdf, there's only outline without
    // pageno. need to investigate
    // CrashIf(!uri);
    if (!uri) {
        return result;
    }

    if (IsExternalLink(uri)) {
        return result;
    }
    float x = 0;
    float y = 0;
    int pageNo = ResolveLink(uri, &x, &y, nullptr);
    if (pageNo == -1) {
        // SubmitBugReportIf(pageNo == -1);
        return result;
    }

    result.x = (double)x;
    result.y = (double)y;
    return result;
}

IPageDestination* NewPageDestinationMupdf(fz_link* link, fz_outline* outline) {
    auto dest = new PageDestinationMupdf(link, outline);
    CrashIf(!dest->kind);
    if (dest->kind == kindDestinationScrollTo) {
        char* uri = PdfLinkGetURI(link, outline);
        float x = 0, y = 0, zoom = 0;
        int pageNo = ResolveLink(uri, &x, &y, &zoom);
        dest->pageNo = pageNo + 1;
        dest->rect = RectF(x, y, x, y);
        dest->value = strconv::Utf8ToWstr(uri);
        dest->name = strconv::Utf8ToWstr(uri);
        dest->zoom = zoom;
    } else {
        // TODO: clean this up
        dest->rect = CalcDestRect(link, outline);
        dest->value = CalcValue(link, outline);
        dest->name = CalcDestName(link, outline);
        dest->pageNo = CalcDestPageNo(link, outline);
    }
    if ((dest->pageNo <= 0) && (dest->kind != kindDestinationNone) && (dest->kind != kindDestinationLaunchFile) &&
        (dest->kind != kindDestinationLaunchURL) && (dest->kind != kindDestinationLaunchEmbedded)) {
        logf("dest->kind: %s, dest->pageNo: %d\n", dest->kind, dest->pageNo);
        // ReportIf(dest->pageNo <= 0);
    }
    return dest;
}
#endif

void HandleLinkMupdf(EngineMupdf* e, IPageDestination* dest, ILinkHandler* linkHandler) {
    CrashIf(kindDestinationMupdf != dest->GetKind());
    PageDestinationMupdf* link = (PageDestinationMupdf*)dest;
    CrashIf(!(link->outline || link->link));
    const char* uri = link->outline ? link->outline->uri : nullptr;
    if (!link->outline) {
        uri = link->link->uri;
    }
    float x, y;
    float zoom = 0.f;
    ResolveLink(uri, &x, &y, &zoom);
    fz_location loc = fz_resolve_link(e->ctx, e->_doc, uri, &x, &y);
    int pageNo = fz_page_number_from_location(e->ctx, e->_doc, loc);
    auto ctrl = linkHandler->GetController();
    if (pageNo != -1) {
        RectF r(x, y, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
        ctrl->ScrollTo(pageNo + 1, r, zoom);
        return;
    }
    if (IsExternalLink(uri)) {
        linkHandler->LauncURL(uri);
        return;
    }
    // TODO: more?
    CrashIf(true);
}

bool EngineMupdf::HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) {
    Kind k = dest->GetKind();
    if (k == kindDestinationMupdf) {
        HandleLinkMupdf(this, dest, linkHandler);
        return true;
    }
    linkHandler->GotoLink(dest);
    return true;
}

RenderedBitmap* EngineMupdf::GetImageForPageElement(IPageElement* ipel) {
    CrashIf(kindPageElementImage != ipel->GetKind());
    auto pel = (PageElementImage*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

bool EngineMupdf::SaveFileAsPDF(const char* pdfFileName) {
    return SaveFileAs(pdfFileName);
}

bool EngineMupdf::BenchLoadPage(int pageNo) {
    return GetFzPageInfo(pageNo, false) != nullptr;
}

fz_matrix EngineMupdf::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = To_fz_rect(PageMediabox(pageNo));
    return FzCreateViewCtm(tmpRc, zoom, rotation);
}

fz_matrix EngineMupdf::viewctm(fz_page* page, float zoom, int rotation) const {
    return FzCreateViewCtm(fz_bound_page(ctx, page), zoom, rotation);
}

RenderedBitmap* EngineMupdf::GetPageImage(int pageNo, RectF rect, int imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= images.isize();
    fz_rect imgRect = images.at(imageIdx).rect;
    bool badRect = ToRectFl(imgRect) != rect;
    CrashIf(outOfBounds);
    CrashIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);

    fz_image* image = FzFindImageAtIdx(ctx, pageInfo, imageIdx);
    CrashIf(!image);
    if (!image) {
        return nullptr;
    }

    RenderedBitmap* bmp = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(pixmap);
    fz_var(bmp);

    fz_try(ctx) {
        // TODO(port): not sure if should provide subarea, w and h
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        bmp = NewRenderedFzPixmap(ctx, pixmap);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    return bmp;
}

PageText EngineMupdf::ExtractPageText(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return {};
    }

    ScopedCritSec scope(ctxAccess);

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
    }
    if (!stext) {
        return {};
    }
    PageText res;
    // TODO: convert to return PageText
    WCHAR* text = FzTextPageToStr(stext, &res.coords);
    fz_drop_stext_page(ctx, stext);
    res.text = text;
    res.len = (int)str::Len(text);
    return res;
}

static void pdf_extract_fonts(fz_context* ctx, pdf_obj* res, Vec<pdf_obj*>& fontList, Vec<pdf_obj*>& resList) {
    if (!res || pdf_mark_obj(ctx, res)) {
        return;
    }
    resList.Append(res);

    pdf_obj* fonts = pdf_dict_gets(ctx, res, "Font");
    for (int k = 0; k < pdf_dict_len(ctx, fonts); k++) {
        pdf_obj* font = pdf_resolve_indirect(ctx, pdf_dict_get_val(ctx, fonts, k));
        if (font && !fontList.Contains(font)) {
            fontList.Append(font);
        }
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj* xobjs = pdf_dict_gets(ctx, res, "XObject");
    for (int k = 0; k < pdf_dict_len(ctx, xobjs); k++) {
        pdf_obj* xobj = pdf_dict_get_val(ctx, xobjs, k);
        pdf_obj* xres = pdf_dict_gets(ctx, xobj, "Resources");
        pdf_extract_fonts(ctx, xres, fontList, resList);
    }
}

WCHAR* EngineMupdf::ExtractFontList() {
    Vec<pdf_obj*> fontList;
    Vec<pdf_obj*> resList;

    // collect all fonts from all page objects
    int nPages = PageCount();
    for (int i = 1; i <= nPages; i++) {
        auto pageInfo = GetFzPageInfo(i, false);
        if (!pageInfo) {
            continue;
        }
        fz_page* fzpage = pageInfo->page;
        if (!fzpage) {
            continue;
        }

        ScopedCritSec scope(ctxAccess);
        pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);
        fz_try(ctx) {
            pdf_obj* resources = pdf_page_resources(ctx, page);
            pdf_extract_fonts(ctx, resources, fontList, resList);
            pdf_annot* annot;
            for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot)) {
                pdf_obj* o = pdf_annot_ap(ctx, annot);
                if (o) {
                    // TODO(port): not sure this is the right thing
                    resources = pdf_xobject_resources(ctx, o);
                    pdf_extract_fonts(ctx, resources, fontList, resList);
                }
            }
        }
        fz_catch(ctx) {
        }
    }

    // start ctxAccess scope here so that we don't also have to
    // ask for pagesAccess (as is required for GetFzPage)
    ScopedCritSec scope(ctxAccess);

    for (pdf_obj* res : resList) {
        pdf_unmark_obj(ctx, res);
    }

    WStrVec fonts;
    for (size_t i = 0; i < fontList.size(); i++) {
        const char *name = nullptr, *type = nullptr, *encoding = nullptr;
        AutoFree anonFontName;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj* font = fontList.at(i);
            pdf_obj* font2 = pdf_array_get(ctx, pdf_dict_gets(ctx, font, "DescendantFonts"), 0);
            if (!font2) {
                font2 = font;
            }

            name = pdf_to_name(ctx, pdf_dict_getsa(ctx, font2, "BaseFont", "Name"));
            bool needAnonName = str::IsEmpty(name);
            if (needAnonName && font2 != font) {
                name = pdf_to_name(ctx, pdf_dict_getsa(ctx, font, "BaseFont", "Name"));
                needAnonName = str::IsEmpty(name);
            }
            if (needAnonName) {
                anonFontName.Set(str::Format("<#%d>", pdf_obj_parent_num(ctx, font2)));
                name = anonFontName;
            }
            embedded = false;
            pdf_obj* desc = pdf_dict_gets(ctx, font2, "FontDescriptor");
            if (desc && (pdf_dict_gets(ctx, desc, "FontFile") || pdf_dict_getsa(ctx, desc, "FontFile2", "FontFile3"))) {
                embedded = true;
            }
            if (embedded && str::Len(name) > 7 && name[6] == '+') {
                name += 7;
            }

            type = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Subtype"));
            if (font2 != font) {
                const char* type2 = pdf_to_name(ctx, pdf_dict_gets(ctx, font2, "Subtype"));
                if (str::Eq(type2, "CIDFontType0")) {
                    type = "Type1 (CID)";
                } else if (str::Eq(type2, "CIDFontType2")) {
                    type = "TrueType (CID)";
                }
            }
            if (str::Eq(type, "Type3")) {
                embedded = pdf_dict_gets(ctx, font2, "CharProcs") != nullptr;
            }

            encoding = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Encoding"));
            if (str::Eq(encoding, "WinAnsiEncoding")) {
                encoding = "Ansi";
            } else if (str::Eq(encoding, "MacRomanEncoding")) {
                encoding = "Roman";
            } else if (str::Eq(encoding, "MacExpertEncoding")) {
                encoding = "Expert";
            }
        }
        fz_catch(ctx) {
            continue;
        }
        CrashIf(!name || !type || !encoding);

        str::Str info;
        if (name[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0)) {
            info.Append(strconv::ToMultiByteV(name, 936, CP_UTF8).data());
        } else {
            info.Append(name);
        }
        if (!str::IsEmpty(encoding) || !str::IsEmpty(type) || embedded) {
            info.Append(" (");
            if (!str::IsEmpty(type)) {
                info.AppendFmt("%s; ", type);
            }
            if (!str::IsEmpty(encoding)) {
                info.AppendFmt("%s; ", encoding);
            }
            if (embedded) {
                info.Append("embedded; ");
            }
            info.RemoveAt(info.size() - 2, 2);
            info.Append(")");
        }

        auto fontInfo = ToWstrTemp(info.LendData());
        if (fontInfo.Get() && !fonts.Contains(fontInfo)) {
            fonts.Append(fontInfo.Get());
        }
    }
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

static const char* DocumentPropertyToMupdfMetadataKey(DocumentProperty prop) {
    switch (prop) {
        case DocumentProperty::Title:
            return FZ_META_INFO_TITLE;
        case DocumentProperty::Author:
            return FZ_META_INFO_AUTHOR;
        case DocumentProperty::Subject:
            return "info:Subject";
        case DocumentProperty::PdfProducer:
            return FZ_META_INFO_PRODUCER;
        case DocumentProperty::CreatorApp:
            return "info:Creator"; // not sure if the same meaning
        case DocumentProperty::CreationDate:
            return "info:CreationDate";
        case DocumentProperty::ModificationDate:
            return "info:ModDate";
    }
    return nullptr;
}

WCHAR* EngineMupdf::GetProperty(DocumentProperty prop) {
    const char* key = DocumentPropertyToMupdfMetadataKey(prop);
    if (key) {
        char buf[1024]{};
        int bufSize = (int)dimof(buf);
        int n = fz_lookup_metadata(ctx, _doc, key, buf, bufSize);
        if (n > 0) {
            if (n > bufSize) {
                // can be bigger if output truncated
                n = bufSize - 1;
                buf[bufSize - 1] = 0; // not sure if necessary
            }
            WCHAR* s = strconv::Utf8ToWstr(buf, (size_t)n - 1);
            return s;
        }
    }
    if (!pdfdoc) {
        return nullptr;
    }

    if (DocumentProperty::PdfVersion == prop) {
        int major = pdfdoc->version / 10, minor = pdfdoc->version % 10;
        pdf_crypt* crypt = pdfdoc->crypt;
        if (1 == major && 7 == minor && pdf_crypt_version(ctx, crypt) == 5) {
            if (pdf_crypt_revision(ctx, crypt) == 5) {
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 3);
            }
            if (pdf_crypt_revision(ctx, crypt) == 6) {
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 8);
            }
        }
        return str::Format(L"%d.%d", major, minor);
    }

    if (DocumentProperty::PdfFileStructure == prop) {
        WStrVec fstruct;
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Linearized"))) {
            fstruct.Append(str::Dup(L"linearized"));
        }
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Marked"))) {
            fstruct.Append(str::Dup(L"tagged"));
        }
        if (pdf_dict_gets(ctx, _info, "OutputIntents")) {
            int n = pdf_array_len(ctx, pdf_dict_gets(ctx, _info, "OutputIntents"));
            for (int i = 0; i < n; i++) {
                pdf_obj* intent = pdf_array_get(ctx, pdf_dict_gets(ctx, _info, "OutputIntents"), i);
                CrashIf(!str::StartsWith(pdf_to_name(ctx, intent), "GTS_"));
                fstruct.Append(strconv::Utf8ToWstr(pdf_to_name(ctx, intent) + 4));
            }
        }
        if (fstruct.size() == 0) {
            return nullptr;
        }
        return fstruct.Join(L",");
    }

    if (DocumentProperty::UnsupportedFeatures == prop) {
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Unsupported_XFA"))) {
            return str::Dup(L"XFA");
        }
        return nullptr;
    }

    if (DocumentProperty::FontList == prop) {
        return ExtractFontList();
    }

    static struct {
        DocumentProperty prop;
        const char* name;
    } pdfPropNames[] = {
        {DocumentProperty::Title, "Title"},
        {DocumentProperty::Author, "Author"},
        {DocumentProperty::Subject, "Subject"},
        {DocumentProperty::Copyright, "Copyright"},
        {DocumentProperty::CreationDate, "CreationDate"},
        {DocumentProperty::ModificationDate, "ModDate"},
        {DocumentProperty::CreatorApp, "Creator"},
        {DocumentProperty::PdfProducer, "Producer"},
    };
    for (int i = 0; i < dimof(pdfPropNames); i++) {
        if (pdfPropNames[i].prop == prop) {
            // _info is guaranteed not to contain any indirect references,
            // so no need for ctxAccess
            pdf_obj* obj = pdf_dict_gets(ctx, _info, pdfPropNames[i].name);
            if (!obj) {
                return nullptr;
            }
            WCHAR* s = PdfToWstr(ctx, obj);
            return PdfCleanString(s);
        }
    }
    return nullptr;
};

std::span<u8> EngineMupdf::GetFileData() {
    if (!pdfdoc) {
        return {};
    }

    std::span<u8> res;
    ScopedCritSec scope(ctxAccess);

    fz_var(res);
    fz_try(ctx) {
        res = FzExtractStreamData(ctx, pdfdoc->file);
    }
    fz_catch(ctx) {
        res = {};
    }

    if (!res.empty()) {
        return res;
    }

    auto path = FileName();
    if (!path) {
        return {};
    }
    return file::ReadFile(path);
}

// TODO: proper support for includeUserAnnots or maybe just remove it
bool EngineMupdf::SaveFileAs(const char* copyFileName) {
    auto dstPath = ToWstrTemp(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.AsSpan());
        return ok;
    }
    auto path = FileName();
    if (!path) {
        return false;
    }
    bool ok = file::Copy(dstPath, path, false);
    return ok;
}

const pdf_write_options pdf_default_write_options2 = {
    0,  /* do_incremental */
    0,  /* do_pretty */
    0,  /* do_ascii */
    0,  /* do_compress */
    0,  /* do_compress_images */
    0,  /* do_compress_fonts */
    0,  /* do_decompress */
    0,  /* do_garbage */
    0,  /* do_linear */
    0,  /* do_clean */
    0,  /* do_sanitize */
    0,  /* do_appearance */
    0,  /* do_encrypt */
    0,  /* dont_regenerate_id */
    ~0, /* permissions */
    "", /* opwd_utf8[128] */
    "", /* upwd_utf8[128] */
};

// re-save current pdf document using mupdf (as opposed to just saving the data)
// this is used after the PDF was modified by the user (e.g. by adding / changing
// annotations).
// if filePath is not given, we save under the same name
// TODO: if the file is locked, this might fail.
bool EngineMupdfSaveUpdated(EngineBase* engine, std::string_view path,
                            std::function<void(std::string_view)> showErrorFunc) {
    CrashIf(!engine);
    if (!engine) {
        return false;
    }
    EngineMupdf* epdf = (EngineMupdf*)engine;
    if (!epdf->pdfdoc) {
        return false;
    }

    TempStr currPath = ToUtf8Temp(engine->FileName());
    if (path.empty()) {
        path = {currPath.Get()};
    }
    fz_context* ctx = epdf->ctx;

    pdf_write_options save_opts{};
    save_opts = pdf_default_write_options2;
    save_opts.do_incremental = pdf_can_be_saved_incrementally(ctx, epdf->pdfdoc);
    save_opts.do_compress = 1;
    save_opts.do_compress_images = 1;
    save_opts.do_compress_fonts = 1;
    if (epdf->pdfdoc->redacted) {
        save_opts.do_garbage = 1;
    }

    bool ok = false;
    fz_try(ctx) {
        pdf_save_document(ctx, epdf->pdfdoc, path.data(), &save_opts);
        ok = true;
        logf("Saved annotations to '%s'\n", path.data());
    }
    fz_catch(ctx) {
        const char* mupdfErr = fz_caught_message(epdf->ctx);
        logf("Saving '%s' failed with: '%s'\n", path.data(), mupdfErr);
        if (showErrorFunc) {
            showErrorFunc(mupdfErr);
        }
    }
    return ok;
}

// https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
bool EngineMupdf::SaveEmbedded(LinkSaverUI& saveUI, int num) {
    ScopedCritSec scope(ctxAccess);

    fz_buffer* buf = nullptr;
    fz_try(ctx) {
        buf = pdf_load_stream_number(ctx, doc, num);
    }
    fz_catch(ctx) {
        return false;
    }
    CrashIf(nullptr == buf);
    u8* data = nullptr;
    size_t dataLen = fz_buffer_extract(ctx, buf, &data);
    std::string_view sv{(char*)data, dataLen};
    bool result = saveUI.SaveEmbedded(sv);
    fz_drop_buffer(ctx, buf);
    return result;
}
#endif

bool EngineMupdf::HasClipOptimizations(int pageNo) {
    if (!pdfdoc) {
        return false;
    }

    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo || !pageInfo->page) {
        return false;
    }

    fz_rect mbox = To_fz_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (FzRectOverlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

WCHAR* EngineMupdf::GetPageLabel(int pageNo) const {
    if (!_pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    return str::Dup(_pageLabels->at(pageNo - 1));
}

int EngineMupdf::GetPageByLabel(const WCHAR* label) const {
    if (!pdfdoc) {
        return 0;
    }
    int pageNo = 0;
    if (_pageLabels) {
        pageNo = _pageLabels->Find(label) + 1;
    }

    if (!pageNo) {
        return EngineBase::GetPageByLabel(label);
    }

    return pageNo;
}

int EngineMupdf::GetAnnotations(Vec<Annotation*>* annotsOut) {
    if (!pdfdoc) {
        return 0;
    }
    int nAnnots = 0;
    for (int i = 1; i <= pageCount; i++) {
        auto pi = GetFzPageInfo(i, true);
        pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pi->page);
        pdf_annot* annot = pdf_first_annot(ctx, pdfpage);
        while (annot) {
            Annotation* a = MakeAnnotationPdf(this, annot, i);
            if (a) {
                annotsOut->Append(a);
                nAnnots++;
            }
            annot = pdf_next_annot(ctx, annot);
        }
    }
    return nAnnots;
}

EngineBase* EngineMupdf::CreateFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    if (str::IsEmpty(path)) {
        return nullptr;
    }
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(path, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineMupdf::CreateFromStream(IStream* stream, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEngineMupdfSupportedFileType(Kind kind) {
    if (kind == kindFilePDF) {
        return true;
    }
    if (kind == kindFileEpub) {
        return true;
    }
    if (kind == kindFileFb2) {
        return true;
    }
    if (kind == kindFileHTML) {
        return true;
    }
    if (kind == kindFileSvg) {
        return true;
    }
    return false;
}

EngineBase* CreateEngineMupdfFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    return EngineMupdf::CreateFromFile(path, pwdUI);
}

EngineBase* CreateEngineMupdfFromStream(IStream* stream, PasswordUI* pwdUI) {
    return EngineMupdf::CreateFromStream(stream, pwdUI);
}

int EngineMupdfGetAnnotations(EngineBase* engine, Vec<Annotation*>* annotsOut) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    return epdf->GetAnnotations(annotsOut);
}

bool EngineMupdfHasUnsavedAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return false;
    }
    int res = pdf_has_unsaved_changes(epdf->ctx, epdf->pdfdoc);
    return res != 0;
}

bool EngineMupdfSupportsAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    return (epdf->pdfdoc != nullptr);
}

static bool IsAllowedAnnot(AnnotationType tp, AnnotationType* allowed) {
    if (!allowed) {
        return true;
    }
    int i = 0;
    while (allowed[i] != AnnotationType::Unknown) {
        AnnotationType tp2 = allowed[i];
        if (tp2 == tp) {
            return true;
        }
        ++i;
    }
    return false;
}

// caller must delete
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, AnnotationType* allowedAnnots) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return nullptr;
    }
    FzPageInfo* pi = epdf->GetFzPageInfo(pageNo, true);

    ScopedCritSec cs(epdf->ctxAccess);

    pdf_page* pdfpage = pdf_page_from_fz_page(epdf->ctx, pi->page);
    pdf_annot* annot = pdf_first_annot(epdf->ctx, pdfpage);
    fz_point p{pos.x, pos.y};

    // find last annotation that contains this point
    // they are drawn in order so later annotations
    // are drawn on top of earlier
    pdf_annot* matched = nullptr;
    while (annot) {
        enum pdf_annot_type tp = pdf_annot_type(epdf->ctx, annot);
        AnnotationType atp = AnnotationTypeFromPdfAnnot(tp);
        if (IsAllowedAnnot(atp, allowedAnnots)) {
            fz_rect rc = pdf_annot_rect(epdf->ctx, annot);
            if (fz_is_point_inside_rect(p, rc)) {
                matched = annot;
            }
        }
        annot = pdf_next_annot(epdf->ctx, annot);
    }
    if (matched) {
        return MakeAnnotationPdf(epdf, matched, pageNo);
    }
    return nullptr;
}

void EngineMupdf::InvalideAnnotationsForPage(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = &_pages[pageIdx];
    if (pageInfo) {
        pageInfo->commentsNeedRebuilding = true;
    }
}

Annotation* MakeAnnotationPdf(EngineMupdf* engine, pdf_annot* annot, int pageNo) {
    CrashIf(!engine->pdfdoc);
    ScopedCritSec cs(engine->ctxAccess);

    auto tp = pdf_annot_type(engine->ctx, annot);
    AnnotationType typ = AnnotationTypeFromPdfAnnot(tp);
    if (typ == AnnotationType::Unknown) {
        // unsupported type
        return nullptr;
    }

    CrashIf(pageNo < 1);
    Annotation* res = new Annotation();
    res->engine = engine;
    res->pageNo = pageNo;
    res->pdfannot = annot;
    res->type = typ;
    return res;
}
