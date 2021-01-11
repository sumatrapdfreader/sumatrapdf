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
#include "utils/Log.h"
#include "utils/LogDbg.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EnginePdf.h"

// in mupdf_load_system_font.c
extern "C" void drop_cached_fonts_for_ctx(fz_context*);
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp);

Kind kindEnginePdf = "enginePdf";

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

pdf_obj* pdf_copy_str_dict(fz_context* ctx, pdf_document* doc, pdf_obj* dict) {
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

static int pdf_stream_no(fz_context* ctx, pdf_obj* ref) {
    pdf_document* doc = pdf_get_indirect_document(ctx, ref);
    if (doc) {
        return pdf_to_num(ctx, ref);
    }
    return 0;
}

// Note: make sure to only call with ctxAccess
static fz_outline* pdf_load_attachments(fz_context* ctx, pdf_document* doc) {
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
        int streamNo = pdf_stream_no(ctx, fs);
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
        AutoFreeWstr prefix(pdf_to_wstr(ctx, data.at(i).prefix));
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
            str::ReplacePtr(&labels->at(idx), unique);
        }
        nDups = dups.isize();
        for (; i + 1 < nDups && str::Eq(dups.at(i), dups.at(i + 1)); i++) {
            // no-op
        }
    }

    return labels;
}

void fz_find_images(fz_stext_page* text, Vec<FitzImagePos>& images) {
    if (!text) {
        return;
    }
    fz_stext_block* block = text->first_block;
    fz_image* image;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_IMAGE) {
            block = block->next;
            continue;
        }
        image = block->u.i.image;
        if (image->colorspace != nullptr) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1480
            // fz_convert_pixmap_samples doesn't handle src without colorspace
            // TODO: this is probably not right
            FitzImagePos img = {block->bbox, block->u.i.transform};
            images.Append(img);
        }
        block = block->next;
    }
}

struct PageTreeStackItem {
    pdf_obj* kids = nullptr;
    int i = -1;
    int len = 0;
    int next_page_no = 0;

    PageTreeStackItem(){};
    explicit PageTreeStackItem(fz_context* ctx, pdf_obj* kids, int next_page_no = 0) {
        this->kids = kids;
        this->len = pdf_array_len(ctx, kids);
        this->next_page_no = next_page_no;
    }
};

class EnginePdf : public EngineBase {
  public:
    EnginePdf();
    ~EnginePdf() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    PageText ExtractPageText(int pageNo) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    int GetAnnotations(Vec<Annotation*>* annotsOut);

    static EngineBase* CreateFromFile(const WCHAR* path, PasswordUI* pwdUI);
    static EngineBase* CreateFromStream(IStream* stream, PasswordUI* pwdUI);

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;

    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    RenderedBitmap* GetPageImage(int pageNo, RectF rect, int imageIdx);

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo> _pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;
    pdf_obj* _info = nullptr;
    WStrVec* _pageLabels = nullptr;

    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* filePath, PasswordUI* pwdUI = nullptr);
    bool Load(IStream* stream, PasswordUI* pwdUI = nullptr);
    // TODO(port): fz_stream can no-longer be re-opened (fz_clone_stream)
    // bool Load(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool LoadFromStream(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool FinishLoading();

    FzPageInfo* GetFzPageInfoFast(int pageNo);
    FzPageInfo* GetFzPageInfo(int pageNo, bool loadQuick);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation);
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment);
    WCHAR* ExtractFontList();

    std::span<u8> LoadStreamFromPDFFile(const WCHAR* filePath);
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
    EnginePdf* e = (EnginePdf*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EnginePdf* e = (EnginePdf*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
    if (!str::EndsWith(msg, "\n")) {
        log("\n");
    }
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

EnginePdf::EnginePdf() {
    kind = kindEnginePdf;
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
    installFitzErrorCallbacks(ctx);

    pdf_install_load_system_font_funcs(ctx);
}

EnginePdf::~EnginePdf() {
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
    pdf_drop_obj(ctx, _info);

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

    WCHAR* GetPassword([[maybe_unused]] const WCHAR* fileName, [[maybe_unused]] u8* fileDigest, u8 decryptionKeyOut[32],
                       bool* saveKey) override {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return nullptr;
    }
};

EngineBase* EnginePdf::Clone() {
    ScopedCritSec scope(ctxAccess);
    if (!FileName()) {
        // before port we could clone streams but it's no longer possible
        return nullptr;
    }

    // use this document's encryption key (if any) to load the clone
    PasswordCloner* pwdUI = nullptr;
    pdf_document* doc = (pdf_document*)_doc;
    if (pdf_crypt_key(ctx, doc->crypt)) {
        pwdUI = new PasswordCloner(pdf_crypt_key(ctx, doc->crypt));
    }

    EnginePdf* clone = new EnginePdf();
    bool ok = clone->Load(FileName(), pwdUI);
    if (!ok) {
        delete clone;
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    if (!decryptionKey && doc->crypt) {
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
    EnginePdf* engine = new EnginePdf();
    auto res = engine->LoadStreamFromPDFFile(filePath);
    delete engine;
    return res;
}

std::span<u8> EnginePdf::LoadStreamFromPDFFile(const WCHAR* filePath) {
    int streamNo = -1;
    AutoFreeWstr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);
    if (streamNo < 0) {
        return {};
    }
    fz_stream* file = nullptr;
    fz_try(ctx) {
        file = fz_open_file2(ctx, fnCopy);
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

bool EnginePdf::Load(const WCHAR* filePath, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
    SetFileName(filePath);
    if (!ctx) {
        return false;
    }

    int streamNo = -1;
    AutoFreeWstr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);

    fz_stream* file = nullptr;
    fz_try(ctx) {
        file = fz_open_file2(ctx, fnCopy);
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

    pdf_document* doc = (pdf_document*)_doc;
    if (!pdf_obj_num_is_stream(ctx, doc, streamNo)) {
        return false;
    }

    fz_buffer* buffer = nullptr;
    fz_var(buffer);
    fz_try(ctx) {
        buffer = pdf_load_stream_number(ctx, doc, streamNo);
        file = fz_open_buffer(ctx, buffer);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx) {
        return false;
    }

    fz_drop_document(ctx, _doc);
    _doc = nullptr;

    if (!LoadFromStream(file, pwdUI)) {
        return false;
    }

    return FinishLoading();
}

bool EnginePdf::Load(IStream* stream, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
    if (!ctx) {
        return false;
    }

    fz_stream* stm = nullptr;
    fz_try(ctx) {
        stm = fz_open_istream(ctx, stream);
    }
    fz_catch(ctx) {
        return false;
    }
    if (!LoadFromStream(stm, pwdUI)) {
        return false;
    }
    return FinishLoading();
}

bool EnginePdf::LoadFromStream(fz_stream* stm, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }

    fz_try(ctx) {
        pdf_document* doc = pdf_open_document_with_stream(ctx, stm);
        _doc = (fz_document*)doc;
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

    u8 digest[16 + 32] = {0};
    pdf_document* doc = (pdf_document*)_doc;
    fz_stream_fingerprint(ctx, doc->file, digest);

    bool ok = false, saveKey = false;
    while (!ok) {
        AutoFreeWstr pwd(pwdUI->GetPassword(FileName(), digest, pdf_crypt_key(ctx, doc->crypt), &saveKey));
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        // MuPDF expects passwords to be UTF-8 encoded
        AutoFree pwd_utf8(strconv::WstrToUtf8(pwd));
        ok = pdf_authenticate_password(ctx, doc, pwd_utf8.Get());
        // according to the spec (1.7 ExtensionLevel 3), the password
        // for crypt revisions 5 and above are in SASLprep normalization
        if (!ok) {
            // TODO: this is only part of SASLprep
            pwd.Set(NormalizeString(pwd, 5 /* NormalizationKC */));
            if (pwd) {
                pwd_utf8 = strconv::WstrToUtf8(pwd);
                ok = pdf_authenticate_password(ctx, doc, pwd_utf8.Get());
            }
        }
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
        if (!ok && GetACP() != 1252) {
            AutoFree pwd_ansi(strconv::WstrToAnsi(pwd));
            AutoFreeWstr pwd_cp1252(strconv::FromCodePage(pwd_ansi.Get(), 1252));
            pwd_utf8 = strconv::WstrToUtf8(pwd_cp1252);
            ok = pdf_authenticate_password(ctx, doc, pwd_utf8.Get());
        }
    }

    if (ok && saveKey) {
        memcpy(digest + 16, pdf_crypt_key(ctx, doc->crypt), 32);
        decryptionKey = _MemToHex(&digest);
    }

    return ok;
}

static PageLayoutType GetPreferredLayout(fz_context* ctx, pdf_document* doc) {
    PageLayoutType layout = Layout_Single;

    pdf_obj* root = nullptr;
    fz_try(ctx) {
        root = pdf_dict_gets(ctx, pdf_trailer(ctx, doc), "Root");
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

static bool IsLinearizedFile(EnginePdf* e) {
    ScopedCritSec scope(e->ctxAccess);

    pdf_document* doc = pdf_document_from_fz_document(e->ctx, e->_doc);
    int isLinear = 0;
    fz_try(e->ctx) {
        isLinear = pdf_doc_was_linearized(e->ctx, doc);
    }
    fz_catch(e->ctx) {
        isLinear = 0;
    }
    return isLinear;
}

bool EnginePdf::FinishLoading() {
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

    pdf_document* doc = (pdf_document*)_doc;

    preferredLayout = GetPreferredLayout(ctx, doc);
    allowsPrinting = fz_has_permission(ctx, _doc, FZ_PERMISSION_PRINT);
    allowsCopyingText = fz_has_permission(ctx, _doc, FZ_PERMISSION_COPY);

    ScopedCritSec scope(ctxAccess);

    bool loadPageTreeFailed = false;
    fz_try(ctx) {
        pdf_load_page_tree(ctx, doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "pdf_load_page_tree() failed");
        loadPageTreeFailed = true;
    }

    int nPages = doc->rev_page_count;
    if (nPages != pageCount) {
        fz_warn(ctx, "mismatch between fz_count_pages() and doc->rev_page_count");
        return false;
    }

    _pages.AppendBlanks(pageCount);

    if (loadPageTreeFailed) {
        for (int pageNo = 0; pageNo < nPages; pageNo++) {
            FzPageInfo* pageInfo = &_pages[pageNo];
            pageInfo->pageNo = pageNo + 1;
            fz_rect mbox{};
            fz_try(ctx) {
                pdf_page* page = pdf_load_page(ctx, doc, pageNo);
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
        pdf_rev_page_map* map = doc->rev_page_map;
        for (int i = 0; i < nPages; i++) {
            int pageNo = map[i].page;
            int objNo = map[i].object;
            fz_rect mbox{};
            fz_matrix page_ctm{};
            fz_try(ctx) {
                pdf_obj* pageref = pdf_load_object(ctx, doc, objNo);
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
        attachments = pdf_load_attachments(ctx, doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load attachments");
    }

    pdf_obj* orig_info = nullptr;
    fz_try(ctx) {
        // keep a copy of the Info dictionary, as accessing the original
        // isn't thread safe and we don't want to block for this when
        // displaying document properties
        orig_info = pdf_dict_gets(ctx, pdf_trailer(ctx, doc), "Info");

        if (orig_info) {
            _info = pdf_copy_str_dict(ctx, doc, orig_info);
        }
        if (!_info) {
            _info = pdf_new_dict(ctx, doc, 4);
        }
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile(this)) {
            pdf_dict_puts_drop(ctx, _info, "Linearized", PDF_TRUE);
        }
        pdf_obj* trailer = pdf_trailer(ctx, doc);
        pdf_obj* marked = pdf_dict_getp(ctx, trailer, "Root/MarkInfo/Marked");
        bool isMarked = pdf_to_bool(ctx, marked);
        if (isMarked) {
            pdf_dict_puts_drop(ctx, _info, "Marked", PDF_TRUE);
        }
        // also remember known output intents (PDF/X, etc.)
        pdf_obj* intents = pdf_dict_getp(ctx, trailer, "Root/OutputIntents");
        if (pdf_is_array(ctx, intents)) {
            int n = pdf_array_len(ctx, intents);
            pdf_obj* list = pdf_new_array(ctx, doc, n);
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
        pdf_obj* xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");
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
        pdf_obj* pageLabels = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/PageLabels");
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
    CrashIf(pdf_js_supported(ctx, doc));

    // TODO: better implementation
    // we use this to check if has unsaved annotations to show a 'unsaved annotations'
    // message on close. reset this the case of damaged documents that
    // were fixed up by mupdf. Hopefully this doesn't mess something else
    doc->dirty = 0;
    return true;
}

PageDestination* destFromAttachment(EnginePdf* engine, fz_outline* outline) {
    PageDestination* dest = new PageDestination();
    dest->kind = kindDestinationLaunchEmbedded;
    // WCHAR* path = strconv::Utf8ToWstr(outline->uri);
    dest->name = strconv::Utf8ToWstr(outline->title);
    // page is really a stream number
    dest->value = str::Format(L"%s:%d", engine->FileName(), outline->page);
    return dest;
}

TocItem* EnginePdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment) {
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    while (outline) {
        WCHAR* name = nullptr;
        if (outline->title) {
            name = strconv::Utf8ToWstr(outline->title);
            name = pdf_clean_string(name);
        }
        if (!name) {
            name = str::Dup(L"");
        }
        int pageNo = outline->page + 1;

        PageDestination* dest = nullptr;
        Kind kindRaw = nullptr;
        if (isAttachment) {
            kindRaw = kindTocFzOutlineAttachment;
            dest = destFromAttachment(this, outline);
        } else {
            kindRaw = kindTocFzOutline;
            dest = newFzDestination(outline);
        }

        TocItem* item = newTocItemWithDestination(parent, name, dest);
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
            item->color = FromPdfColor(ctx, outline->n_color, outline->color);
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
TocTree* EnginePdf::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (outline == nullptr && attachments == nullptr) {
        return nullptr;
    }

    int idCounter = 0;

    TocItem* root = nullptr;
    if (outline) {
        root = BuildTocTree(nullptr, outline, idCounter, false);
    }
    if (!attachments) {
        if (!root) {
            return nullptr;
        }
        tocTree = new TocTree(root);
        return tocTree;
    }
    TocItem* att = BuildTocTree(nullptr, attachments, idCounter, true);
    if (!root) {
        tocTree = new TocTree(att);
        return tocTree;
    }
    root->AddSiblingAtEnd(att);
    tocTree = new TocTree(root);
    return tocTree;
}

PageDestination* EnginePdf::GetNamedDest(const WCHAR* name) {
    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    pdf_document* doc = (pdf_document*)_doc;

    AutoFree name_utf8(strconv::WstrToUtf8(name));
    pdf_obj* dest = nullptr;

    fz_var(dest);
    fz_try(ctx) {
        pdf_obj* nameobj = pdf_new_string(ctx, name_utf8.Get(), (int)name_utf8.size());
        dest = pdf_lookup_dest(ctx, doc, nameobj);
        pdf_drop_obj(ctx, nameobj);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!dest) {
        return nullptr;
    }

    PageDestination* pageDest = nullptr;
    char* uri = nullptr;

    fz_var(uri);
    fz_try(ctx) {
        uri = pdf_parse_link_dest(ctx, doc, dest);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!uri) {
        return nullptr;
    }

    float x, y;
    int pageNo = resolve_link(uri, &x, &y);

    RectF r{x, y, 0, 0};
    pageDest = newSimpleDest(pageNo, r);
    fz_free(ctx, uri);
    return pageDest;
}

// return a page but only if is fully loaded
FzPageInfo* EnginePdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = &_pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static PageElement* newFzComment(const WCHAR* comment, int pageNo, RectF rect) {
    auto res = new PageElement();
    res->kind_ = kindPageElementComment;
    res->pageNo = pageNo;
    res->rect = rect;
    res->value = str::Dup(comment);
    return res;
}

static PageElement* makePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_annot_rect(ctx, annot);
    auto tp = pdf_annot_type(ctx, annot);
    const char* contents = pdf_annot_contents(ctx, annot);
    const char* label = pdf_field_label(ctx, annot->obj);
    const char* s = contents;
    // TODO: use separate classes for comments and tooltips?
    if (str::IsEmpty(contents) && PDF_ANNOT_WIDGET == tp) {
        s = label;
    }
    AutoFreeWstr ws = strconv::Utf8ToWstr(s);
    RectF rd = ToRectFl(rect);
    return newFzComment(ws, pageNo, rd);
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
        const char* label = pdf_field_label(ctx, annot->obj); // don't free
        bool isLabelEmpty = str::IsEmpty(label);
        int flags = pdf_field_flags(ctx, annot->obj);

        if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
            dbglogf("found file attachment annotation\n");

            pdf_obj* fs = pdf_dict_get(ctx, annot->obj, PDF_NAME(FS));
            const char* attname = pdf_embedded_file_name(ctx, fs);
            fz_rect rect = pdf_annot_rect(ctx, annot);
            if (str::IsEmpty(attname) || fz_is_empty_rect(rect) || !pdf_is_embedded_file(ctx, fs)) {
                continue;
            }

            dbglogf("attachement: %s\n", attname);

            PageElement* el = new PageElement();
            el->kind_ = kindPageElementDest;
            el->pageNo = pageNo;
            el->rect = ToRectFl(rect);
            el->value = strconv::Utf8ToWstr(attname);
            el->dest = new PageDestination();
            el->dest->kind = kindDestinationLaunchEmbedded;
            el->dest->value = strconv::Utf8ToWstr(attname);
            el->dest->pageNo = pageNo;
            comments.Append(el);
            // TODO: need to implement https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
            // for saving the attachment to a file
            // TODO: expose /Contents in addition to the file path
            continue;
        }

        if (!isContentsEmpty && tp != PDF_ANNOT_FREE_TEXT) {
            auto comment = makePdfCommentFromPdfAnnot(ctx, pageNo, annot);
            comments.Append(comment);
            continue;
        }

        if (PDF_ANNOT_WIDGET == tp && !isLabelEmpty) {
            if (!(flags & PDF_FIELD_IS_READ_ONLY)) {
                auto comment = makePdfCommentFromPdfAnnot(ctx, pageNo, annot);
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
FzPageInfo* EnginePdf::GetFzPageInfo(int pageNo, bool loadQuick) {
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
    MakePageElementCommentsFromAnnotations(ctx, pageInfo);
    if (!stext) {
        return pageInfo;
    }

    FzLinkifyPageText(pageInfo, stext);
    fz_find_image_positions(ctx, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectF EnginePdf::PageMediabox(int pageNo) {
    FzPageInfo* pi = &_pages[pageNo - 1];
    return pi->mediabox;
}

RectF EnginePdf::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);

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

RectF EnginePdf::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    if (zoom <= 0) {
        char* name = str::Dup("");
        const WCHAR* nameW = FileName();
        if (nameW) {
            name = (char*)strconv::WstrToUtf8(nameW).data();
        }
        logf("doc: %s, pageNo: %d, zoom: %.2f\n", name, pageNo, zoom);
        free(name);
    }
    CrashIf(zoom <= 0);
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = To_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return ToRectFl(rect2);
}

RenderedBitmap* EnginePdf::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;
    pdf_page* pdfpage = pdf_page_from_fz_page(ctx, page);

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

    fz_colorspace* colorspace = fz_device_rgb(ctx);
    fz_irect ibounds = bbox;
    fz_rect cliprect = fz_rect_from_irect(bbox);

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

    fz_try(ctx) {
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
        // initialize with white background
        fz_clear_pixmap_with_value(ctx, pix, 0xff);
        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);
        pdf_run_page_with_usage(ctx, doc, pdfpage, dev, ctm, usage, fzcookie);
        bitmap = new_rendered_fz_pixmap(ctx, pix);
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
    return bitmap;
}

IPageElement* EnginePdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

Vec<IPageElement*>* EnginePdf::GetElements(int pageNo) {
    auto pageInfo = GetFzPageInfoFast(pageNo);
    auto res = new Vec<IPageElement*>();
    FzGetElements(res, pageInfo);
    if (res->IsEmpty()) {
        delete res;
        return nullptr;
    }
    return res;
}

RenderedBitmap* EnginePdf::GetImageForPageElement(IPageElement* ipel) {
    PageElement* pel = (PageElement*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

bool EnginePdf::SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots) {
    return SaveFileAs(pdfFileName, includeUserAnnots);
}

bool EnginePdf::BenchLoadPage(int pageNo) {
    return GetFzPageInfo(pageNo, false) != nullptr;
}

fz_matrix EnginePdf::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = To_fz_rect(PageMediabox(pageNo));
    return fz_create_view_ctm(tmpRc, zoom, rotation);
}

fz_matrix EnginePdf::viewctm(fz_page* page, float zoom, int rotation) {
    return fz_create_view_ctm(fz_bound_page(ctx, page), zoom, rotation);
}

RenderedBitmap* EnginePdf::GetPageImage(int pageNo, RectF rect, int imageIdx) {
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

    fz_image* image = fz_find_image_at_idx(ctx, pageInfo, imageIdx);
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
        bmp = new_rendered_fz_pixmap(ctx, pixmap);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    return bmp;
}

PageText EnginePdf::ExtractPageText(int pageNo) {
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
    WCHAR* text = fz_text_page_to_str(stext, &res.coords);
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

WCHAR* EnginePdf::ExtractFontList() {
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
            info.Append(strconv::ToMultiByte(name, 936, CP_UTF8).data());
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

        AutoFreeWstr fontInfo = strconv::Utf8ToWstr(info.LendData());
        if (fontInfo && !fonts.Contains(fontInfo)) {
            fonts.Append(fontInfo.StealData());
        }
    }
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR* EnginePdf::GetProperty(DocumentProperty prop) {
    if (!_doc) {
        return nullptr;
    }

    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

    if (DocumentProperty::PdfVersion == prop) {
        int major = doc->version / 10, minor = doc->version % 10;
        pdf_crypt* crypt = doc->crypt;
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
            WCHAR* s = pdf_to_wstr(ctx, obj);
            return pdf_clean_string(s);
        }
    }
    return nullptr;
};

std::span<u8> EnginePdf::GetFileData() {
    std::span<u8> res;
    ScopedCritSec scope(ctxAccess);

    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

    fz_var(res);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, doc->file);
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
bool EnginePdf::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.AsSpan());
        return ok;
    }
    auto path = FileName();
    if (!path) {
        return false;
    }
    bool ok = CopyFileW(path, dstPath, FALSE);
    if (!ok) {
        return false;
    }
    return true;
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
bool EnginePdfSaveUpdated(EngineBase* engine, std::string_view path) {
    CrashIf(!engine);
    if (!engine) {
        return false;
    }
    EnginePdf* enginePdf = (EnginePdf*)engine;
    strconv::StackWstrToUtf8 currPath = engine->FileName();
    if (path.empty()) {
        path = {currPath.Get()};
    }
    fz_context* ctx = enginePdf->ctx;
    pdf_document* doc = pdf_document_from_fz_document(ctx, enginePdf->_doc);

    pdf_write_options save_opts;
    save_opts = pdf_default_write_options2;
    save_opts.do_incremental = 1;
    save_opts.do_compress = 1;
    save_opts.do_compress_images = 1;
    save_opts.do_compress_fonts = 1;
    if (doc->redacted) {
        save_opts.do_garbage = 1;
    }

    bool ok = true;
    fz_try(ctx) {
        pdf_save_document(ctx, doc, path.data(), &save_opts);
    }
    fz_catch(ctx) {
        const char* errMsg = fz_caught_message(enginePdf->ctx);
        logf("Pdf save of '%s' failed with '%s'\n", path.data(), errMsg);
        // TODO: show error message
        ok = false;
    }
    return ok;
}

// https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
bool EnginePdf::SaveEmbedded(LinkSaverUI& saveUI, int num) {
    ScopedCritSec scope(ctxAccess);
    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

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

bool EnginePdf::HasClipOptimizations(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo || !pageInfo->page) {
        return false;
    }

    fz_rect mbox = To_fz_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_calc_overlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

WCHAR* EnginePdf::GetPageLabel(int pageNo) const {
    if (!_pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    return str::Dup(_pageLabels->at(pageNo - 1));
}

int EnginePdf::GetPageByLabel(const WCHAR* label) const {
    int pageNo = 0;
    if (_pageLabels) {
        pageNo = _pageLabels->Find(label) + 1;
    }

    if (!pageNo) {
        return EngineBase::GetPageByLabel(label);
    }

    return pageNo;
}

// in Annotation.cpp
extern Annotation* MakeAnnotationPdf(CRITICAL_SECTION* ctxAccess, fz_context* ctx, pdf_page* page, pdf_annot* annot,
                                     int pageNo);

int EnginePdf::GetAnnotations(Vec<Annotation*>* annotsOut) {
    int nAnnots = 0;
    for (int i = 1; i <= pageCount; i++) {
        auto pi = GetFzPageInfo(i, true);
        pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pi->page);
        pdf_annot* annot = pdf_first_annot(ctx, pdfpage);
        while (annot) {
            Annotation* a = MakeAnnotationPdf(ctxAccess, ctx, pdfpage, annot, i);
            if (a) {
                annotsOut->Append(a);
                nAnnots++;
            }
            annot = pdf_next_annot(ctx, annot);
        }
    }
    return nAnnots;
}

EngineBase* EnginePdf::CreateFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    if (str::IsEmpty(path)) {
        return nullptr;
    }
    EnginePdf* engine = new EnginePdf();
    if (!engine->Load(path, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EnginePdf::CreateFromStream(IStream* stream, PasswordUI* pwdUI) {
    EnginePdf* engine = new EnginePdf();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsPdfEngineSupportedFileType(Kind kind) {
    return kind == kindFilePDF;
}

EngineBase* CreateEnginePdfFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    return EnginePdf::CreateFromFile(path, pwdUI);
}

EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI) {
    return EnginePdf::CreateFromStream(stream, pwdUI);
}

static const char* getuser(void) {
    const char* u;
    u = getenv("USER");
    if (!u)
        u = getenv("USERNAME");
    if (!u)
        u = "user";
    return u;
}

Annotation* EnginePdfCreateAnnotation(EngineBase* engine, AnnotationType typ, int pageNo, PointF pos) {
    CrashIf(engine->kind != kindEnginePdf);
    EnginePdf* epdf = (EnginePdf*)engine;
    fz_context* ctx = epdf->ctx;

    auto pageInfo = epdf->GetFzPageInfo(pageNo, true);

    ScopedCritSec cs(epdf->ctxAccess);

    auto page = pdf_page_from_fz_page(ctx, pageInfo->page);
    enum pdf_annot_type atyp = (enum pdf_annot_type)typ;

    auto annot = pdf_create_annot(ctx, page, atyp);

    pdf_set_annot_modification_date(ctx, annot, time(NULL));
    if (pdf_annot_has_author(ctx, annot)) {
        pdf_set_annot_author(ctx, annot, getuser());
    }

    switch (typ) {
        case AnnotationType::Text:
        case AnnotationType::FreeText:
        case AnnotationType::Stamp:
        case AnnotationType::Caret:
        case AnnotationType::Square:
        case AnnotationType::Circle: {
            fz_rect trect = pdf_annot_rect(ctx, annot);
            float dx = trect.x1 - trect.x0;
            trect.x0 = pos.x;
            trect.x1 = trect.x0 + dx;
            float dy = trect.y1 - trect.y0;
            trect.y0 = pos.y;
            trect.y1 = trect.y0 + dy;
            pdf_set_annot_rect(ctx, annot, trect);
        } break;
        case AnnotationType::Line: {
            fz_point a{pos.x, pos.y};
            fz_point b{pos.x + 100, pos.y + 50};
            pdf_set_annot_line(ctx, annot, a, b);
        } break;
    }
    if (typ == AnnotationType::FreeText) {
        pdf_set_annot_contents(ctx, annot, "This is a text...");
        pdf_set_annot_border(ctx, annot, 1);
    }

    pdf_update_appearance(ctx, annot);
    auto res = MakeAnnotationPdf(epdf->ctxAccess, ctx, page, annot, pageNo);
    return res;
}

int EnginePdfGetAnnotations(EngineBase* engine, Vec<Annotation*>* annotsOut) {
    CrashIf(engine->kind != kindEnginePdf);
    EnginePdf* epdf = (EnginePdf*)engine;
    return epdf->GetAnnotations(annotsOut);
}

bool EnginePdfHasUnsavedAnnotations(EngineBase* engine) {
    if (!engine || engine->kind != kindEnginePdf) {
        return false;
    }
    EnginePdf* epdf = (EnginePdf*)engine;
    pdf_document* pdfdoc = pdf_document_from_fz_document(epdf->ctx, epdf->_doc);
    return pdfdoc->dirty;
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

Annotation* EnginePdfGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, AnnotationType* allowedAnnots) {
    if (!engine || engine->kind != kindEnginePdf) {
        return nullptr;
    }
    EnginePdf* epdf = (EnginePdf*)engine;
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
        return MakeAnnotationPdf(epdf->ctxAccess, epdf->ctx, pdfpage, matched, pageNo);
    }
    return nullptr;
}
