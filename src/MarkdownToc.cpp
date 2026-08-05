/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/Win.h"

#include "Theme.h"
#include "MarkdownToc.h"

extern "C" {
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "node.h"
}

static bool IsMarkdownExt(Str path) {
    return str::EndsWithI(path, StrL(".md")) || str::EndsWithI(path, StrL(".markdown"));
}

static void CollectMdInDir(Str dir, StrVec& out) {
    DirIter di(dir);
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = false;
    for (DirIterEntry* de : di) {
        if (!IsRegularFile(de) || !IsMarkdownExt(de->name)) {
            continue;
        }
        out.Append(de->filePath);
    }
}

void CollectMarkdownFiles(Str baseDir, Str openedFile, StrVec& filesOut) {
    filesOut.Reset();
    if (len(baseDir) == 0) {
        if (openedFile) {
            filesOut.Append(openedFile);
        }
        return;
    }

    CollectMdInDir(baseDir, filesOut);

    DirIter di(baseDir);
    di.includeFiles = false;
    di.includeDirs = true;
    di.recurse = false;
    for (DirIterEntry* de : di) {
        if (!IsDirectory(de)) {
            continue;
        }
        CollectMdInDir(de->filePath, filesOut);

        DirIter di2(de->filePath);
        di2.includeFiles = false;
        di2.includeDirs = true;
        di2.recurse = false;
        for (DirIterEntry* de2 : di2) {
            if (!IsDirectory(de2)) {
                continue;
            }
            CollectMdInDir(de2->filePath, filesOut);
        }
    }

    if (openedFile) {
        bool found = false;
        for (Str p : filesOut) {
            if (path::IsSame(p, openedFile)) {
                found = true;
                break;
            }
        }
        if (!found) {
            filesOut.Append(openedFile);
        }
    }

    Sort(&filesOut);
}

// cmark_gfm_core_extensions_ensure_registered() must run exactly once process-wide;
// concurrent calls from TOC worker threads race and abort in cmark_register_node_flag.
static Mutex gCmarkInitLock;
static bool gCmarkInitialized = false;

static void EnsureCmarkPluginsRegistered() {
    ScopedMutex scope(&gCmarkInitLock);
    if (gCmarkInitialized) {
        return;
    }
    cmark_gfm_core_extensions_ensure_registered();
    gCmarkInitialized = true;
}

static void AttachGfmExtensions(cmark_parser* parser) {
    // no "autoheaderid": its slug rules differ from GitHub's, so the ids it
    // makes don't match the "#anchor" links such documents carry.
    // AddHeadingAnchors() emits them instead (#5883).
    const char* exts[] = {"table", "strikethrough", "autolink", "tagfilter", "tasklist"};
    for (const char* ext : exts) {
        cmark_syntax_extension* syntax = cmark_find_syntax_extension(ext);
        if (syntax) {
            cmark_parser_attach_syntax_extension(parser, syntax);
        }
    }
}

// GitHub's slugger (github-slugger), which is what generated docs link against:
// lowercase, drop anything that isn't a letter / digit / '_' / '-', and turn
// each whitespace run character into its own '-'. Notably '_' survives and
// separators are NOT collapsed, so "## adc_intr_ctl . TRANS_EN" is
// "#adc_intr_ctl--trans_en" (issue #5883). cmark-gfm's autoheaderid extension
// uses different rules (maps '_' and punctuation to '-', then collapses runs),
// so we don't use it -- MarkdownAddHeadingAnchors() emits the ids from here,
// keeping the ToC anchors and the html ids one implementation.
static void AppendSlugChar(str::Builder* out, unsigned int c) {
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
        out->AppendChar((char)c);
        return;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        out->AppendChar('-');
        return;
    }
    // everything else (ascii punctuation) is dropped
}

Str MarkdownHeadingSlug(Arena* a, Str title) {
    str::Builder out;
    const u8* p = (const u8*)title.s;
    const u8* e = p + title.len;
    // GitHub trims the title before slugging, so leading/trailing spaces don't
    // become hyphens
    while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) {
        e--;
    }
    while (p < e) {
        unsigned int c = *p++;
        if (c >= 0x80) {
            // non-ascii is kept as-is (GitHub keeps accented letters); copy the
            // continuation bytes through unchanged
            out.AppendChar((char)c);
            while (p < e && (*p & 0xC0) == 0x80) {
                out.AppendChar((char)*p++);
            }
            continue;
        }
        AppendSlugChar(&out, c);
    }
    Str res = out.TakeStr();
    if (a) {
        return str::Dup(a, res);
    }
    return res;
}

// cmark chunks are length-prefixed; alloc=1 buffers are not always NUL-terminated.
static Str DupCmarkChunk(cmark_chunk* chunk) {
    if (!chunk || !chunk->data || chunk->len <= 0) {
        return {};
    }
    return str::Dup(Str((char*)chunk->data, (int)chunk->len));
}

static void AppendHeadingText(cmark_node* node, str::Builder* out) {
    cmark_node_type type = cmark_node_get_type(node);
    if (type == CMARK_NODE_TEXT || type == CMARK_NODE_CODE) {
        Str s = DupCmarkChunk(&node->as.literal);
        if (s) {
            out->Append(s);
            str::Free(s);
        }
        return;
    }
    for (cmark_node* child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        AppendHeadingText(child, out);
    }
}

static Str ExtractHeadingTitle(cmark_node* heading) {
    str::Builder out;
    for (cmark_node* child = cmark_node_first_child(heading); child; child = cmark_node_next(child)) {
        AppendHeadingText(child, &out);
    }
    Str title = out.TakeStr();
    if (!title) {
        return {};
    }
    return str::Dup(title);
}

static void ParseMarkdownHeadings(Str filePath, MarkdownFileToc* toc) {
    toc->filePath = str::Dup(filePath);
    toc->headings.Reset();

    Str data = file::ReadFile(filePath);
    if (!data) {
        return;
    }

    EnsureCmarkPluginsRegistered();

    int options = CMARK_OPT_DEFAULT;
    cmark_parser* parser = cmark_parser_new(options);
    AttachGfmExtensions(parser);
    cmark_parser_feed(parser, data.s, (size_t)data.len);
    cmark_node* doc = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    if (!doc) {
        return;
    }

    cmark_iter* iter = cmark_iter_new(doc);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev != CMARK_EVENT_ENTER) {
            continue;
        }
        cmark_node* node = cmark_iter_get_node(iter);
        if (cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }
        Str title = ExtractHeadingTitle(node);
        if (!title) {
            continue;
        }
        MarkdownHeadingItem item;
        item.title = title;
        item.anchor = MarkdownHeadingSlug(nullptr, title);
        item.level = cmark_node_get_heading_level(node);
        toc->headings.Append(item);
    }
    cmark_iter_free(iter);
    cmark_node_free(doc);
}

struct MdTocParseCtx {
    StrVec* files = nullptr;
    Vec<MarkdownFileToc>* tocs = nullptr;
    AtomicInt nextIdx;
};

static void MdTocParseWorker(MdTocParseCtx* ctx) {
    int n = len(*ctx->files);
    for (;;) {
        int i = AtomicIntInc(&ctx->nextIdx) - 1;
        if (i >= n) {
            break;
        }
        Str path = ctx->files->At(i);
        MarkdownFileToc* toc = &(*ctx->tocs)[i];
        ParseMarkdownHeadings(path, toc);
    }
}

static void InitMarkdownFileToc(MarkdownFileToc* ft) {
    ft->filePath = {};
    ft->relPath = {};
    // MarkdownFileToc contains a nested Vec; Reset() from a zeroed slot is safe.
    ft->headings.Reset();
}

void ParseMarkdownTocsParallel(StrVec& files, Vec<MarkdownFileToc>& tocsOut) {
    int n = len(files);
    tocsOut.Reset();
    if (n == 0) {
        return;
    }
    // Allocate all slots before initializing nested Vec members. AppendBlanks would
    // memmove MarkdownFileToc values and leave headings.els pointing at stale addrs.
    if (!VecResize(tocsOut, n)) {
        return;
    }
    for (int i = 0; i < n; i++) {
        InitMarkdownFileToc(&tocsOut[i]);
    }

    EnsureCmarkPluginsRegistered();

    MdTocParseCtx ctx;
    ctx.files = &files;
    ctx.tocs = &tocsOut;
    AtomicIntSet(&ctx.nextIdx, 0);

    int numThreads = CpuCoreCount() - 2;
    numThreads = std::max(numThreads, 1);
    numThreads = std::min(numThreads, n);

    Vec<ThreadHandle> threads;
    for (int t = 0; t < numThreads; t++) {
        auto fn = MkFunc0(MdTocParseWorker, &ctx);
        threads.Append(StartThread(fn, "MdTocParse"));
    }
    for (ThreadHandle h : threads) {
#if OS_WIN
        WaitForSingleObject(h, INFINITE);
        SafeCloseThreadHandle(&h);
#else
        SafeCloseThreadHandle(&h);
#endif
    }
}

static const char* kMarkdownPageCssFmt = R"(
:root { %s }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif; font-size: 16px;
  line-height: 1.5; color: var(--fg); background: var(--bg); margin: 0; padding: 2rem 3rem; max-width: 980px; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
h1,h2,h3,h4,h5,h6 { margin-top: 1.5rem; margin-bottom: 1rem; font-weight: 600; line-height: 1.25; }
h1 { font-size: 2em; border-bottom: 1px solid var(--border); padding-bottom: .3em; }
h2 { font-size: 1.5em; border-bottom: 1px solid var(--border); padding-bottom: .3em; }
code, pre { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 85%%; }
pre { background: var(--code-bg); padding: 16px; overflow: auto; border-radius: 6px; }
code { background: var(--code-bg); padding: .2em .4em; border-radius: 6px; }
pre code { background: transparent; padding: 0; }
blockquote { margin: 0; padding: 0 1em; color: var(--muted); border-left: .25em solid var(--border); }
table { border-collapse: collapse; }
table th, table td { border: 1px solid var(--border); padding: 6px 13px; }
img { max-width: 100%%; }
)";

static TempStr ColorToCssTemp(COLORREF c) {
    return fmt("#%02x%02x%02x", (int)GetRValue(c), (int)GetGValue(c), (int)GetBValue(c));
}

// page colors follow the document color mode: ThemePageRenderColors gives
// black-on-white (or FixedPageUI overrides) when DocumentColorsFollowTheme
// is off and theme-derived page colors when it's on
static TempStr MarkdownPageCssTemp() {
    COLORREF bgCol;
    COLORREF txtCol = ThemePageRenderColors(bgCol);
    bool dark = !IsLightColor(bgCol);
    bool isDefault = (bgCol == RGB(0xff, 0xff, 0xff)) && (txtCol == RGB(0, 0, 0));

    TempStr bg = ColorToCssTemp(bgCol);
    // the default black-on-white gets the classic GitHub palette
    TempStr fg = isDefault ? str::DupTemp("#24292f") : ColorToCssTemp(txtCol);
    Str link = dark ? StrL("#4493f8") : StrL("#0969da");
    Str muted = dark ? StrL("#9198a1") : StrL("#57606a");
    TempStr border = isDefault ? str::DupTemp("#d0d7de") : ColorToCssTemp(AccentColor(bgCol, 25));
    TempStr codeBg = isDefault ? str::DupTemp("#f6f8fa") : ColorToCssTemp(AccentColor(bgCol, 8));

    TempStr cssVars =
        fmt("--bg:%s; --fg:%s; --link:%s; --muted:%s; --border:%s; --code-bg:%s;", bg, fg, link, muted, border, codeBg);
    return fmt(kMarkdownPageCssFmt, cssVars);
}

// Markdown pages are exposed to WebView2 as generated .html resources. Keep
// relative links on that virtual resource graph instead of navigating to the
// source .md name.
static TempStr MarkdownLinkToHtmlTemp(Str url) {
    if (!url || url.s[0] == '#' || str::StartsWith(url, StrL("//"))) {
        return {};
    }

    int pathLen = url.len;
    for (int i = 0; i < url.len; i++) {
        char c = url.s[i];
        if (c == '?' || c == '#') {
            pathLen = i;
            break;
        }
        if (c == ':') {
            return {};
        }
    }

    Str path(url.s, pathLen);
    int extLen = 0;
    if (str::EndsWithI(path, StrL(".markdown"))) {
        extLen = 9;
    } else if (str::EndsWithI(path, StrL(".md"))) {
        extLen = 3;
    } else {
        return {};
    }

    Str base(url.s, pathLen - extLen);
    Str suffix(url.s + pathLen, url.len - pathLen);
    return fmt("%s.html%s", base, suffix);
}

static void RewriteMarkdownLinks(cmark_node* doc) {
    cmark_iter* iter = cmark_iter_new(doc);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev != CMARK_EVENT_ENTER) {
            continue;
        }
        cmark_node* node = cmark_iter_get_node(iter);
        if (cmark_node_get_type(node) != CMARK_NODE_LINK) {
            continue;
        }
        const char* url = cmark_node_get_url(node);
        if (!url) {
            continue;
        }
        TempStr htmlUrl = MarkdownLinkToHtmlTemp(Str(url));
        if (htmlUrl) {
            cmark_node_set_url(node, CStrTemp(htmlUrl));
        }
    }
    cmark_iter_free(iter);
}

static bool IsSafeAnchorId(Str id) {
    if (!id) {
        return false;
    }
    for (int i = 0; i < id.len; i++) {
        char c = id.s[i];
        bool isAlphaNum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!isAlphaNum && c != '-' && c != '_' && c != ':' && c != '.') {
            return false;
        }
    }
    return true;
}

static bool ParseSafeAnchorOpen(Str html, Str* idOut) {
    str::TrimWSInPlace(html, str::TrimOpt::Both);
    Str prefix = StrL("<a id=\"");
    Str suffix = StrL("\">");
    if (!str::TrimPrefix(html, prefix) || !str::EndsWith(html, suffix)) {
        return false;
    }
    Str id(html.s, html.len - suffix.len);
    if (!IsSafeAnchorId(id)) {
        return false;
    }
    *idOut = id;
    return true;
}

static bool ParseSafeEmptyAnchor(Str html, Str* idOut) {
    str::TrimWSInPlace(html, str::TrimOpt::Both);
    Str prefix = StrL("<a id=\"");
    Str suffix = StrL("\"></a>");
    if (!str::TrimPrefix(html, prefix) || !str::EndsWith(html, suffix)) {
        return false;
    }
    Str id(html.s, html.len - suffix.len);
    if (!IsSafeAnchorId(id)) {
        return false;
    }
    *idOut = id;
    return true;
}

static cmark_node* NewSafeAnchorNode(cmark_node_type type, Str id) {
    cmark_node* node = cmark_node_new(type);
    if (!node) {
        return nullptr;
    }
    TempStr html = fmt("<a id=\"%s\"></a>", id);
    if (!cmark_node_set_on_enter(node, CStrTemp(html))) {
        cmark_node_free(node);
        return nullptr;
    }
    return node;
}

// cmark's safe renderer drops all raw HTML. Preserve empty anchors with a
// strictly validated id so links in generated reports keep working without
// enabling arbitrary HTML in WebView2.
static void PreserveSafeEmptyAnchors(cmark_node* parent) {
    for (cmark_node* node = cmark_node_first_child(parent); node;) {
        cmark_node* next = cmark_node_next(node);
        cmark_node_type type = cmark_node_get_type(node);
        if (type == CMARK_NODE_HTML_BLOCK || type == CMARK_NODE_HTML_INLINE) {
            Str raw = DupCmarkChunk(&node->as.literal);
            Str id;
            if (ParseSafeEmptyAnchor(raw, &id)) {
                cmark_node_type customType =
                    type == CMARK_NODE_HTML_BLOCK ? CMARK_NODE_CUSTOM_BLOCK : CMARK_NODE_CUSTOM_INLINE;
                cmark_node* replacement = NewSafeAnchorNode(customType, id);
                if (replacement && cmark_node_insert_before(node, replacement)) {
                    cmark_node_unlink(node);
                    cmark_node_free(node);
                } else if (replacement) {
                    cmark_node_free(replacement);
                }
                str::Free(raw);
                node = next;
                continue;
            }

            Str openId;
            cmark_node* close = next;
            if (type == CMARK_NODE_HTML_INLINE && ParseSafeAnchorOpen(raw, &openId) && close &&
                cmark_node_get_type(close) == CMARK_NODE_HTML_INLINE) {
                Str closeRaw = DupCmarkChunk(&close->as.literal);
                str::TrimWSInPlace(closeRaw, str::TrimOpt::Both);
                if (str::Eq(closeRaw, StrL("</a>"))) {
                    next = cmark_node_next(close);
                    cmark_node* replacement = NewSafeAnchorNode(CMARK_NODE_CUSTOM_INLINE, openId);
                    if (replacement && cmark_node_insert_before(node, replacement)) {
                        cmark_node_unlink(node);
                        cmark_node_free(node);
                        cmark_node_unlink(close);
                        cmark_node_free(close);
                    } else if (replacement) {
                        cmark_node_free(replacement);
                    }
                }
                str::Free(closeRaw);
            }
            str::Free(raw);
        } else {
            PreserveSafeEmptyAnchors(node);
        }
        node = next;
    }
}

// Give every heading an "<a id="slug"></a>" so in-document links like
// "[INTR_STATE](#intr_state)" have something to jump to (#5883). Uses the same
// MarkdownHeadingSlug() as the ToC, so the two can't disagree, and the same
// validated-anchor node as PreserveSafeEmptyAnchors() so cmark's safe renderer
// keeps it.
static void AddHeadingAnchors(cmark_node* doc) {
    for (cmark_node* node = cmark_node_first_child(doc); node; node = cmark_node_next(node)) {
        if (cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }
        Str title = ExtractHeadingTitle(node);
        if (!title) {
            continue;
        }
        Str slug = MarkdownHeadingSlug(nullptr, title);
        if (slug) {
            cmark_node* anchor = NewSafeAnchorNode(CMARK_NODE_CUSTOM_BLOCK, slug);
            if (anchor && !cmark_node_insert_before(node, anchor)) {
                cmark_node_free(anchor);
            }
        }
        str::Free(slug);
        str::Free(title);
    }
}

static char* MarkdownToHtmlBody(Str markdown) {
    if (!markdown) {
        return nullptr;
    }

    EnsureCmarkPluginsRegistered();

    int options = CMARK_OPT_DEFAULT;
    cmark_parser* parser = cmark_parser_new(options);
    AttachGfmExtensions(parser);
    cmark_parser_feed(parser, markdown.s, (size_t)markdown.len);
    cmark_node* doc = cmark_parser_finish(parser);
    if (!doc) {
        cmark_parser_free(parser);
        return nullptr;
    }

    RewriteMarkdownLinks(doc);
    PreserveSafeEmptyAnchors(doc);
    AddHeadingAnchors(doc);

    // Render before cmark_parser_free(); the extensions list is owned by the parser.
    cmark_llist* extensions = cmark_parser_get_syntax_extensions(parser);
    char* body = cmark_render_html(doc, options, extensions);
    cmark_parser_free(parser);
    cmark_node_free(doc);
    return body;
}

Str MarkdownToHtmlPage(Str markdown) {
    char* body = MarkdownToHtmlBody(markdown);
    if (!body) {
        return {};
    }

    str::Builder html;
    html.Append(
        StrL("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
             "<style>"));
    html.Append(Str(MarkdownPageCssTemp()));
    html.Append(StrL("</style></head><body>"));
    html.Append(Str(body));
    html.Append(StrL("</body></html>"));
    cmark_mem* mem = cmark_get_default_mem_allocator();
    mem->free(body);

    return html.TakeStr();
}

bool MarkdownToc_UnitTestHtmlLinks() {
    // GitHub's slug rules: '_' survives, punctuation is dropped, each space
    // becomes its own '-'. Generated register docs link to "#intr_state" and
    // "#adc_intr_ctl--trans_en" (#5883).
    struct {
        Str title;
        Str want;
    } slugs[] = {
        {StrL("INTR_STATE"), StrL("intr_state")},
        {StrL("adc_chn0_filter_ctl"), StrL("adc_chn0_filter_ctl")},
        {StrL("adc_intr_ctl . TRANS_EN"), StrL("adc_intr_ctl--trans_en")},
        {StrL("Hello World! (again)"), StrL("hello-world-again")},
        {StrL("Advanced options / settings"), StrL("advanced-options--settings")},
        {StrL("  padded  "), StrL("padded")},
    };
    for (auto& t : slugs) {
        Str slug = MarkdownHeadingSlug(nullptr, t.title);
        bool ok = str::Eq(slug, t.want);
        str::Free(slug);
        if (!ok) {
            return false;
        }
    }

    // headings get an anchor with that slug, so "#intr_state" has a target
    Str headings = StrL("## INTR_STATE\n\ntext\n\n### adc_intr_ctl . TRANS_EN\n\nmore\n");
    char* headingsHtml = MarkdownToHtmlBody(headings);
    if (!headingsHtml) {
        return false;
    }
    Str hh(headingsHtml);
    bool headingAnchorsOk = str::Contains(hh, StrL("<a id=\"intr_state\"></a>")) &&
                            str::Contains(hh, StrL("<a id=\"adc_intr_ctl--trans_en\"></a>"));
    cmark_get_default_mem_allocator()->free(headingsHtml);
    if (!headingAnchorsOk) {
        return false;
    }

    TempStr url = MarkdownLinkToHtmlTemp(StrL("other.md"));
    if (!str::Eq(url, StrL("other.html"))) {
        return false;
    }
    url = MarkdownLinkToHtmlTemp(StrL("sub/page.markdown#heading"));
    if (!str::Eq(url, StrL("sub/page.html#heading"))) {
        return false;
    }
    url = MarkdownLinkToHtmlTemp(StrL("UPPER.MD?x=1#part"));
    if (!str::Eq(url, StrL("UPPER.html?x=1#part"))) {
        return false;
    }
    bool linksOk = !MarkdownLinkToHtmlTemp(StrL("https://example.com/readme.md")) &&
                   !MarkdownLinkToHtmlTemp(StrL("//example.com/readme.md")) &&
                   !MarkdownLinkToHtmlTemp(StrL("#heading"));

    Str markdown = StrL(
        "<a id=\"finding-14\"></a>\n\n"
        "before <a id=\"inline-anchor\"></a> after\n\n"
        "<a id=\"bad\" onclick=\"alert(1)\"></a>\n\n"
        "<script>alert(2)</script>\n");
    char* body = MarkdownToHtmlBody(markdown);
    if (!body) {
        return false;
    }
    Str html(body);
    bool anchorsOk = str::Contains(html, StrL("<a id=\"finding-14\"></a>")) &&
                     str::Contains(html, StrL("<a id=\"inline-anchor\"></a>")) &&
                     !str::Contains(html, StrL("onclick")) && !str::Contains(html, StrL("<script>"));
    cmark_mem* mem = cmark_get_default_mem_allocator();
    mem->free(body);
    return linksOk && anchorsOk;
}
