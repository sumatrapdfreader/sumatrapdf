/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

Kind kindEnginePdf = "enginePdf";

static fz_link* FixupPageLinks(fz_link* root) {
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    fz_link* new_root = nullptr;
    while (root) {
        fz_link* tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link* link = new_root;
        if (link->rect.x0 > link->rect.x1)
            std::swap(link->rect.x0, link->rect.x1);
        if (link->rect.y0 > link->rect.y1)
            std::swap(link->rect.y0, link->rect.y1);
        AssertCrash(link->rect.x1 >= link->rect.x0);
        AssertCrash(link->rect.y1 >= link->rect.y0);
    }
    return new_root;
}

static Vec<PageAnnotation> fz_get_user_page_annots(Vec<PageAnnotation>& userAnnots, int pageNo) {
    Vec<PageAnnotation> result;
    for (size_t i = 0; i < userAnnots.size(); i++) {
        PageAnnotation& annot = userAnnots.at(i);
        if (annot.pageNo != pageNo)
            continue;
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot.type) {
            case PageAnnotType::Highlight:
            case PageAnnotType::Underline:
            case PageAnnotType::StrikeOut:
            case PageAnnotType::Squiggly:
                result.Append(annot);
                break;
        }
    }
    return result;
}

// TODO(port)
#if 0
static void fz_run_user_page_annots(fz_context* ctx, Vec<PageAnnotation>& pageAnnots, fz_device* dev, fz_matrix ctm,
                                    const fz_rect* cliprect, fz_cookie* cookie) {
    for (size_t i = 0; i < pageAnnots.size() && (!cookie || !cookie->abort); i++) {
        PageAnnotation& annot = pageAnnots.at(i);
        // skip annotation if it isn't visible
        fz_rect rect = fz_RectD_to_rect(annot.rect);
        rect = fz_transform_rect(rect, ctm);
        if (cliprect && fz_is_empty_rect(fz_intersect_rect(rect, *cliprect))) {
            continue;
        }
        // prepare text highlighting path (cf. pdf_create_highlight_annot
        // and pdf_create_markup_annot in pdf_annot.c)
        fz_path* path = fz_new_path(ctx);
        fz_stroke_state* stroke = nullptr;
        switch (annot.type) {
            case PageAnnotType::Highlight:
                fz_moveto(ctx, path, annot.rect.TL().x, annot.rect.TL().y);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.TL().y);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.BR().y);
                fz_lineto(ctx, path, annot.rect.TL().x, annot.rect.BR().y);
                fz_closepath(ctx, path);
                break;
            case PageAnnotType::Underline:
                fz_moveto(ctx, path, annot.rect.TL().x, annot.rect.BR().y - 0.25f);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.BR().y - 0.25f);
                break;
            case PageAnnotType::StrikeOut:
                fz_moveto(ctx, path, annot.rect.TL().x, annot.rect.TL().y + annot.rect.dy / 2);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.TL().y + annot.rect.dy / 2);
                break;
            case PageAnnotType::Squiggly:
                fz_moveto(ctx, path, annot.rect.TL().x + 1, annot.rect.BR().y);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.BR().y);
                fz_moveto(ctx, path, annot.rect.TL().x, annot.rect.BR().y - 0.5f);
                fz_lineto(ctx, path, annot.rect.BR().x, annot.rect.BR().y - 0.5f);
                stroke = fz_new_stroke_state_with_dash_len(ctx, 2);
                CrashIf(!stroke);
                stroke->linewidth = 0.5f;
                stroke->dash_list[stroke->dash_len++] = 1;
                stroke->dash_list[stroke->dash_len++] = 1;
                break;
            default:
                CrashIf(true);
        }
        fz_colorspace* cs = fz_device_rgb(ctx);
        float color[3] = {annot.color.r / 255.f, annot.color.g / 255.f, annot.color.b / 255.f};
        if (PageAnnotType::Highlight == annot.type) {
            // render path with transparency effect
            fz_begin_group(ctx, dev, rect, nullptr, 0, 0, FZ_BLEND_MULTIPLY, 1.f);
            fz_fill_path(ctx, dev, path, 0, ctm, cs, color, annot.color.a / 255.f, fz_default_color_params);
            fz_end_group(ctx, dev);
        } else {
            if (!stroke)
                stroke = fz_new_stroke_state(ctx);
            fz_stroke_path(ctx, dev, path, stroke, ctm, cs, color, 1.0f, fz_default_color_params);
            fz_drop_stroke_state(ctx, stroke);
        }
        fz_drop_path(ctx, path);
    }
}
#endif

// TODO(port): not sure why it was here and if it's still needed
#if 0
static void fz_run_page_transparency(fz_context* ctx, Vec<PageAnnotation>& pageAnnots, fz_device* dev,
                                     const fz_rect* cliprect, bool endGroup, bool hasTransparency = false) {
    if (hasTransparency || pageAnnots.size() == 0)
        return;
    bool needsTransparency = false;
    for (size_t i = 0; i < pageAnnots.size(); i++) {
        if (PageAnnotType::Highlight == pageAnnots.at(i).type) {
            needsTransparency = true;
            break;
        }
    }
    if (!needsTransparency)
        return;
    if (!endGroup) {
        fz_rect r = cliprect ? *cliprect : fz_infinite_rect;
        fz_begin_group(ctx, dev, r, nullptr, 1, 0, 0, 1);
    } else
        fz_end_group(ctx, dev);
}
#endif

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

// Note: make sure to only call with ctxAccess
static fz_outline* pdf_load_attachments(fz_context* ctx, pdf_document* doc) {
    pdf_obj* dict = pdf_load_name_tree(ctx, doc, PDF_NAME(EmbeddedFiles));
    if (!dict)
        return nullptr;

    fz_outline root = {0}, *node = &root;
    for (int i = 0; i < pdf_dict_len(ctx, dict); i++) {
        pdf_obj* name = pdf_dict_get_key(ctx, dict, i);
        pdf_obj* dest = pdf_dict_get_val(ctx, dict, i);
        auto ef = pdf_dict_get(ctx, dest, PDF_NAME(EF));
        pdf_obj* embedded = pdf_dict_geta(ctx, ef, PDF_NAME(DOS), PDF_NAME(F));
        if (!embedded)
            continue;

        // TODO: in fz_try ?
        char* uri = pdf_parse_file_spec(ctx, doc, dest, nullptr);
        // undo the mangling done in pdf_parse_file_spec
        if (str::StartsWith(uri, "file://")) {
            char* prev = uri;
            uri = fz_strdup(ctx, uri + 7);
            fz_free(ctx, prev);
        }

        char* title = fz_strdup(ctx, pdf_to_name(ctx, name));
        int streamNo = pdf_to_num(ctx, embedded);
        fz_outline* link = fz_new_outline(ctx);

        link->uri = uri;
        link->title = title;
        // TODO: a hack: re-using page as stream number
        link->page = streamNo;

        node = node->next = link;
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
    if (str::Eq(type, "D"))
        return str::Format(L"%s%d", prefix, pageNo);
    if (str::EqI(type, "R")) {
        // roman numbering style
        AutoFreeWstr number(str::FormatRomanNumeral(pageNo));
        if (*type == 'r')
            str::ToLowerInPlace(number.Get());
        return str::Format(L"%s%s", prefix, number.get());
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::WStr number;
        number.Append('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++)
            number.Append(number.at(0));
        if (*type == 'a')
            str::ToLowerInPlace(number.Get());
        return str::Format(L"%s%s", prefix, number.Get());
    }
    return str::Dup(prefix);
}

void BuildPageLabelRec(fz_context* ctx, pdf_obj* node, int pageCount, Vec<PageLabelInfo>& data) {
    pdf_obj* obj;
    if ((obj = pdf_dict_gets(ctx, node, "Kids")) != nullptr && !pdf_mark_obj(ctx, node)) {
        for (int i = 0; i < pdf_array_len(ctx, obj); i++) {
            auto arr = pdf_array_get(ctx, obj, i);
            BuildPageLabelRec(ctx, arr, pageCount, data);
        }
        pdf_unmark_obj(ctx, node);
    } else if ((obj = pdf_dict_gets(ctx, node, "Nums")) != nullptr) {
        for (int i = 0; i < pdf_array_len(ctx, obj); i += 2) {
            pdf_obj* info = pdf_array_get(ctx, obj, i + 1);
            PageLabelInfo pli;
            pli.startAt = pdf_to_int(ctx, pdf_array_get(ctx, obj, i)) + 1;
            if (pli.startAt < 1)
                continue;

            pli.type = pdf_to_name(ctx, pdf_dict_gets(ctx, info, "S"));
            pli.prefix = pdf_dict_gets(ctx, info, "P");
            pli.countFrom = pdf_to_int(ctx, pdf_dict_gets(ctx, info, "St"));
            if (pli.countFrom < 1)
                pli.countFrom = 1;
            data.Append(pli);
        }
    }
}

WStrVec* BuildPageLabelVec(fz_context* ctx, pdf_obj* root, int pageCount) {
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(ctx, root, pageCount, data);
    data.Sort(CmpPageLabelInfo);

    size_t n = data.size();
    if (n == 0)
        return nullptr;

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
            size_t idx = pli.startAt + j - 1;
            free(labels->at(idx));
            labels->at(idx) = FormatPageLabel(pli.type, pli.countFrom + j, prefix);
        }
    }

    for (int idx = 0; (idx = labels->Find(nullptr, idx)) != -1; idx++) {
        labels->at(idx) = str::Dup(L"");
    }

    // ensure that all page labels are unique (by appending a number to duplicates)
    WStrVec dups(*labels);
    dups.Sort();
    for (size_t i = 1; i < dups.size(); i++) {
        if (!str::Eq(dups.at(i), dups.at(i - 1)))
            continue;
        int idx = labels->Find(dups.at(i)), counter = 0;
        while ((idx = labels->Find(dups.at(i), idx + 1)) != -1) {
            AutoFreeWstr unique;
            do {
                unique.Set(str::Format(L"%s.%d", dups.at(i), ++counter));
            } while (labels->Contains(unique));
            str::ReplacePtr(&labels->at(idx), unique);
        }
        for (; i + 1 < dups.size() && str::Eq(dups.at(i), dups.at(i + 1)); i++)
            ;
    }

    return labels;
}

void fz_find_images(fz_stext_page* text, Vec<FitzImagePos>& images) {
    if (!text) {
        return;
    }
    fz_stext_block* block = text->first_block;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_IMAGE) {
            block = block->next;
            continue;
        }
        FitzImagePos img = {block->u.i.image, block->bbox, block->u.i.transform};
        images.Append(img);
        block = block->next;
    }
}

struct PageTreeStackItem {
    pdf_obj* kids = nullptr;
    int i = -1;
    int len = 0;
    int next_page_no = 0;

    PageTreeStackItem(){};
    explicit PageTreeStackItem(fz_context* ctx, pdf_obj* kids, int next_page_no = 0)
        : kids(kids), i(-1), len(pdf_array_len(ctx, kids)), next_page_no(next_page_no) {
    }
};

///// Above are extensions to Fitz and MuPDF, now follows PdfEngine /////

class PdfTocItem;
class PdfLink;
class PdfImage;

class PdfEngineImpl : public EngineBase {
  public:
    PdfEngineImpl();
    virtual ~PdfEngineImpl();
    EngineBase* Clone() override;

    int PageCount() const override;

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

    static EngineBase* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI);
    static EngineBase* CreateFromStream(IStream* stream, PasswordUI* pwdUI);

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;

    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    RenderedBitmap* GetPageImage(int pageNo, RectD rect, size_t imageIx);

  protected:
    int pageCount = -1;

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo*> _pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;
    pdf_obj* _info = nullptr;
    WStrVec* _pageLabels = nullptr;

    Vec<PageAnnotation> userAnnots; // TODO(port): put in PageInfo

    DocTocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName, PasswordUI* pwdUI = nullptr);
    bool Load(IStream* stream, PasswordUI* pwdUI = nullptr);
    // TODO(port): fz_stream can no-longer be re-opened (fz_clone_stream)
    // bool Load(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool LoadFromStream(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool FinishLoading();

    fz_page* GetFzPage(int pageNo, bool failIfBusy = false);
    FzPageInfo* GetFzPageInfo(int pageNo, bool failIfBusy = false);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation);
    PdfTocItem* BuildTocTree(fz_outline* entry, int& idCounter, bool isAttachment);
    void LinkifyPageText(FzPageInfo* pageInfo);
    void ProcessPageAnnotations(FzPageInfo* pageInfo);
    WCHAR* ExtractFontList();
    bool IsLinearizedFile();

    bool SaveUserAnnots(const char* fileName);
};

class PdfLink : public PageElement, public PageDestination {
  public:
    PdfEngineImpl* engine = nullptr;
    // must be one or the other
    fz_link* link = nullptr;
    fz_outline* outline = nullptr;
    bool isAttachment = false;

    PdfLink(PdfEngineImpl* engine, int pageNo, fz_link* link, fz_outline* outline);

    // PageElement
    WCHAR* CalcValue() const;
    int CalcDestPageNo() const;
    PageDestType CalcDestType();
    RectD CalcDestRect();
    WCHAR* CalcDestName();
};

#if 0
static bool IsRelativeURI(const WCHAR* uri) {
    const WCHAR* c = uri;
    while (*c && *c != ':' && *c != '/' && *c != '?' && *c != '#') {
        c++;
    }
    return *c != ':';
}
#endif

PdfLink::PdfLink(PdfEngineImpl* engine, int pageNo, fz_link* link, fz_outline* outline) {
    this->engine = engine;
    elementPageNo = pageNo;
    CrashIf(!link && !outline);
    this->link = link;
    this->outline = outline;

    destType = CalcDestType();
    destRect = CalcDestRect();
    destValue = GetValue();
    destName = CalcDestName();
    destPageNo = CalcDestPageNo();

    elementType = PageElementType::Link;
    if (link) {
        elementRect = fz_rect_to_RectD(link->rect);
    }
    elementValue = CalcValue();
    elementDest = this;
}

static char* PdfLinkGetURI(const PdfLink* link) {
    if (link->link) {
        return link->link->uri;
    }
    if (link->outline) {
        return link->outline->uri;
    }
    return nullptr;
}

WCHAR* PdfLink::CalcDestName() {
    char* uri = PdfLinkGetURI(this);
    if (is_external_link(uri)) {
        return nullptr;
    }
    // TODO(port): test with more stuff
    // figure out what PDF_NAME(GoToR) ends up being
    return nullptr;
#if 0
    if (!link || FZ_LINK_GOTOR != link->kind || !link->ld.gotor.dest)
        return nullptr;
    return strconv::FromUtf8(link->ld.gotor.dest);
#endif
}

WCHAR* PdfLink::CalcValue() const {
    if (outline && isAttachment) {
        WCHAR* path = strconv::FromUtf8(outline->uri);
        return path;
    }

    char* uri = PdfLinkGetURI(this);
    if (!uri) {
        return nullptr;
    }
    if (!is_external_link(uri)) {
        // other values: #1,115,208
        return nullptr;
    }
    WCHAR* path = strconv::FromUtf8(uri);
    return path;
#if 0
    if (!link || !engine)
        return nullptr;
    if (link->kind != FZ_LINK_URI && link->kind != FZ_LINK_LAUNCH && link->kind != FZ_LINK_GOTOR)
        return nullptr;

    ScopedCritSec scope(&engine->ctxAccess);

    WCHAR* path = nullptr;

    switch (link->kind) {
        case FZ_LINK_URI:
            path = strconv::FromUtf8(link->ld.uri.uri);
            if (IsRelativeURI(path)) {
                AutoFreeWstr base;
                fz_try(engine->ctx) {
                    pdf_obj* obj = pdf_dict_gets(pdf_trailer(engine->_doc), "Root");
                    obj = pdf_dict_gets(pdf_dict_gets(obj, "URI"), "Base");
                    if (obj)
                        base.Set(PdfToWstr(obj));
                }
                fz_catch(engine->ctx) {}
                if (!str::IsEmpty(base.Get())) {
                    AutoFreeWstr uri(str::Join(base, path));
                    free(path);
                    path = uri.StealData();
                }
            }
            if (link->ld.uri.is_map) {
                int x = 0, y = 0;
                if (rect.Contains(pt)) {
                    x = (int)(pt.x - rect.x + 0.5);
                    y = (int)(pt.y - rect.y + 0.5);
                }
                AutoFreeWstr uri(str::Format(L"%s?%d,%d", path, x, y));
                free(path);
                path = uri.StealData();
            }
            break;
        case FZ_LINK_LAUNCH:
            // note: we (intentionally) don't support the /Win specific Launch parameters
            if (link->ld.launch.file_spec)
                path = strconv::FromUtf8(link->ld.launch.file_spec);
            if (path && link->ld.launch.embedded_num && str::EndsWithI(path, L".pdf")) {
                free(path);
                path = str::Format(L"%s:%d:%d", engine->FileName(), link->ld.launch.embedded_num,
                                   link->ld.launch.embedded_gen);
            }
            break;
        case FZ_LINK_GOTOR:
            if (link->ld.gotor.file_spec)
                path = strconv::FromUtf8(link->ld.gotor.file_spec);
            break;
    }

    return path;
#endif
}

#if 0
static PageDestType DestTypeFromName(const char* name) {
// named actions are converted either to Dest_Name or Dest_NameDialog
#define HandleType(type)      \
    if (str::Eq(name, #type)) \
    return PageDestType::##type
#define HandleTypeDialog(type) \
    if (str::Eq(name, #type))  \
    return PageDestType::##type##Dialog
    // predefined named actions
    HandleType(NextPage);
    HandleType(PrevPage);
    HandleType(FirstPage);
    HandleType(LastPage);
    // Adobe Reader extensions to the spec
    // cf. http://www.tug.org/applications/hyperref/manual.html
    HandleTypeDialog(Find);
    HandleType(FullScreen);
    HandleType(GoBack);
    HandleType(GoForward);
    HandleTypeDialog(GoToPage);
    HandleTypeDialog(Print);
    HandleTypeDialog(SaveAs);
    HandleTypeDialog(ZoomTo);
#undef HandleType
#undef HandleTypeDialog
    // named action that we don't support (or invalid action name)
    return PageDestType::None;
}
#endif

PageDestType PdfLink::CalcDestType() {
    if (outline && isAttachment) {
        return PageDestType::LaunchEmbedded;
    }

    char* uri = PdfLinkGetURI(this);
    // some outline entries are bad (issue 1245)
    if (!uri) {
        return PageDestType::None;
    }
    if (!is_external_link(uri)) {
        float x, y;
        int pageNo = resolve_link(uri, &x, &y);
        if (pageNo == -1) {
            // TODO: figure out what it could be
            CrashMePort();
            return PageDestType::None;
        }
        return PageDestType::ScrollTo;
    }
    if (str::StartsWith(uri, "file://")) {
        return PageDestType::LaunchFile;
    }
    if (str::StartsWithI(uri, "http://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWithI(uri, "https://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWithI(uri, "ftp://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWith(uri, "mailto:")) {
        return PageDestType::LaunchURL;
    }

    // TODO: PageDestType::LaunchEmbedded, PageDestType::LaunchURL, named destination
    CrashMePort();
    return PageDestType::None;
#if 0
    switch (link->kind) {
        case FZ_LINK_GOTO:
            return PageDestType::ScrollTo;
        case FZ_LINK_URI:
            return PageDestType::LaunchURL;
        case FZ_LINK_NAMED:
            return DestTypeFromName(link->ld.named.named);
        case FZ_LINK_LAUNCH:
            if (link->ld.launch.embedded_num)
                return PageDestType::LaunchEmbedded;
            if (link->ld.launch.is_uri)
                return PageDestType::LaunchURL;
            return PageDestType::LaunchFile;
        case FZ_LINK_GOTOR:
            return PageDestType::LaunchFile;
        default:
            return PageDestType::None; // unsupported action
    }
#endif
}

int PdfLink::CalcDestPageNo() const {
    char* uri = PdfLinkGetURI(this);
    CrashIf(!uri);
    if (!uri) {
        return 0;
    }
    if (is_external_link(uri)) {
        return 0;
    }
    float x, y;
    int pageNo = resolve_link(uri, &x, &y);
    if (pageNo == -1) {
        return 0;
    }
    return pageNo + 1; // TODO(port): or is it just pageNo?
#if 0
    if (link && FZ_LINK_GOTO == link->kind)
        return link->ld.gotor.page + 1;
    if (link && FZ_LINK_GOTOR == link->kind && !link->ld.gotor.dest)
        return link->ld.gotor.page + 1;
#endif
    return 0;
}

RectD PdfLink::CalcDestRect() {
    RectD result(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    char* uri = PdfLinkGetURI(this);
    CrashIf(!uri);
    if (!uri) {
        return result;
    }

    if (is_external_link(uri)) {
        return result;
    }
    float x, y;
    int pageNo = resolve_link(uri, &x, &y);
    if (pageNo == -1) {
        // SendCrashReportIf(pageNo == -1);
        return result;
    }

    result.x = (double)x;
    result.y = (double)y;
    return result;
#if 0
    if (!link || FZ_LINK_GOTO != link->kind && FZ_LINK_GOTOR != link->kind)
        return result;
    if (link->ld.gotor.page < 0 || link->ld.gotor.page >= engine->PageCount())
        return result;

    pdf_page* page = engine->GetFzPage(link->ld.gotor.page + 1);
    if (!page)
        return result;
    fz_point lt = link->ld.gotor.lt, rb = link->ld.gotor.rb;
    fz_transform_point(&lt, &page->ctm);
    fz_transform_point(&rb, &page->ctm);

    if ((link->ld.gotor.flags & fz_link_flag_r_is_zoom)) {
        // /XYZ link, undefined values for the coordinates mean: keep the current position
        if ((link->ld.gotor.flags & fz_link_flag_l_valid))
            result.x = lt.x;
        if ((link->ld.gotor.flags & fz_link_flag_t_valid))
            result.y = lt.y;
        result.dx = result.dy = 0;
    } else if ((link->ld.gotor.flags & (fz_link_flag_fit_h | fz_link_flag_fit_v)) ==
                   (fz_link_flag_fit_h | fz_link_flag_fit_v) &&
               (link->ld.gotor.flags &
                (fz_link_flag_l_valid | fz_link_flag_t_valid | fz_link_flag_r_valid | fz_link_flag_b_valid))) {
        // /FitR link
        result = RectD::FromXY(lt.x, lt.y, rb.x, rb.y);
        // an empty destination rectangle would imply an /XYZ-type link to callers
        if (result.IsEmpty())
            result.dx = result.dy = 0.1;
    } else if ((link->ld.gotor.flags & (fz_link_flag_fit_h | fz_link_flag_fit_v)) == fz_link_flag_fit_h &&
               (link->ld.gotor.flags & fz_link_flag_t_valid)) {
        // /FitH or /FitBH link
        result.y = lt.y;
    }
    // all other link types only affect the zoom level, which we intentionally leave alone
#endif
}

// https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
bool PdfLink::SaveEmbedded(LinkSaverUI& saveUI) {
    CrashIf(!outline || !isAttachment);

    ScopedCritSec scope(engine->ctxAccess);
    // TODO: hack, we stored stream number in outline->page
    return engine->SaveEmbedded(saveUI, outline->page);
}
#endif

class PdfComment : public PageElement {
  public:
    PageAnnotation annot;
    AutoFreeWstr content;

    PdfComment(const WCHAR* content, RectD rect, int pageNo) : annot(PageAnnotType::None, pageNo, rect, 0) {
        this->content = str::Dup(content);
        elementPageNo = pageNo;
        elementType = PageElementType::Comment;
        elementRect = annot.rect;
        elementValue = str::Dup(content);
    }
};

class PdfTocItem : public DocTocItem {
  public:
    PdfLink link;

    PdfTocItem(WCHAR* title, PdfLink link) : DocTocItem(title), link(link) {
    }

    PageDestination* GetPageDestination() override {
        return &link;
    }
};

class PdfImage : public PageElement {
  public:
    PdfEngineImpl* engine = nullptr;
    size_t imageIdx = 0;

    PdfImage(PdfEngineImpl* engine, int pageNo, fz_rect rect, size_t imageIdx) {
        this->engine = engine;
        this->imageIdx = imageIdx;
        elementPageNo = pageNo;
        elementType = PageElementType::Image;
        elementRect = fz_rect_to_RectD(rect);
    }

    RenderedBitmap* GetImage() override {
        return engine->GetPageImage(elementPageNo, elementRect, imageIdx);
    }
};

// in mupdf_load_system_font.c
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

extern "C" static void fz_lock_context_cs(void* user, int lock) {
    PdfEngineImpl* e = (PdfEngineImpl*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

extern "C" static void fz_unlock_context_cs(void* user, int lock) {
    PdfEngineImpl* e = (PdfEngineImpl*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

PdfEngineImpl::PdfEngineImpl() {
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
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_UNLIMITED);
    installFitzErrorCallbacks(ctx);

    pdf_install_load_system_font_funcs(ctx);
}

PdfEngineImpl::~PdfEngineImpl() {
    EnterCriticalSection(&pagesAccess);

    // TODO: remove this lock and see what happens
    EnterCriticalSection(ctxAccess);

    for (auto* pi : _pages) {
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->stext) {
            fz_drop_stext_page(ctx, pi->stext);
        }
        if (pi->list) {
            fz_drop_display_list(ctx, pi->list);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
    }

    DeleteVecMembers(_pages);

    fz_drop_outline(ctx, outline);
    fz_drop_outline(ctx, attachments);
    pdf_drop_obj(ctx, _info);

    fz_drop_document(ctx, _doc);
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

int PdfEngineImpl::PageCount() const {
    CrashIf(pageCount < 0);
    return pageCount;
}

class PasswordCloner : public PasswordUI {
    unsigned char* cryptKey;

  public:
    explicit PasswordCloner(unsigned char* cryptKey) : cryptKey(cryptKey) {
    }

    virtual WCHAR* GetPassword(const WCHAR* fileName, unsigned char* fileDigest, unsigned char decryptionKeyOut[32],
                               bool* saveKey) {
        UNUSED(fileName);
        UNUSED(fileDigest);
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return nullptr;
    }
};

EngineBase* PdfEngineImpl::Clone() {
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

    PdfEngineImpl* clone = new PdfEngineImpl();
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

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

static const WCHAR* findEmbedMarks(const WCHAR* fileName) {
    const WCHAR* embedMarks = nullptr;

    int colonCount = 0;
    for (const WCHAR* c = fileName + str::Len(fileName) - 1; c > fileName; c--) {
        if (*c == ':') {
            if (!str::IsDigit(*(c + 1)))
                break;
            if (++colonCount % 2 == 0)
                embedMarks = c;
        } else if (!str::IsDigit(*c))
            break;
    }

    return embedMarks;
}

bool PdfEngineImpl::Load(const WCHAR* fileName, PasswordUI* pwdUI) {
    AssertCrash(!FileName() && !_doc && ctx);
    SetFileName(fileName);
    if (!ctx)
        return false;

    fz_stream* file = nullptr;
    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    WCHAR* embedMarks = (WCHAR*)findEmbedMarks(fileName);
    if (embedMarks)
        *embedMarks = '\0';
    fz_try(ctx) {
        file = fz_open_file2(ctx, fileName);
    }
    fz_catch(ctx) {
        file = nullptr;
    }
    if (embedMarks) {
        *embedMarks = ':';
    }

OpenEmbeddedFile:
    if (!LoadFromStream(file, pwdUI)) {
        return false;
    }

    if (str::IsEmpty(embedMarks)) {
        return FinishLoading();
    }

    int num, gen;
    embedMarks = (WCHAR*)str::Parse(embedMarks, L":%d:%d", &num, &gen);
    CrashIf(!embedMarks);
    pdf_document* doc = (pdf_document*)_doc;
    // TODO(port): not sure if this fully translates as gen is no longer used
    if (!embedMarks || !pdf_obj_num_is_stream(ctx, doc, num))
        return false;

    fz_buffer* buffer = nullptr;
    fz_var(buffer);
    fz_try(ctx) {
        // TODO(port): not sure if this is the right translation
        CrashMe();
        buffer = pdf_load_stream_number(ctx, doc, num);
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

    goto OpenEmbeddedFile;
}

bool PdfEngineImpl::Load(IStream* stream, PasswordUI* pwdUI) {
    AssertCrash(!FileName() && !_doc && ctx);
    if (!ctx)
        return false;

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

bool PdfEngineImpl::LoadFromStream(fz_stream* stm, PasswordUI* pwdUI) {
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
        if (str::EndsWith(name, "Right"))
            layout = Layout_Book;
        else if (str::StartsWith(name, "Two"))
            layout = Layout_Facing;
    }
    fz_catch(ctx) {
    }

    fz_try(ctx) {
        pdf_obj* prefs = pdf_dict_gets(ctx, root, "ViewerPreferences");
        const char* direction = pdf_to_name(ctx, pdf_dict_gets(ctx, prefs, "Direction"));
        if (str::Eq(direction, "R2L"))
            layout = (PageLayoutType)(layout | Layout_R2L);
    }
    fz_catch(ctx) {
    }

    return layout;
}

bool PdfEngineImpl::FinishLoading() {
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
        fz_rect mbox;
        fz_matrix page_ctm;

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

    fz_try(ctx) {
        // keep a copy of the Info dictionary, as accessing the original
        // isn't thread safe and we don't want to block for this when
        // displaying document properties
        _info = pdf_dict_gets(ctx, pdf_trailer(ctx, doc), "Info");
        if (_info)
            _info = pdf_copy_str_dict(ctx, doc, _info);
        if (!_info)
            _info = pdf_new_dict(ctx, doc, 4);
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile())
            pdf_dict_puts_drop(ctx, _info, "Linearized", PDF_TRUE);
        if (pdf_to_bool(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/MarkInfo/Marked")))
            pdf_dict_puts_drop(ctx, _info, "Marked", PDF_TRUE);
        // also remember known output intents (PDF/X, etc.)
        pdf_obj* intents = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/OutputIntents");
        if (pdf_is_array(ctx, intents)) {
            pdf_obj* list = pdf_new_array(ctx, doc, pdf_array_len(ctx, intents));
            for (int i = 0; i < pdf_array_len(ctx, intents); i++) {
                pdf_obj* intent = pdf_dict_gets(ctx, pdf_array_get(ctx, intents, i), "S");
                if (pdf_is_name(ctx, intent) && !pdf_is_indirect(ctx, intent) &&
                    str::StartsWith(pdf_to_name(ctx, intent), "GTS_PDF"))
                    pdf_array_push(ctx, list, intent);
            }
            pdf_dict_puts_drop(ctx, _info, "OutputIntents", list);
        }
        // also note common unsupported features (such as XFA forms)
        pdf_obj* xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");
        if (pdf_is_array(ctx, xfa))
            pdf_dict_puts_drop(ctx, _info, "Unsupported_XFA", PDF_TRUE);
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
    AssertCrash(!pdf_js_supported(ctx, doc));

    return true;
}

static COLORREF pdfColorToCOLORREF(float color[4]) {
    return MkRgb(color[0], color[1], color[2]);
}

PdfTocItem* PdfEngineImpl::BuildTocTree(fz_outline* outline, int& idCounter, bool isAttachment) {
    PdfTocItem* root = nullptr;
    PdfTocItem* curr = nullptr;

    while (outline) {
        WCHAR* name = nullptr;
        if (outline->title) {
            name = strconv::FromUtf8(outline->title);
            name = pdf_clean_string(name);
        }
        if (!name) {
            name = str::Dup(L"");
        }
        int pageNo = outline->page + 1;
        PdfLink link(this, pageNo, nullptr, outline);
        link.isAttachment = isAttachment;
        PdfTocItem* item = new PdfTocItem(name, link);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = outline->flags;
        item->pageNo = pageNo;
        if (outline->has_color) {
            item->color = pdfColorToCOLORREF(outline->color);
        }

        if (outline->down) {
            item->child = BuildTocTree(outline->down, idCounter, isAttachment);
        }

        if (!root) {
            root = item;
            curr = item;
        } else {
            curr->next = item;
            curr = item;
        }

        outline = outline->next;
    }

    return root;
}

// TODO: maybe build in FinishDownload
DocTocTree* PdfEngineImpl::GetTocTree() {
    if (tocTree) {
        return tocTree;
    }
    if (outline == nullptr && attachments == nullptr) {
        return nullptr;
    }

    int idCounter = 0;

    PdfTocItem* root = nullptr;
    if (outline) {
        root = BuildTocTree(outline, idCounter, false);
    }
    if (!attachments) {
        if (!root) {
            return nullptr;
        }
        tocTree = new DocTocTree(root);
        return tocTree;
    }
    PdfTocItem* att = BuildTocTree(attachments, idCounter, true);
    if (!root) {
        tocTree = new DocTocTree(att);
        return tocTree;
    }
    root->AddSibling(att);
    tocTree = new DocTocTree(root);
    return tocTree;
}

PageDestination* PdfEngineImpl::GetNamedDest(const WCHAR* name) {
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
    pageDest = makeSimpleDest(pageNo, r);
    fz_free(ctx, uri);
    return pageDest;
}

FzPageInfo* PdfEngineImpl::GetFzPageInfo(int pageNo, bool failIfBusy) {
    GetFzPage(pageNo, failIfBusy);
    return _pages[pageNo - 1];
}

fz_page* PdfEngineImpl::GetFzPage(int pageNo, bool failIfBusy) {
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = _pages[pageNo - 1];
    fz_page* page = pageInfo->page;
    // TODO: not sure what failIfBusy is supposed to do
    if (page || failIfBusy) {
        return page;
    }

    ScopedCritSec ctxScope(ctxAccess);
    fz_var(page);
    fz_try(ctx) {
        page = fz_load_page(ctx, _doc, pageNo - 1);
        pageInfo->page = page;
    }
    fz_catch(ctx) {
    }

    /* TODO: handle try later?
    if (fz_caught(ctx) != FZ_ERROR_TRYLATER) {
        return nullptr;
    }
    */

    fz_display_list* list = NULL;
    fz_var(list);
    fz_try(ctx) {
        list = fz_new_display_list_from_page(ctx, page);
    }
    fz_catch(ctx) {
        list = nullptr;
    }

    pageInfo->list = list;

    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;

    fz_try(ctx) {
        pageInfo->stext = fz_new_stext_page_from_page(ctx, page, &opts);
    }
    fz_catch(ctx) {
        pageInfo->stext = nullptr;
    }

    auto* links = fz_load_links(ctx, page);
    pageInfo->links = FixupPageLinks(links);
    LinkifyPageText(pageInfo);

    ProcessPageAnnotations(pageInfo);

    fz_find_images(pageInfo->stext, pageInfo->images);
    return page;
}

RectD PdfEngineImpl::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectD PdfEngineImpl::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie = {};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);

    RectD mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
        fz_run_display_list(ctx, pageInfo->list, dev, fz_identity, pagerect, &fzcookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
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

PointD PdfEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    fz_point pt2 = {(float)pt.x, (float)pt.y};
    pt2 = fz_transform_point(pt2, ctm);
    return PointD(pt2.x, pt2.y);
}

RectD PdfEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    fz_rect rect2 = fz_RectD_to_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap* PdfEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD* pageRect, RenderTarget target,
                                            AbortCookie** cookie_out) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    fz_page* page = pageInfo->page;

    if (!page || !pageInfo->list) {
        return nullptr;
    }

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (cookie_out) {
        cookie = new FitzAbortCookie();
        *cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    // TODO(port): I don't see why this lock is needed
    ScopedCritSec cs(ctxAccess);

    fz_rect pRect;
    if (pageRect) {
        pRect = fz_RectD_to_rect(*pageRect);
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
    fz_device* dev = NULL;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    fz_try(ctx) {
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
        // initialize with white background
        fz_clear_pixmap_with_value(ctx, pix, 0xff);

        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        // TODO: use fz_infinite_rect instead of cliprect?
        fz_run_display_list(ctx, pageInfo->list, dev, ctm, cliprect, fzcookie);
        bitmap = new_rendered_fz_pixmap(ctx, pix);
    }
    fz_always(ctx) {
        if (dev) {
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
        }
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        return nullptr;
    }
    return bitmap;
}

static PdfComment* makePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_annot_rect(ctx, annot);
    auto tp = pdf_annot_type(ctx, annot);
    const char* contents = pdf_annot_contents(ctx, annot);
    const char* label = pdf_field_label(ctx, annot->obj);
    const char* s = contents;
    // TODO: use separate classes for comments and tooltips?
    if (str::IsEmpty(contents) && PDF_ANNOT_WIDGET == tp) {
        s = label;
    }
    AutoFreeWstr ws(strconv::FromUtf8(s));
    RectD rd = fz_rect_to_RectD(rect);
    return new PdfComment(ws, rd, pageNo);
}

PageElement* PdfEngineImpl::GetElementAtPos(int pageNo, PointD pt) {
    auto* pageInfo = GetFzPageInfo(pageNo);
    if (!pageInfo) {
        return nullptr;
    }

    fz_link* link = pageInfo->links;
    fz_point p = {(float)pt.x, (float)pt.y};
    while (link) {
        if (fz_is_pt_in_rect(link->rect, p)) {
            return new PdfLink(this, pageNo, link, nullptr);
        }
        link = link->next;
    }

    size_t imageIdx = 0;
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_is_pt_in_rect(ir, p)) {
            return new PdfImage(this, pageNo, ir, imageIdx);
        }
        imageIdx++;
    }

    ScopedCritSec scope(ctxAccess); // TODO: probably not needed
    for (auto* annot : pageInfo->pageAnnots) {
        fz_rect rect = pdf_annot_rect(ctx, annot);
        if (!fz_is_pt_in_rect(rect, p)) {
            continue;
        }
        return makePdfCommentFromPdfAnnot(ctx, pageNo, annot);
    }

    return nullptr;
}

Vec<PageElement*>* PdfEngineImpl::GetElements(int pageNo) {
    fz_page* page = GetFzPage(pageNo, true);
    if (!page) {
        return nullptr;
    }
    FzPageInfo* pageInfo = _pages[pageNo - 1];

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement*>* els = new Vec<PageElement*>();

    size_t imageIdx = 0;
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        auto image = new PdfImage(this, pageNo, ir, imageIdx);
        els->Append(image);
        imageIdx++;
    }

    ScopedCritSec scope(ctxAccess); // TODO: possibly not needed
    for (auto* annot : pageInfo->pageAnnots) {
        auto comment = makePdfCommentFromPdfAnnot(ctx, pageNo, annot);
        els->Append(comment);
    }

    fz_link* link = pageInfo->links;
    while (link) {
        auto* el = new PdfLink(this, pageNo, link, nullptr);
        els->Append(el);
        link = link->next;
    }
    if (els->size() == 0) {
        delete els;
        return nullptr;
    }

    els->Reverse();
    return els;
}

void PdfEngineImpl::LinkifyPageText(FzPageInfo* pageInfo) {
    RectI* coords;
    fz_stext_page* stext = pageInfo->stext;
    if (!stext) {
        return;
    }
    ScopedCritSec scope(ctxAccess);
    WCHAR* pageText = fz_text_page_to_str(stext, &coords);
    if (!pageText) {
        return;
    }

    LinkRectList* list = LinkifyText(pageText, coords);
    free(pageText);
    fz_page* page = pageInfo->page;

    for (size_t i = 0; i < list->links.size(); i++) {
        fz_rect bbox = list->coords.at(i);
        bool overlaps = false;
        fz_link* link = pageInfo->links;
        while (link && !overlaps) {
            overlaps = fz_calc_overlap(bbox, link->rect) >= 0.25f;
            link = link->next;
        }
        if (overlaps) {
            continue;
        }

        AutoFree uri(strconv::WstrToUtf8(list->links.at(i)));
        if (!uri.Get()) {
            continue;
        }

        // add links in top-to-bottom order (i.e. last-to-first)
        link = fz_new_link(ctx, bbox, _doc, uri.Get());
        link->next = pageInfo->links;
        pageInfo->links = link;
    }
    delete list;
    free(coords);
}

bool PdfEngineImpl::SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots) {
    return SaveFileAs(pdfFileName, includeUserAnnots);
}

bool PdfEngineImpl::BenchLoadPage(int pageNo) {
    return GetFzPage(pageNo) != nullptr;
}

fz_matrix PdfEngineImpl::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = fz_RectD_to_rect(PageMediabox(pageNo));
    return fz_create_view_ctm(tmpRc, zoom, rotation);
}

fz_matrix PdfEngineImpl::viewctm(fz_page* page, float zoom, int rotation) {
    return fz_create_view_ctm(fz_bound_page(ctx, page), zoom, rotation);
}

void PdfEngineImpl::ProcessPageAnnotations(FzPageInfo* pageInfo) {
    auto& annots = pageInfo->pageAnnots;

    auto page = (pdf_page*)pageInfo->page;
    if (!page) {
        return;
    }
    for (pdf_annot* annot = page->annots; annot; annot = annot->next) {
        auto tp = pdf_annot_type(ctx, annot);
        const char* contents = pdf_annot_contents(ctx, annot); // don't free
        bool isContentsEmpty = str::IsEmpty(contents);
        const char* label = pdf_field_label(ctx, annot->obj); // don't free
        bool isLabelEmpty = str::IsEmpty(label);
        int flags = pdf_field_flags(ctx, annot->obj);

        if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
            dbglogf("found file attachment annotation\n");
            // TODO: write a program for mass processing of files to find pdfs
            // with wanted features for testing
#if 0
            pdf_obj* file = pdf_dict_gets(annot->obj, "FS");
            pdf_obj* embedded = pdf_dict_getsa(pdf_dict_gets(file, "EF"), "DOS", "F");
            fz_rect rect;
            pdf_to_rect(ctx, pdf_dict_gets(annot->obj, "Rect"), &rect);
            if (file && embedded && !fz_is_empty_rect(rect)) {
                fz_link_dest ld;
                ld.kind = FZ_LINK_LAUNCH;
                ld.ld.launch.file_spec = pdf_file_spec_to_str(_doc, file);
                ld.ld.launch.new_window = 1;
                ld.ld.launch.embedded_num = pdf_to_num(embedded);
                ld.ld.launch.embedded_gen = pdf_to_gen(embedded);
                ld.ld.launch.is_uri = 0;
                fz_transform_rect(&rect, &page->ctm);
                // add links in top-to-bottom order (i.e. last-to-first)
                fz_link* link = fz_new_link(ctx, &rect, ld);
                link->next = page->links;
                page->links = link;
                // TODO: expose /Contents in addition to the file path
            } else if (!isContentsEmpty) {
                annots.Append(annot);
            }
#endif
            continue;
        }

        if (!isContentsEmpty && tp != PDF_ANNOT_FREE_TEXT) {
            annots.Append(annot);
            continue;
        }

        if (PDF_ANNOT_WIDGET == tp && !isLabelEmpty) {
            if (!(flags & PDF_FIELD_IS_READ_ONLY)) {
                annots.Append(annot);
            }
        }
    }

    // re-order list into top-to-bottom order (i.e. last-to-first)
    annots.Reverse();
}

RenderedBitmap* PdfEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    if (!pageInfo->page) {
        return nullptr;
    }
    auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= images.size();
    fz_rect imgRect = images.at(imageIdx).rect;
    bool badRect = fz_rect_to_RectD(imgRect) != rect;
    CrashIf(outOfBounds);
    CrashIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);

    fz_image* image = images.at(imageIdx).image;
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

// TODO: remember this instead of re-doing
WCHAR* PdfEngineImpl::ExtractPageText(int pageNo, RectI** coordsOut) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    fz_stext_page* stext = pageInfo->stext;
    if (!stext) {
        return nullptr;
    }
    ScopedCritSec scope(ctxAccess);
    WCHAR* content = fz_text_page_to_str(stext, coordsOut);
    return content;
}

bool PdfEngineImpl::IsLinearizedFile() {
    ScopedCritSec scope(ctxAccess);
    // determine the object number of the very first object in the file
    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);
    fz_seek(ctx, doc->file, 0, 0);
    int tok = pdf_lex(ctx, doc->file, &doc->lexbuf.base);
    if (tok != PDF_TOK_INT)
        return false;
    int num = doc->lexbuf.base.i;
    if (num < 0 || num >= pdf_xref_len(ctx, doc))
        return false;
    // check whether it's a linearization dictionary
    fz_try(ctx) {
        pdf_cache_object(ctx, doc, num);
    }
    fz_catch(ctx) {
        return false;
    }
    pdf_obj* obj = pdf_get_xref_entry(ctx, doc, num)->obj;
    if (!pdf_is_dict(ctx, obj))
        return false;
    // /Linearized format must be version 1.0
    if (pdf_to_real(ctx, pdf_dict_gets(ctx, obj, "Linearized")) != 1.0f)
        return false;
    // /L must be the exact file size
    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "L")) != doc->file_size)
        return false;

    // /O must be the object number of the first page
    // TODO(port): at this point we don't have _pages loaded yet. for now always return false here
    auto fzpage = _pages[0]->page;
    if (!fzpage) {
        return false;
    }
    pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);

    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "O")) != pdf_to_num(ctx, page->obj))
        return false;

    // /N must be the total number of pages
    if (pdf_to_int(ctx, pdf_dict_gets(ctx, obj, "N")) != PageCount())
        return false;
    // /H must be an array and /E and /T must be integers
    return pdf_is_array(ctx, pdf_dict_gets(ctx, obj, "H")) && pdf_is_int(ctx, pdf_dict_gets(ctx, obj, "E")) &&
           pdf_is_int(ctx, pdf_dict_gets(ctx, obj, "T"));
}

static void pdf_extract_fonts(fz_context* ctx, pdf_obj* res, Vec<pdf_obj*>& fontList, Vec<pdf_obj*>& resList) {
    if (!res || pdf_mark_obj(ctx, res)) {
        return;
    }
    resList.Append(res);

    pdf_obj* fonts = pdf_dict_gets(ctx, res, "Font");
    for (int k = 0; k < pdf_dict_len(ctx, fonts); k++) {
        pdf_obj* font = pdf_resolve_indirect(ctx, pdf_dict_get_val(ctx, fonts, k));
        if (font && !fontList.Contains(font))
            fontList.Append(font);
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj* xobjs = pdf_dict_gets(ctx, res, "XObject");
    for (int k = 0; k < pdf_dict_len(ctx, xobjs); k++) {
        pdf_obj* xobj = pdf_dict_get_val(ctx, xobjs, k);
        pdf_obj* xres = pdf_dict_gets(ctx, xobj, "Resources");
        pdf_extract_fonts(ctx, xres, fontList, resList);
    }
}

WCHAR* PdfEngineImpl::ExtractFontList() {
    Vec<pdf_obj*> fontList;
    Vec<pdf_obj*> resList;

    // collect all fonts from all page objects
    for (int i = 1; i <= PageCount(); i++) {
        fz_page* fzpage = GetFzPage(i);
        if (!fzpage) {
            continue;
        }

        ScopedCritSec scope(ctxAccess);
        pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);
        fz_try(ctx) {
            pdf_obj* resources = pdf_page_resources(ctx, page);
            pdf_extract_fonts(ctx, resources, fontList, resList);
            for (pdf_annot* annot = page->annots; annot; annot = annot->next) {
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
            if (!font2)
                font2 = font;

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
            if (desc && (pdf_dict_gets(ctx, desc, "FontFile") || pdf_dict_getsa(ctx, desc, "FontFile2", "FontFile3")))
                embedded = true;
            if (embedded && str::Len(name) > 7 && name[6] == '+')
                name += 7;

            type = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Subtype"));
            if (font2 != font) {
                const char* type2 = pdf_to_name(ctx, pdf_dict_gets(ctx, font2, "Subtype"));
                if (str::Eq(type2, "CIDFontType0"))
                    type = "Type1 (CID)";
                else if (str::Eq(type2, "CIDFontType2"))
                    type = "TrueType (CID)";
            }
            if (str::Eq(type, "Type3"))
                embedded = pdf_dict_gets(ctx, font2, "CharProcs") != nullptr;

            encoding = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Encoding"));
            if (str::Eq(encoding, "WinAnsiEncoding"))
                encoding = "Ansi";
            else if (str::Eq(encoding, "MacRomanEncoding"))
                encoding = "Roman";
            else if (str::Eq(encoding, "MacExpertEncoding"))
                encoding = "Expert";
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

        AutoFreeWstr fontInfo(strconv::FromUtf8(info.LendData()));
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

WCHAR* PdfEngineImpl::GetProperty(DocumentProperty prop) {
    if (!_doc)
        return nullptr;

    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

    if (DocumentProperty::PdfVersion == prop) {
        int major = doc->version / 10, minor = doc->version % 10;
        pdf_crypt* crypt = doc->crypt;
        if (1 == major && 7 == minor && pdf_crypt_version(ctx, crypt) == 5) {
            if (pdf_crypt_revision(ctx, crypt) == 5)
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 3);
            if (pdf_crypt_revision(ctx, crypt) == 6)
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 8);
        }
        return str::Format(L"%d.%d", major, minor);
    }

    if (DocumentProperty::PdfFileStructure == prop) {
        WStrVec fstruct;
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Linearized")))
            fstruct.Append(str::Dup(L"linearized"));
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Marked")))
            fstruct.Append(str::Dup(L"tagged"));
        if (pdf_dict_gets(ctx, _info, "OutputIntents")) {
            for (int i = 0; i < pdf_array_len(ctx, pdf_dict_gets(ctx, _info, "OutputIntents")); i++) {
                pdf_obj* intent = pdf_array_get(ctx, pdf_dict_gets(ctx, _info, "OutputIntents"), i);
                CrashIf(!str::StartsWith(pdf_to_name(ctx, intent), "GTS_"));
                fstruct.Append(strconv::FromUtf8(pdf_to_name(ctx, intent) + 4));
            }
        }
        return fstruct.size() > 0 ? fstruct.Join(L",") : nullptr;
    }

    if (DocumentProperty::UnsupportedFeatures == prop) {
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, _info, "Unsupported_XFA")))
            return str::Dup(L"XFA");
        return nullptr;
    }

    if (DocumentProperty::FontList == prop)
        return ExtractFontList();

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
            return obj ? pdf_clean_string(pdf_to_wstr(ctx, obj)) : nullptr;
        }
    }
    return nullptr;
};

bool PdfEngineImpl::SupportsAnnotation(bool forSaving) const {
    if (!forSaving) {
        return true;
    }
    // TODO: support updating of documents where pages aren't all numbered objects?
    size_t n = _pages.size();
    for (size_t i = 0; i < n; i++) {
        auto* pageInfo = _pages[i];
        fz_page* fzpage = pageInfo->page;
        pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);
        if (!page || pdf_to_num(ctx, page->obj) == 0) {
            return false;
        }
    }
    return true;
}

void PdfEngineImpl::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    // TODO: use a new critical section to avoid blocking the UI thread
    ScopedCritSec scope(ctxAccess);
    if (list)
        userAnnots = *list;
    else
        userAnnots.Reset();
}

std::string_view PdfEngineImpl::GetFileData() {
    u8* res = nullptr;
    ScopedCritSec scope(ctxAccess);

    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);
    size_t size = 0;

    fz_var(res);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, doc->file, &size);
    }
    fz_catch(ctx) {
        res = nullptr;
    }

    if (res) {
        return {(char*)res, size};
    }
    auto path = FileName();
    if (!path) {
        return {};
    }
    return file::ReadFile(path);
}

bool PdfEngineImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    AutoFreeWstr dstPath = strconv::FromUtf8(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.as_view());
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

static bool pdf_file_update_add_annotation(fz_context* ctx, pdf_document* doc, pdf_page* page, pdf_obj* page_obj,
                                           PageAnnotation& annot, pdf_obj* annots) {
    CrashMePort();
#if 0
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
    fz_rect r = fz_RectD_to_rect(annot.rect);
    fz_matrix invctm = fz_invert_matrix(page->ctm)
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
#endif
    return true;
}

bool PdfEngineImpl::SaveUserAnnots(const char* pathUtf8) {
    if (!userAnnots.size())
        return true;

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    bool ok = true;
    Vec<PageAnnotation> pageAnnots;
    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

    fz_try(ctx) {
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            fz_page* fzpage = GetFzPage(pageNo);
            pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);
            pdf_obj* page_obj = page->obj;
            // TODO: this will skip annotations for broken documents
            if (!page || !pdf_to_num(ctx, page_obj)) {
                ok = false;
                break;
            }
            pageAnnots = fz_get_user_page_annots(userAnnots, pageNo);
            if (pageAnnots.size() == 0)
                continue;
            // get the page's /Annots array for appending
            pdf_obj* annots = pdf_dict_gets(ctx, page_obj, "Annots");
            if (!pdf_is_array(ctx, annots)) {
                pdf_dict_puts_drop(ctx, page_obj, "Annots", pdf_new_array(ctx, doc, (int)pageAnnots.size()));
                annots = pdf_dict_gets(ctx, page_obj, "Annots");
            }
            if (!pdf_is_indirect(ctx, annots)) {
                // make /Annots indirect for the current /Page
                CrashMePort();
                // TODO(port): no pdf_new_ref
                // pdf_dict_puts_drop(ctx, page_obj, "Annots", pdf_new_ref(ctx, _doc, annots));
            }
            // append all annotations for the current page
            for (size_t i = 0; i < pageAnnots.size(); i++) {
                ok &= pdf_file_update_add_annotation(ctx, doc, page, page_obj, pageAnnots.at(i), annots);
            }
        }
        if (ok) {
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
bool PdfEngineImpl::SaveEmbedded(LinkSaverUI& saveUI, int num) {
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

bool PdfEngineImpl::HasClipOptimizations(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return false;
    }

    fz_rect mbox = fz_RectD_to_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_calc_overlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

WCHAR* PdfEngineImpl::GetPageLabel(int pageNo) const {
    if (!_pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    return str::Dup(_pageLabels->at(pageNo - 1));
}

int PdfEngineImpl::GetPageByLabel(const WCHAR* label) const {
    int pageNo = 0;
    if (_pageLabels) {
        pageNo = _pageLabels->Find(label) + 1;
    }

    if (!pageNo) {
        return EngineBase::GetPageByLabel(label);
    }

    return pageNo;
}

EngineBase* PdfEngineImpl::CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    if (str::IsEmpty(fileName)) {
        return nullptr;
    }
    PdfEngineImpl* engine = new PdfEngineImpl();
    if (!engine->Load(fileName, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* PdfEngineImpl::CreateFromStream(IStream* stream, PasswordUI* pwdUI) {
    PdfEngineImpl* engine = new PdfEngineImpl();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEnginePdfSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        char header[1024] = {0};
        file::ReadN(fileName, header, sizeof(header));

        for (int i = 0; i < sizeof(header) - 4; i++) {
            if (str::EqN(header + i, "%PDF", 4))
                return true;
        }
        return false;
    }

    return str::EndsWithI(fileName, L".pdf") || findEmbedMarks(fileName);
}

EngineBase* CreateEnginePdfFromFile(const WCHAR* fileName, PasswordUI* pwdUI) {
    return PdfEngineImpl::CreateFromFile(fileName, pwdUI);
}

EngineBase* CreateEnginePdfFromStream(IStream* stream, PasswordUI* pwdUI) {
    return PdfEngineImpl::CreateFromStream(stream, pwdUI);
}
