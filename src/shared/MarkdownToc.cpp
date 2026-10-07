/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"

#include "base/HtmlTags.h"

#include "Theme.h"
#include "GumboHtmlParser.h"

extern "C" {
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "node.h"
}
#include "MarkdownToc.h"

static bool IsMarkdownExt(Str path) {
    return str::EndsWithI(path, StrL(".md")) || str::EndsWithI(path, StrL(".markdown"));
}

static bool IsHtmlExt(Str path) {
    return str::EndsWithI(path, StrL(".html")) || str::EndsWithI(path, StrL(".htm")) ||
           str::EndsWithI(path, StrL(".xhtml"));
}

static bool IsCollectedExt(Str path, bool htmlMode) {
    return htmlMode ? IsHtmlExt(path) : IsMarkdownExt(path);
}

static void CollectMdInDir(Str dir, bool htmlMode, int depth, StrVec& out) {
    DirIter di(dir);
    di.includeDirs = depth != 0;
    for (DirIterEntry* de : di) {
        if (IsDirectory(de)) {
            CollectMdInDir(de->filePath, htmlMode, depth - 1, out);
        } else if (IsRegularFile(de) && IsCollectedExt(de->name, htmlMode)) {
            out.Append(de->filePath);
        }
    }
}

// Collect .md / .markdown (or, in htmlMode, .html / .htm / .xhtml) files under
// baseDir, at most two subdirectory levels deep.
void CollectMarkdownFiles(Str baseDir, Str openedFile, bool htmlMode, StrVec& filesOut) {
    filesOut.Reset();
    if (len(baseDir) == 0) {
        if (openedFile) {
            filesOut.Append(openedFile);
        }
        return;
    }

    constexpr int kMaxTocDirDepth = 2;
    CollectMdInDir(baseDir, htmlMode, kMaxTocDirDepth, filesOut);

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

static void EnsureCmarkPluginsRegistered() {
    [[maybe_unused]] static const bool registered = [] {
        cmark_gfm_core_extensions_ensure_registered();
        return true;
    }();
}

static cmark_parser* CreateMarkdownParser(Str data) {
    EnsureCmarkPluginsRegistered();
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);

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

    cmark_parser_feed(parser, data.s, (size_t)len(data));
    return parser;
}

static const Str kSlugWhitespace = StrL(" \t\n\r");

// Share GitHub-style slugs between TOC links and heading anchors.
static Str MarkdownHeadingSlug(Str title) {
    str::Builder out;
    str::TrimAny(title, kSlugWhitespace.s);
    while (len(title) > 0 && str::ContainsChar(kSlugWhitespace, title.s[len(title) - 1])) {
        title.len--;
    }
    for (int i = 0; i < len(title); i++) {
        u8 c = (u8)title.s[i];
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
        if (c >= 0x80 || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out.AppendChar((char)c);
        } else if (str::ContainsChar(kSlugWhitespace, (char)c)) {
            out.AppendChar('-');
        }
    }
    return out.TakeStr();
}

// cmark literals are length-prefixed and need not be NUL-terminated.
static Str CmarkLiteral(cmark_node* node) {
    cmark_chunk& chunk = node->as.literal;
    return chunk.data && chunk.len > 0 ? Str((char*)chunk.data, (int)chunk.len) : Str{};
}

static void AppendHeadingText(cmark_node* node, str::Builder* out) {
    cmark_node_type type = cmark_node_get_type(node);
    if (type == CMARK_NODE_TEXT || type == CMARK_NODE_CODE) {
        out->Append(CmarkLiteral(node));
        return;
    }
    for (cmark_node* child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        AppendHeadingText(child, out);
    }
}

static Str ExtractHeadingTitle(cmark_node* heading) {
    str::Builder out;
    AppendHeadingText(heading, &out);
    return out.TakeStr();
}

static void ParseMarkdownHeadings(Str data, Vec<MarkdownHeadingItem>& headingsOut) {
    VecReset(headingsOut);
    if (len(data) == 0) {
        return;
    }

    cmark_parser* parser = CreateMarkdownParser(data);
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
        if (len(title) == 0) {
            continue;
        }
        MarkdownHeadingItem item;
        item.title = title;
        item.anchor = MarkdownHeadingSlug(title);
        item.level = cmark_node_get_heading_level(node);
        VecAppend(headingsOut, item);
    }
    cmark_iter_free(iter);
    cmark_node_free(doc);
}

// Extract <h1>..<h6> headings from HTML source, using each heading's id=""
// attribute (when present) as the in-page anchor. headingsOut items are heap-owned.
void ParseHtmlHeadingsData(Str data, Vec<MarkdownHeadingItem>& headingsOut) {
    VecReset(headingsOut);
    if (len(data) == 0) {
        return;
    }
    GumboHtmlParser parser(data);
    int headingLevel = 0;
    Str headingId;
    str::Builder text;
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr) {
        if (tok->IsStartTag() && IsHeadingTag(tok->tag)) {
            headingLevel = (int)(tok->tag - Tag_H1) + 1;
            AttrInfo id = tok->GetAttrByName(StrL("id"));
            headingId = len(id.val) > 0 ? id.val : Str{};
            text.Reset();
            continue;
        }
        if (headingLevel == 0) {
            continue;
        }
        if (tok->IsText()) {
            text.Append(tok->s);
        } else if (tok->IsEndTag() && IsHeadingTag(tok->tag)) {
            // heap (not Temp) entity resolution: this runs on TOC worker threads
            // whose thread-local temp arena would leak on thread exit
            Str title = ResolveHtmlEntities(ToStr(text));
            str::TrimWSInPlace(title, str::TrimOpt::Both);
            if (len(title) > 0) {
                VecAppend(headingsOut, {title, str::Dup(headingId), headingLevel});
            } else {
                str::Free(title);
            }
            headingLevel = 0;
        }
    }
}

struct MdTocParseCtx {
    StrVec* files = nullptr;
    Vec<MarkdownFileToc>* tocs = nullptr;
    bool htmlMode = false;
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
        toc->filePath = str::Dup(path);
        Str data = file::ReadFile(path);
        AutoFree dataOwner(data.s);
        if (ctx->htmlMode) {
            ParseHtmlHeadingsData(data, toc->headings);
        } else {
            ParseMarkdownHeadings(data, toc->headings);
        }
    }
}

// Parse headings from all files in parallel (up to CpuCoreCount() - 2 threads).
void ParseMarkdownTocsParallel(StrVec& files, bool htmlMode, Vec<MarkdownFileToc>& tocsOut) {
    int n = len(files);
    VecReset(tocsOut);
    if (n == 0) {
        return;
    }
    if (!VecResize(tocsOut, n)) {
        return;
    }

    EnsureCmarkPluginsRegistered();

    MdTocParseCtx ctx;
    ctx.files = &files;
    ctx.tocs = &tocsOut;
    ctx.htmlMode = htmlMode;
    AtomicIntSet(&ctx.nextIdx, 0);

    int numThreads = CpuCoreCount() - 2;
    numThreads = std::max(numThreads, 1);
    numThreads = std::min(numThreads, n);

    Vec<ThreadHandle> threads;
    for (int t = 0; t < numThreads; t++) {
        auto fn = MkFunc0(MdTocParseWorker, &ctx);
        VecAppend(threads, StartThread(fn, StrL("MdTocParse")));
    }
    for (ThreadHandle& h : threads) {
        JoinThread(&h, -1);
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
pre.mermaid { background: transparent; text-align: center; overflow: visible; }
pre.mermaid svg { max-width: 100%%; height: auto; }
blockquote { margin: 0; padding: 0 1em; color: var(--muted); border-left: .25em solid var(--border); }
table { border-collapse: collapse; }
table th, table td { border: 1px solid var(--border); padding: 6px 13px; }
img { max-width: 100%%; }
)";

// cmark emits <pre><code class="language-mermaid">…</code></pre> for ```mermaid
// fences. mermaid.js looks for <pre class="mermaid"> (or elements with that class).
// Returns how many blocks were rewritten.
static int RewriteMermaidCodeBlocks(str::Builder& out, Str body) {
    const Str kOpen = StrL("<pre><code class=\"language-");
    const Str kCloseCodePre = StrL("</code></pre>");
    constexpr int kMaxFenceAttrLen = 64;

    int nRewritten = 0;
    Str rest = body;
    while (rest) {
        Str before, afterOpen;
        bool found = str::Cut(rest, kOpen, &before, &afterOpen);
        out.Append(before);
        if (!found) {
            break;
        }
        Str fromOpen(afterOpen.s - len(kOpen), len(afterOpen) + len(kOpen));
        Str attrs, content;
        if (!str::CutChar(afterOpen, '>', &attrs, &content) || len(attrs) > kMaxFenceAttrLen) {
            out.Append(kOpen);
            rest = afterOpen;
            continue;
        }

        int langLen = 0;
        while (langLen < len(attrs) && attrs.s[langLen] != '"' && attrs.s[langLen] != ' ') {
            langLen++;
        }
        if (!str::EqI(Str(attrs.s, langLen), StrL("mermaid"))) {
            out.Append(Str(fromOpen.s, len(kOpen) + len(attrs) + 1));
            rest = content;
            continue;
        }

        Str block;
        if (!str::Cut(content, kCloseCodePre, &block, &rest)) {
            out.Append(fromOpen);
            break;
        }
        // cmark has already HTML-escaped the diagram text.
        out.Append(StrL("<pre class=\"mermaid\">"));
        out.Append(block);
        out.Append(StrL("</pre>"));
        nRewritten++;
    }
    return nRewritten;
}

// mermaid.min.js lives in IDR_EMBEDDED_PAK and is served from the markdown
// virtual host (GetDataForUrl). Must match kMdVirtualHost in MarkdownModel.cpp.
static const char kMermaidBootstrap[] = R"HTML(
<script src="https://sumatrapdf.markdown/mermaid.min.js"></script>
<script>
(function () {
  if (typeof mermaid === "undefined") return;
  var bg = getComputedStyle(document.body).backgroundColor || "";
  var dark = false;
  var m = bg.match(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/i);
  if (m) {
    var r = +m[1], g = +m[2], b = +m[3];
    dark = (0.2126 * r + 0.7152 * g + 0.0722 * b) < 140;
  }
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: "strict",
    theme: dark ? "dark" : "default"
  });
  mermaid.run({ querySelector: "pre.mermaid" }).catch(function () {});
})();
</script>
)HTML";

// page colors follow the document color mode: ThemePageRenderColors gives
// black-on-white (or FixedPageUI overrides) when DocumentColorsFollowTheme
// is off and theme-derived page colors when it's on
static TempStr MarkdownPageCssTemp() {
    Color bgCol;
    Color txtCol = ThemePageRenderColors(bgCol);
    bool dark = !IsLightColor(bgCol);
    bool isDefault = (bgCol == kColWhite) && (txtCol == kColBlack);

    TempStr bg = ColorToCssTemp(bgCol);
    // the default black-on-white gets the classic GitHub palette
    Str fg = isDefault ? StrL("#24292f") : ColorToCssTemp(txtCol);
    Str link = dark ? StrL("#4493f8") : StrL("#0969da");
    Str muted = dark ? StrL("#9198a1") : StrL("#57606a");
    Str border = isDefault ? StrL("#d0d7de") : ColorToCssTemp(AccentColor(bgCol, 25));
    Str codeBg = isDefault ? StrL("#f6f8fa") : ColorToCssTemp(AccentColor(bgCol, 8));

    TempStr cssVars =
        fmt("--bg:%s; --fg:%s; --link:%s; --muted:%s; --border:%s; --code-bg:%s;", bg, fg, link, muted, border, codeBg);
    return fmt(kMarkdownPageCssFmt, cssVars);
}

// Markdown pages are exposed to WebView2 as generated .html resources. Keep
// relative links on that virtual resource graph instead of navigating to the
// source .md name.
static TempStr MarkdownLinkToHtmlTemp(Str url) {
    if (len(url) == 0 || url.s[0] == '#' || str::StartsWith(url, StrL("//"))) {
        return {};
    }

    Str path;
    str::CutChar(url, '?', &path, nullptr);
    str::CutChar(path, '#', &path, nullptr);
    if (str::ContainsChar(path, ':')) {
        return {};
    }

    int extLen = 0;
    if (str::EndsWithI(path, StrL(".markdown"))) {
        extLen = 9;
    } else if (str::EndsWithI(path, StrL(".md"))) {
        extLen = 3;
    } else {
        return {};
    }

    Str base(path.s, len(path) - extLen);
    Str suffix(url.s + len(path), len(url) - len(path));
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
    if (len(id) == 0) {
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

static bool ParseSafeAnchor(Str html, Str suffix, Str* idOut) {
    str::TrimWsBoth(html);
    Str prefix = StrL("<a id=\"");
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
        if (type != CMARK_NODE_HTML_BLOCK && type != CMARK_NODE_HTML_INLINE) {
            PreserveSafeEmptyAnchors(node);
            node = next;
            continue;
        }

        Str raw = CmarkLiteral(node);
        Str id;
        cmark_node* close = nullptr;
        bool matched = ParseSafeAnchor(raw, StrL("\"></a>"), &id);
        if (!matched && type == CMARK_NODE_HTML_INLINE && ParseSafeAnchor(raw, StrL("\">"), &id) && next &&
            cmark_node_get_type(next) == CMARK_NODE_HTML_INLINE) {
            Str closeRaw = CmarkLiteral(next);
            str::TrimWsBoth(closeRaw);
            if (str::Eq(closeRaw, StrL("</a>"))) {
                close = next;
                next = cmark_node_next(close);
                matched = true;
            }
        }
        if (matched) {
            cmark_node_type customType =
                type == CMARK_NODE_HTML_BLOCK ? CMARK_NODE_CUSTOM_BLOCK : CMARK_NODE_CUSTOM_INLINE;
            cmark_node* replacement = NewSafeAnchorNode(customType, id);
            if (replacement && cmark_node_insert_before(node, replacement)) {
                cmark_node_unlink(node);
                cmark_node_free(node);
                if (close) {
                    cmark_node_unlink(close);
                    cmark_node_free(close);
                }
            } else if (replacement) {
                cmark_node_free(replacement);
            }
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
        if (len(title) == 0) {
            continue;
        }
        Str slug = MarkdownHeadingSlug(title);
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
    if (len(markdown) == 0) {
        return nullptr;
    }

    cmark_parser* parser = CreateMarkdownParser(markdown);
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
    char* body = cmark_render_html(doc, CMARK_OPT_DEFAULT, extensions);
    cmark_parser_free(parser);
    cmark_node_free(doc);
    return body;
}

// Convert markdown source to a full HTML page (body only is rendered with cmark-gfm).
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
    int nMermaid = RewriteMermaidCodeBlocks(html, Str(body));
    cmark_mem* mem = cmark_get_default_mem_allocator();
    mem->free(body);

    // Mermaid diagrams need JS (WebView2). Fixed-page MuPDF path has no scripts.
    if (nMermaid > 0) {
        html.Append(Str(kMermaidBootstrap, (int)(sizeof(kMermaidBootstrap) - 1)));
    }
    html.Append(StrL("</body></html>"));

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
        {StrL("\t\r\nPadded\t\r\n"), StrL("padded")},
        {StrL("\v hello \f"), StrL("-hello-")},
        {StrL("ÄCAFÉ 😀"), StrL("ÄcafÉ-😀")},
        {StrL("\xc3X\xa9"), StrL("\xc3x\xa9")},
    };
    for (auto& t : slugs) {
        Str slug = MarkdownHeadingSlug(t.title);
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
    bool linksOk = len(MarkdownLinkToHtmlTemp(StrL("https://example.com/readme.md"))) == 0 &&
                   len(MarkdownLinkToHtmlTemp(StrL("//example.com/readme.md"))) == 0 &&
                   len(MarkdownLinkToHtmlTemp(StrL("#heading"))) == 0;

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

bool MarkdownToc_UnitTestHtmlHeadings() {
    Str html = StrL(
        "<!DOCTYPE html><html><body>\n"
        "<h1 id=\"intro\">Introduction</h1>\n"
        "<p>ignored</p>\n"
        "<h2 id=\"details\">Details &amp; Notes</h2>\n"
        "<h3>No Id Here</h3>\n"
        "<h2 id=\"more\">More <em>emphasis</em></h2>\n"
        "</body></html>");
    Vec<MarkdownHeadingItem> hs;
    ParseHtmlHeadingsData(html, hs);

    struct {
        Str title;
        Str anchor;
        int level;
    } want[] = {
        {StrL("Introduction"), StrL("intro"), 1},
        {StrL("Details & Notes"), StrL("details"), 2}, // &amp; decoded
        {StrL("No Id Here"), StrL(""), 3},             // no id -> empty anchor
        {StrL("More emphasis"), StrL("more"), 2},      // nested inline text kept
    };
    bool ok = (len(hs) == (int)dimof(want));
    for (int i = 0; ok && i < len(hs); i++) {
        MarkdownHeadingItem& h = hs[i];
        Str anchor = h.anchor ? h.anchor : StrL("");
        ok = str::Eq(h.title, want[i].title) && str::Eq(anchor, want[i].anchor) && (h.level == want[i].level);
    }
    for (MarkdownHeadingItem& h : hs) {
        str::Free(h.title);
        str::Free(h.anchor);
    }
    return ok;
}

// ```mermaid fences become <pre class="mermaid">; the page path also injects
// mermaid.min.js when any were rewritten (#5927). Avoid MarkdownToHtmlPage here
// — it needs a live theme for CSS (not set up in -unit-tests).
bool MarkdownToc_UnitTestMermaid() {
    Str md = StrL("before\n\n```mermaid\nflowchart TD\n    A --> B\n```\n\nafter\n");
    char* body = MarkdownToHtmlBody(md);
    if (!body) {
        return false;
    }
    str::Builder rewritten;
    int n = RewriteMermaidCodeBlocks(rewritten, Str(body));
    cmark_get_default_mem_allocator()->free(body);
    Str html = ToStr(rewritten);
    bool ok = (n == 1) && str::Contains(html, StrL("<pre class=\"mermaid\">")) &&
              str::Contains(html, StrL("flowchart TD")) && !str::Contains(html, StrL("language-mermaid"));

    // other language fences stay as cmark emitted them
    body = MarkdownToHtmlBody(StrL("```js\nnot mermaid\n```\n"));
    if (!body) {
        return false;
    }
    str::Builder plainB;
    int nPlain = RewriteMermaidCodeBlocks(plainB, Str(body));
    cmark_get_default_mem_allocator()->free(body);
    Str plain = ToStr(plainB);
    ok = ok && (nPlain == 0) && str::Contains(plain, StrL("language-js")) &&
         !str::Contains(plain, StrL("pre class=\"mermaid\""));
    return ok;
}
