/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectD Transform(const RectD& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    WCHAR* ExtractPageText(int pageNo, Rect** coordsOut = nullptr) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointD pt) override;
    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    int GetAnnotations(Vec<Annotation*>* annotsOut) override;

    static EngineBase* CreateFromFile(const WCHAR* path, PasswordUI* pwdUI);
    static EngineBase* CreateFromStream(IStream* stream, PasswordUI* pwdUI);

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;

    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    RenderedBitmap* GetPageImage(int pageNo, RectD rect, int imageIdx);

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo*> _pages;
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
    void MakePageElementCommentsFromAnnotations(FzPageInfo* pageInfo);
    WCHAR* ExtractFontList();
    bool IsLinearizedFile();

    bool SaveUserAnnots(const char* pathUtf8);
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

// in mupdf_load_system_font.c
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

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
    supportsAnnotations = true;
    supportsAnnotationsForSaving = true;
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

    for (auto* pi : _pages) {
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
    }

    DeleteVecMembers(_pages);

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
    unsigned char* cryptKey = nullptr;

  public:
    explicit PasswordCloner(unsigned char* cryptKey) {
        this->cryptKey = cryptKey;
    }

    WCHAR* GetPassword(const WCHAR* fileName, unsigned char* fileDigest, unsigned char decryptionKeyOut[32],
                       bool* saveKey) override {
        UNUSED(fileName);
        UNUSED(fileDigest);
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

    clone->SetUserAnnotations(userAnnots);

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

    unsigned char digest[16 + 32] = {0};
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

    // this does the job of pdf_bound_page but without doing pdf_load_page()
    // TODO: time pdf_load_page(), maybe it's not slow?
    for (int i = 0; i < pageCount; i++) {
        fz_rect mbox{};
        fz_matrix page_ctm{};

        fz_try(ctx) {
            pdf_obj* pageref = pdf_lookup_page_obj(ctx, doc, i);
            pdf_page_obj_transform(ctx, pageref, &mbox, &page_ctm);
            mbox = fz_transform_rect(mbox, page_ctm);
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
        FzPageInfo* pageInfo = new FzPageInfo();
        pageInfo->mediabox = fz_rect_to_RectD(mbox);
        pageInfo->pageNo = i + 1;
        _pages.Append(pageInfo);
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
        if (IsLinearizedFile()) {
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

// TODO: maybe build in FinishDownload
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

    RectD r{x, y, 0, 0};
    pageDest = newSimpleDest(pageNo, r);
    fz_free(ctx, uri);
    return pageDest;
}

// return a page but only if is fully loaded
FzPageInfo* EnginePdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    auto pageInfo = _pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
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
    FzPageInfo* pageInfo = _pages[pageIdx];

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
    MakePageElementCommentsFromAnnotations(pageInfo);
    if (!stext) {
        return pageInfo;
    }

    FzLinkifyPageText(pageInfo, stext);
    fz_find_image_positions(ctx, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectD EnginePdf::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectD EnginePdf::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie = {};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    fz_display_list* list = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);
    fz_var(list);

    RectD mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        list = fz_new_display_list_from_page(ctx, pageInfo->page);
        dev = fz_new_bbox_device(ctx, &rect);
        fz_run_display_list(ctx, list, dev, fz_identity, pagerect, &fzcookie);
        fz_close_device(ctx, dev);
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

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectD rect2 = fz_rect_to_RectD(rect);
    return rect2.Intersect(mediabox);
}

RectD EnginePdf::Transform(const RectD& rect, int pageNo, float zoom, int rotation, bool inverse) {
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
    fz_rect rect2 = RectD_to_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap* EnginePdf::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;
    pdf_page* pdfpage = pdf_page_from_fz_page(ctx, page);

    int transparency = pdfpage->transparency;

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
        pRect = RectD_to_fz_rect(*pageRect);
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
    fz_display_list* list = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);
    fz_var(list);

    Vec<Annotation*> annots = FilterAnnotationsForPage(userAnnots, pageNo);

    fz_try(ctx) {
        list = fz_new_display_list_from_page(ctx, page);
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
        // initialize with white background
        fz_clear_pixmap_with_value(ctx, pix, 0xff);

        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        // TODO: use fz_infinite_rect instead of cliprect?
        fz_run_page_transparency(ctx, &annots, dev, cliprect, false, transparency);
        fz_run_display_list(ctx, list, dev, ctm, cliprect, fzcookie);
        fz_run_page_transparency(ctx, &annots, dev, cliprect, true, transparency);
        fz_run_user_page_annots(ctx, &annots, dev, ctm, cliprect, fzcookie);
        bitmap = new_rendered_fz_pixmap(ctx, pix);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        if (dev) {
            fz_drop_device(ctx, dev);
        }
        fz_drop_pixmap(ctx, pix);
        if (list) {
            fz_drop_display_list(ctx, list);
        }
    }
    fz_catch(ctx) {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

IPageElement* EnginePdf::GetElementAtPos(int pageNo, PointD pt) {
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
    const fz_rect tmpRc = RectD_to_fz_rect(PageMediabox(pageNo));
    return fz_create_view_ctm(tmpRc, zoom, rotation);
}

fz_matrix EnginePdf::viewctm(fz_page* page, float zoom, int rotation) {
    return fz_create_view_ctm(fz_bound_page(ctx, page), zoom, rotation);
}

void EnginePdf::MakePageElementCommentsFromAnnotations(FzPageInfo* pageInfo) {
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
            el->rect = fz_rect_to_RectD(rect);
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

RenderedBitmap* EnginePdf::GetPageImage(int pageNo, RectD rect, int imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= images.isize();
    fz_rect imgRect = images.at(imageIdx).rect;
    bool badRect = fz_rect_to_RectD(imgRect) != rect;
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

WCHAR* EnginePdf::ExtractPageText(int pageNo, Rect** coordsOut) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);

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
        return nullptr;
    }
    WCHAR* text = fz_text_page_to_str(stext, coordsOut);
    fz_drop_stext_page(ctx, stext);
    return text;
}

bool EnginePdf::IsLinearizedFile() {
    ScopedCritSec scope(ctxAccess);
    // determine the object number of the very first object in the file
    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);
    fz_seek(ctx, doc->file, 0, 0);
    int tok = pdf_lex(ctx, doc->file, &doc->lexbuf.base);
    if (tok != PDF_TOK_INT) {
        return false;
    }
    int num = doc->lexbuf.base.i;
    if (num < 0 || num >= pdf_xref_len(ctx, doc)) {
        return false;
    }
    // check whether it's a linearization dictionary
    fz_try(ctx) {
        pdf_cache_object(ctx, doc, num);
    }
    fz_catch(ctx) {
        return false;
    }
    pdf_obj* obj = pdf_get_xref_entry(ctx, doc, num)->obj;
    if (!pdf_is_dict(ctx, obj)) {
        return false;
    }
    // /Linearized format must be version 1.0
    if (pdf_to_real(ctx, pdf_dict_gets(ctx, obj, "Linearized")) != 1.0f) {
        return false;
    }
    // /L must be the exact file size
    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "L")) != doc->file_size) {
        return false;
    }

    // /O must be the object number of the first page
    // TODO(port): at this point we don't have _pages loaded yet. for now always return false here
    auto fzpage = _pages[0]->page;
    if (!fzpage) {
        return false;
    }
    pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);

    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "O")) != pdf_to_num(ctx, page->obj)) {
        return false;
    }

    // /N must be the total number of pages
    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "N")) != PageCount()) {
        return false;
    }
    // /H must be an array and /E and /T must be integers
    bool ok = pdf_is_array(ctx, pdf_dict_gets(ctx, obj, "H"));
    if (!ok) {
        return false;
    }
    ok = pdf_is_int(ctx, pdf_dict_gets(ctx, obj, "E"));
    if (!ok) {
        return false;
    }
    ok = pdf_is_int(ctx, pdf_dict_gets(ctx, obj, "T"));
    return ok;
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
                if (annot->ap) {
                    pdf_obj* o = annot->ap;
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

bool EnginePdf::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.AsSpan());
        if (ok) {
            return !includeUserAnnots || SaveUserAnnots(copyFileName);
        }
    }
    auto path = FileName();
    if (!path) {
        return false;
    }
    bool ok = CopyFileW(path, dstPath, FALSE);
    if (!ok) {
        return false;
    }
    // TODO: try to recover when SaveUserAnnots fails?
    return !includeUserAnnots || SaveUserAnnots(copyFileName);
}

#if 0
static bool pdf_file_update_add_annotation(fz_context* ctx, pdf_document* doc, pdf_page* page, pdf_obj* page_obj,
                                           PageAnnotation& annot, pdf_obj* annots) {
    static const char* obj_dict =
        "<<\
    /Type /Annot /Subtype /%s\
    /Rect [%f %f %f %f]\
    /C [%f %f %f]\
    /F %d\
    /P %d %d R\
    /QuadPoints %s\
    /AP << >>\
>>";
    static const char* obj_quad_tpl = "[%f %f %f %f %f %f %f %f]";
    static const char* ap_dict =
        "<< /Type /XObject /Subtype /Form /BBox [0 0 %f %f] /Resources << /ExtGState << /GS << /Type /ExtGState "
        "/ca "
        "%.f /AIS false /BM /Multiply >> >> /ProcSet [/PDF] >> >>";
    static const char* ap_highlight = "q /DeviceRGB cs /GS gs %f %f %f rg 0 0 %f %f re f Q\n";
    static const char* ap_underline = "q /DeviceRGB CS %f %f %f RG 1 w [] 0 d 0 0.5 m %f 0.5 l S Q\n";
    static const char* ap_strikeout = "q /DeviceRGB CS %f %f %f RG 1 w [] 0 d 0 %f m %f %f l S Q\n";
    static const char* ap_squiggly =
        "q /DeviceRGB CS %f %f %f RG 0.5 w [1] 1.5 d 0 0.25 m %f 0.25 l S [1] 0.5 d 0 0.75 m %f 0.75 l S Q\n";

    pdf_obj *annot_obj = nullptr, *ap_obj = nullptr;
    fz_buffer* ap_buf = nullptr;

    fz_var(annot_obj);
    fz_var(ap_obj);
    fz_var(ap_buf);

    const char* subtype = PageAnnotType::Highlight == annot.type
                              ? "Highlight"
                              : PageAnnotType::Underline == annot.type
                                    ? "Underline"
                                    : PageAnnotType::StrikeOut == annot.type
                                          ? "StrikeOut"
                                          : PageAnnotType::Squiggly == annot.type ? "Squiggly" : nullptr;
    CrashIf(!subtype);
    int rotation = (page->rotate + 360) % 360;
    CrashIf((rotation % 90) != 0);
    // convert the annotation's rectangle back to raw user space
    fz_rect r = RectD_to_fz_rect(annot.rect);
    fz_matrix invctm = fz_invert_matrix(page->ctm);
    fz_transform_rect(&r, invctm);
    double dx = r.x1 - r.x0, dy = r.y1 - r.y0;
    if ((rotation % 180) == 90)
        std::swap(dx, dy);
    float rgb[3] = {annot.color.r / 255.f, annot.color.g / 255.f, annot.color.b / 255.f};
    // rotate the QuadPoints to match the page
    AutoFree quad_tpl;
    if (0 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y1, r.x1, r.y1, r.x0, r.y0, r.x1, r.y0));
    else if (90 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y0, r.x0, r.y1, r.x1, r.y0, r.x1, r.y1));
    else if (180 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y0, r.x0, r.y0, r.x1, r.y1, r.x0, r.y1));
    else // if (270 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y1, r.x1, r.y0, r.x0, r.y1, r.x0, r.y0));
    AutoFree annot_tpl(str::Format(obj_dict, subtype, r.x0, r.y0, r.x1, r.y1, rgb[0], rgb[1],
                                   rgb[2],                                              // Rect and Color
                                   F_Print, pdf_to_num(ctx, page_obj), pdf_to_gen(ctx, page_obj), // F and P
                                   quad_tpl.Get()));
    AutoFree annot_ap_dict(str::Format(ap_dict, dx, dy, annot.color.a / 255.f));
    AutoFree annot_ap_stream;

    fz_try(ctx) {
        annot_obj = pdf_new_obj_from_str(ctx, doc, annot_tpl);
        // append the annotation to the file
        pdf_array_push_drop(ctx, annots, pdf_new_ref(doc, annot_obj));
    }
    fz_catch(ctx) {
        pdf_drop_obj(ctx, annot_obj);
        return false;
    }

    if (doc->crypt) {
        // since we don't encrypt the appearance stream, for encrypted documents
        // the readers will have to synthesize an appearance stream themselves
        pdf_drop_obj(ctx, annot_obj);
        return true;
    }

    fz_try(ctx) {
        // create the appearance stream (unencrypted) and append it to the file
        ap_obj = pdf_new_obj_from_str(ctx, doc, annot_ap_dict);
        switch (annot.type) {
            case PageAnnotType::Highlight:
                annot_ap_stream.Set(str::Format(ap_highlight, rgb[0], rgb[1], rgb[2], dx, dy));
                break;
            case PageAnnotType::Underline:
                annot_ap_stream.Set(str::Format(ap_underline, rgb[0], rgb[1], rgb[2], dx));
                break;
            case PageAnnotType::StrikeOut:
                annot_ap_stream.Set(str::Format(ap_strikeout, rgb[0], rgb[1], rgb[2], dy / 2, dx, dy / 2));
                break;
            case PageAnnotType::Squiggly:
                annot_ap_stream.Set(str::Format(ap_squiggly, rgb[0], rgb[1], rgb[2], dx, dx));
                break;
        }
        if (annot.type != PageAnnotType::Highlight)
            pdf_dict_dels(ctx, pdf_dict_gets(ctx, ap_obj, "Resources"), "ExtGState");
        if (rotation) {
            pdf_dict_puts_drop(ctx, ap_obj, "Matrix", pdf_new_matrix(ctx, doc, fz_rotate(rotation)));
        }
        ap_buf = fz_new_buffer(ctx, (int)str::Len(annot_ap_stream));
        memcpy(ap_buf->data, annot_ap_stream, (ap_buf->len = (int)str::Len(annot_ap_stream)));
        pdf_dict_puts_drop(ctx, ap_obj, "Length", pdf_new_int(ctx, doc, ap_buf->len));
        // append the appearance stream to the file
        int num = pdf_create_object(ctx, doc);
        pdf_update_object(ctx, doc, num, ap_obj);
        pdf_update_stream(ctx, doc, num, ap_buf);
        pdf_dict_puts_drop(ctx, pdf_dict_gets(ctx, annot_obj, "AP"), "N", pdf_new_indirect(ctx, doc, num, 0));
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, ap_obj);
        fz_drop_buffer(ctx, ap_buf);
        pdf_drop_obj(ctx, annot_obj);
    }
    fz_catch(ctx) { return false; }
    return true;
}
#endif

static enum pdf_annot_type PageAnnotTypeToPdf(AnnotationType tp) {
    switch (tp) {
        case AnnotationType::Highlight:
            return PDF_ANNOT_HIGHLIGHT;
        case AnnotationType::Squiggly:
            return PDF_ANNOT_SQUIGGLY;
        case AnnotationType::Underline:
            return PDF_ANNOT_UNDERLINE;
        case AnnotationType::StrikeOut:
            return PDF_ANNOT_SQUIGGLY;
    }
    return PDF_ANNOT_UNKNOWN;
}

static void add_user_annotation(fz_context* ctx, pdf_document* doc, pdf_page* page, Annotation* userAnnot) {
    enum pdf_annot_type tp = PageAnnotTypeToPdf(userAnnot->Type());
    pdf_annot* annot = pdf_create_annot(ctx, page, tp);

    fz_rect r = RectD_to_fz_rect(userAnnot->Rect());

    // TODO: not sure if this is needed
#if 0
    // rotate the QuadPoints to match the page
    AutoFree quad_tpl;
    if (0 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y1, r.x1, r.y1, r.x0, r.y0, r.x1, r.y0));
    else if (90 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y0, r.x0, r.y1, r.x1, r.y0, r.x1, r.y1));
    else if (180 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y0, r.x0, r.y0, r.x1, r.y1, r.x0, r.y1));
    else // if (270 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y1, r.x1, r.y0, r.x0, r.y1, r.x0, r.y0));
#endif
    fz_quad quad = fz_make_quad(r.x0, r.y1, r.x1, r.y1, r.x0, r.y0, r.x1, r.y0);
    pdf_add_annot_quad_point(ctx, annot, quad);

    // TODO: not sure if rect is needed for annotations
    // maybe only for some annotations but not highlight?
#if 0
    fz_rect r = RectD_to_fz_rect(userAnnot.rect);
    pdf_set_annot_rect(ctx, annot, r);
#endif

    float col[4];
    ToPdfRgba(userAnnot->Color(), col);
    pdf_set_annot_color(ctx, annot, 3, col);
    pdf_set_annot_opacity(ctx, annot, col[3]);

    pdf_set_annot_modification_date(ctx, annot, time(NULL));
    pdf_update_appearance(ctx, annot);
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

bool EnginePdf::SaveUserAnnots(const char* pathUtf8) {
    if (!userAnnots) {
        return true;
    }
    int n = userAnnots->isize();
    // TODO: should only count non-deleted annotations
    if (n == 0) {
        return true;
    }

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    bool ok = true;
    Vec<Annotation*> pageAnnots;
    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);
    int nAdded = 0;

    int nPages = PageCount();
    fz_try(ctx) {
        for (int pageNo = 1; pageNo <= nPages; pageNo++) {
            FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
            pdf_page* page = pdf_page_from_fz_page(ctx, pageInfo->page);

            pageAnnots = FilterAnnotationsForPage(userAnnots, pageNo);
            if (pageAnnots.size() == 0) {
                continue;
            }

            for (auto&& annot : pageAnnots) {
                add_user_annotation(ctx, doc, page, annot);
                nAdded++;
            }
        }

        if (nAdded > 0) {
            pdf_write_options opts = {0};
            opts.do_incremental = 1;
            pdf_save_document(ctx, doc, const_cast<char*>(pathUtf8), &opts);
        }
    }
    fz_catch(ctx) {
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

    fz_rect mbox = RectD_to_fz_rect(PageMediabox(pageNo));
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
extern Annotation* MakeAnnotationPdf(fz_context* ctx, pdf_page* page, pdf_annot* annot, int pageNo);

int EnginePdf::GetAnnotations(Vec<Annotation*>* annotsOut) {
    int nAnnots = 0;
    for (int i = 1; i <= pageCount; i++) {
        auto pi = GetFzPageInfo(i, true);
        pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pi->page);
        pdf_annot* annot = pdf_first_annot(ctx, pdfpage);
        while (annot) {
            Annotation* a = MakeAnnotationPdf(ctx, pdfpage, annot, i);
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

Annotation* EnginePdfCreateAnnotation(EngineBase* engine, AnnotationType typ, int pageNo, PointD pos) {
    CrashIf(engine->kind != kindEnginePdf);
    EnginePdf* enginePdf = (EnginePdf*)engine;
    fz_context* ctx = enginePdf->ctx;

    auto pageInfo = enginePdf->GetFzPageInfo(pageNo, true);
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
            fz_rect trect = pdf_annot_rect(ctx, annot);
            float dx = trect.x1 - trect.x0;
            trect.x0 = pos.x;
            trect.x1 = trect.x0 + dx;
            float dy = trect.y1 - trect.y0;
            trect.y0 = pos.y;
            trect.y1 = trect.y0 + dy;
            pdf_set_annot_rect(ctx, annot, trect);
            break;
    }
    if (typ == AnnotationType::FreeText) {
        pdf_set_annot_contents(ctx, annot, "This is a text...");
        pdf_set_annot_border(ctx, annot, 1);
    }

    pdf_update_appearance(ctx, annot);
    auto res = MakeAnnotationPdf(ctx, page, annot, pageNo);
    return res;
}
