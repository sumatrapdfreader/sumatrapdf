/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"

#include "base/HtmlTags.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "GumboHtmlParser.h"

extern "C" {
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "node.h"
}
#include "MarkdownToc.h"

static constexpr SeqStrings kMarkdownExtensions = ".md\0.markdown\0";
static constexpr SeqStrings kHtmlExtensions = ".html\0.htm\0.xhtml\0";

static Str FindFileExt(Str path, SeqStrings extensions) {
    for (Str ext = SeqStrFirst(extensions); ext; ext = SeqStrNext(ext)) {
        if (str::EndsWithI(path, ext)) {
            return ext;
        }
    }
    return {};
}

static void CollectMdInDir(Str dir, bool htmlMode, int depth, StrVec& out) {
    DirIter di(dir);
    di.includeDirs = depth != 0;
    for (DirIterEntry* de : di) {
        if (IsDirectory(de)) {
            CollectMdInDir(de->filePath, htmlMode, depth - 1, out);
        } else if (IsRegularFile(de) && FindFileExt(de->name, htmlMode ? kHtmlExtensions : kMarkdownExtensions)) {
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

template <typename F>
static void VisitMarkdownNodes(cmark_node* doc, cmark_node_type type, F visit) {
    cmark_iter* iter = cmark_iter_new(doc);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node* node = cmark_iter_get_node(iter);
        if (ev == CMARK_EVENT_ENTER && cmark_node_get_type(node) == type) {
            visit(node);
        }
    }
    cmark_iter_free(iter);
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

    VisitMarkdownNodes(doc, CMARK_NODE_HEADING, [&](cmark_node* node) {
        Str title = ExtractHeadingTitle(node);
        if (len(title) == 0) {
            return;
        }
        VecAppend(headingsOut, {title, MarkdownHeadingSlug(title), cmark_node_get_heading_level(node)});
    });
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

    int numThreads = ClampI(CpuCoreCount() - 2, 1, n);

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
pre[data-lang]::before { content: attr(data-lang); display: block; margin-bottom: 8px; color: var(--muted);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif; }
.code-block { position: relative; }
.code-copy { position: absolute; top: 8px; right: 8px; padding: 5px; line-height: 0; color: var(--muted);
  background: var(--code-bg); border: 1px solid var(--border); border-radius: 6px; cursor: pointer; opacity: 0; }
.code-block:hover .code-copy, .code-copy:focus-visible { opacity: 1; }
.code-copy:hover { color: var(--fg); }
::highlight(comment), ::highlight(quote) { color: var(--syntax-comment); }
::highlight(keyword), ::highlight(storage), ::highlight(at-rule), ::highlight(doctype), ::highlight(important),
  ::highlight(section) { color: var(--syntax-keyword); }
::highlight(string), ::highlight(regexp), ::highlight(attribute-value), ::highlight(link), ::highlight(raw)
  { color: var(--syntax-string); }
::highlight(numeric), ::highlight(boolean), ::highlight(constant), ::highlight(symbol), ::highlight(character-entity),
  ::highlight(anchor), ::highlight(entity) { color: var(--syntax-constant); }
::highlight(function), ::highlight(decorator), ::highlight(animation) { color: var(--syntax-function); }
::highlight(type), ::highlight(support), ::highlight(selector) { color: var(--syntax-type); }
::highlight(variable), ::highlight(interpolation) { color: var(--syntax-variable); }
::highlight(property), ::highlight(key), ::highlight(attribute-name) { color: var(--syntax-constant); }
::highlight(tag), ::highlight(inserted) { color: var(--syntax-tag); }
::highlight(deleted) { color: var(--syntax-keyword); }
blockquote { margin: 0; padding: 0 1em; color: var(--muted); border-left: .25em solid var(--border); }
table { border-collapse: collapse; }
table th, table td { border: 1px solid var(--border); padding: 6px 13px; }
img { max-width: 100%%; }
)";

struct CodeBlockCounts {
    int mermaid = 0;
    // fences that name a language other than mermaid: highlighted
    int withLang = 0;
    // fences without a language
    int plain = 0;
};

// cmark emits <pre><code class="language-mermaid">…</code></pre> for ```mermaid
// fences. mermaid.js looks for <pre class="mermaid"> (or elements with that class).
// Other languages get <pre data-lang="python">, which the page shows as a label.
static CodeBlockCounts RewriteCodeBlocks(str::Builder& out, Str body) {
    CodeBlockCounts counts;
    for (Str rest = body; rest;) {
        Str after;
        if (!str::Cut(rest, StrL("<pre><code>"), nullptr, &after)) {
            break;
        }
        counts.plain++;
        rest = after;
    }

    const Str kOpen = StrL("<pre><code class=\"language-");
    const Str kCloseCodePre = StrL("</code></pre>");
    constexpr int kMaxFenceAttrLen = 64;

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
        Str lang(attrs.s, langLen);
        if (!str::EqI(lang, StrL("mermaid"))) {
            // cmark has already HTML-escaped the language name
            out.Append(StrL("<pre data-lang=\""));
            out.Append(lang);
            out.Append(StrL("\"><code class=\"language-"));
            out.Append(Str(attrs.s, len(attrs) + 1));
            counts.withLang++;
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
        counts.mermaid++;
    }
    return counts;
}

// mermaid.min.js lives in IDR_EMBEDDED_PAK and mermaid-run.js is kMermaidRunJs,
// both served from the markdown virtual host (GetDataForUrl). Not inline, so
// kMarkdownCsp can allow just these two scripts. Must match kMdVirtualHost in
// MarkdownModel.cpp.
static const char kMermaidBootstrap[] = R"HTML(
<script src="https://sumatrapdf.markdown/mermaid.min.js"></script>
<script src="https://sumatrapdf.markdown/mermaid-run.js"></script>
)HTML";

static const char kMermaidRunJs[] = R"JS(
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
)JS";

// Raw HTML in markdown is rendered (#6380), so the page must not run the
// document's script (e.g. <img onerror=...>): only our mermaid and code scripts.
static const char kMarkdownCsp[] =
    "<meta http-equiv=\"Content-Security-Policy\" content=\""
    "default-src 'self' data:; img-src * data:; media-src * data:; style-src 'self' 'unsafe-inline'; "
    "script-src https://sumatrapdf.markdown/mermaid.min.js https://sumatrapdf.markdown/mermaid-run.js "
    "https://sumatrapdf.markdown/microlighter.min.js https://sumatrapdf.markdown/code-run.js; "
    "object-src 'none'; frame-src 'none'; base-uri 'none'; form-action 'none'\">";

Str MermaidRunJs() {
    return Str(kMermaidRunJs, (int)(sizeof(kMermaidRunJs) - 1));
}

// microlighter.min.js (ext/, in IDR_EMBEDDED_PAK) colors code blocks that name a
// language; code-run.js is kCodeRunJs. Both from the markdown virtual host.
static const char kCodeHighlightScript[] =
    "<script src=\"https://sumatrapdf.markdown/microlighter.min.js\"></script>\n";
static const char kCodeRunScript[] = "<script src=\"https://sumatrapdf.markdown/code-run.js\"></script>\n";

// A copy button on every code block, then syntax coloring through the CSS Custom
// Highlight API (where missing, code stays plain).
static const char kCodeRunJs[] = R"JS(
(function () {
  var svg = '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><path d="';
  var copyIcon = svg + 'M0 6.75C0 5.78.78 5 1.75 5h1.5a.75.75 0 0 1 0 1.5h-1.5a.25.25 0 0 0-.25.25v7.5c0 .14.11.25.25.25h7.5a.25.25 0 0 0 .25-.25v-1.5a.75.75 0 0 1 1.5 0v1.5A1.75 1.75 0 0 1 9.25 16h-7.5A1.75 1.75 0 0 1 0 14.25Zm5-5C5 .78 5.78 0 6.75 0h7.5C15.22 0 16 .78 16 1.75v7.5A1.75 1.75 0 0 1 14.25 11h-7.5A1.75 1.75 0 0 1 5 9.25Zm1.75-.25a.25.25 0 0 0-.25.25v7.5c0 .14.11.25.25.25h7.5a.25.25 0 0 0 .25-.25v-7.5a.25.25 0 0 0-.25-.25Z"/></svg>';
  var doneIcon = svg + 'M13.78 4.22a.75.75 0 0 1 0 1.06l-7.25 7.25a.75.75 0 0 1-1.06 0L2.22 9.28a.75.75 0 0 1 1.06-1.06L6 10.94l6.72-6.72a.75.75 0 0 1 1.06 0Z"/></svg>';
  document.querySelectorAll("pre > code").forEach(function (code) {
    var pre = code.parentElement;
    var box = document.createElement("div");
    box.className = "code-block";
    pre.parentNode.insertBefore(box, pre);
    box.appendChild(pre);
    var btn = document.createElement("button");
    btn.type = "button";
    btn.className = "code-copy";
    btn.title = "Copy";
    btn.innerHTML = copyIcon;
    btn.addEventListener("click", function () {
      navigator.clipboard.writeText(code.textContent).then(function () {
        btn.innerHTML = doneIcon;
        setTimeout(function () { btn.innerHTML = copyIcon; }, 1500);
      });
    });
    box.appendChild(btn);
  });
  if (window.microlighter && window.CSS && CSS.highlights && window.Highlight) {
    microlighter.highlightAll().catch(function () {});
  }
})();
)JS";

Str CodeRunJs() {
    return Str(kCodeRunJs, (int)(sizeof(kCodeRunJs) - 1));
}

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
    // code colors: GitHub's light or dark palette
    Str syntax = dark ? StrL(
                            "--syntax-comment:#8b949e; --syntax-keyword:#ff7b72; --syntax-string:#a5d6ff; "
                            "--syntax-constant:#79c0ff; --syntax-function:#d2a8ff; --syntax-type:#d2a8ff; "
                            "--syntax-variable:#ffa657; --syntax-tag:#7ee787;")
                      : StrL(
                            "--syntax-comment:#6e7781; --syntax-keyword:#cf222e; --syntax-string:#0a3069; "
                            "--syntax-constant:#0550ae; --syntax-function:#8250df; --syntax-type:#8250df; "
                            "--syntax-variable:#953800; --syntax-tag:#116329;");
    return fmt(kMarkdownPageCssFmt, str::JoinTemp(cssVars, StrL(" "), syntax));
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

    Str ext = FindFileExt(path, kMarkdownExtensions);
    if (len(ext) == 0) {
        return {};
    }

    Str base(path.s, len(path) - len(ext));
    Str suffix(url.s + len(path), len(url) - len(path));
    return fmt("%s.html%s", base, suffix);
}

static void RewriteMarkdownLinks(cmark_node* doc) {
    VisitMarkdownNodes(doc, CMARK_NODE_LINK, [](cmark_node* node) {
        const char* url = cmark_node_get_url(node);
        if (!url) {
            return;
        }
        TempStr htmlUrl = MarkdownLinkToHtmlTemp(Str(url));
        if (htmlUrl) {
            cmark_node_set_url(node, CStrTemp(htmlUrl));
        }
    });
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

static bool InsertSafeAnchorBefore(cmark_node* target, cmark_node_type type, Str id) {
    cmark_node* node = cmark_node_new(type);
    if (!node) {
        return false;
    }
    TempStr html = fmt("<a id=\"%s\"></a>", id);
    if (!cmark_node_set_on_enter(node, CStrTemp(html)) || !cmark_node_insert_before(target, node)) {
        cmark_node_free(node);
        return false;
    }
    return true;
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
            if (InsertSafeAnchorBefore(node, customType, id)) {
                cmark_node_free(node);
                if (close) {
                    cmark_node_free(close);
                }
            }
        }
        node = next;
    }
}

// Give headings anchors with the TOC's slugs so links like "#intr_state"
// reach their headings through cmark's safe renderer (#5883).
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
            InsertSafeAnchorBefore(node, CMARK_NODE_CUSTOM_BLOCK, slug);
        }
        str::Free(slug);
        str::Free(title);
    }
}

static char* MarkdownToHtmlBody(Str markdown, int options = CMARK_OPT_DEFAULT) {
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
    char* body = cmark_render_html(doc, options, extensions);
    cmark_parser_free(parser);
    cmark_node_free(doc);
    return body;
}

// Convert markdown source to a full HTML page (body only is rendered with cmark-gfm).
Str MarkdownToHtmlPage(Str markdown) {
    // raw HTML is kept; tagfilter and kMarkdownCsp keep it from running script
    int options = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE;
    if (gSettings && gSettings->markdownUI.preserveLineBreaks) {
        options |= CMARK_OPT_HARDBREAKS;
    }
    char* body = MarkdownToHtmlBody(markdown, options);
    if (!body) {
        return {};
    }

    str::Builder html;
    html.Append(
        StrL("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"));
    html.Append(Str(kMarkdownCsp, (int)(sizeof(kMarkdownCsp) - 1)));
    html.Append(StrL("<style>"));
    html.Append(Str(MarkdownPageCssTemp()));
    html.Append(StrL("</style></head><body>"));
    CodeBlockCounts counts = RewriteCodeBlocks(html, Str(body));
    cmark_mem* mem = cmark_get_default_mem_allocator();
    mem->free(body);

    // Mermaid diagrams and code coloring need JS (WebView2). Fixed-page MuPDF path has no scripts.
    if (counts.mermaid > 0) {
        html.Append(Str(kMermaidBootstrap, (int)(sizeof(kMermaidBootstrap) - 1)));
    }
    if (counts.withLang > 0) {
        html.Append(Str(kCodeHighlightScript, (int)(sizeof(kCodeHighlightScript) - 1)));
    }
    if (counts.withLang + counts.plain > 0) {
        html.Append(Str(kCodeRunScript, (int)(sizeof(kCodeRunScript) - 1)));
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
    CodeBlockCounts n = RewriteCodeBlocks(rewritten, Str(body));
    cmark_get_default_mem_allocator()->free(body);
    Str html = ToStr(rewritten);
    bool ok = (n.mermaid == 1) && (n.withLang == 0) && str::Contains(html, StrL("<pre class=\"mermaid\">")) &&
              str::Contains(html, StrL("flowchart TD")) && !str::Contains(html, StrL("language-mermaid"));

    // other language fences keep their class and get a data-lang label (#6433);
    // fences without a language are only counted
    body = MarkdownToHtmlBody(StrL("```js\nnot mermaid\n```\n\n```\nplain\n```\n"));
    if (!body) {
        return false;
    }
    str::Builder plainB;
    CodeBlockCounts nPlain = RewriteCodeBlocks(plainB, Str(body));
    cmark_get_default_mem_allocator()->free(body);
    Str plain = ToStr(plainB);
    ok = ok && (nPlain.mermaid == 0) && (nPlain.withLang == 1) && (nPlain.plain == 1) &&
         str::Contains(plain, StrL("<pre data-lang=\"js\"><code class=\"language-js\">")) &&
         str::Contains(plain, StrL("<pre><code>plain")) && !str::Contains(plain, StrL("pre class=\"mermaid\""));
    return ok;
}

bool MarkdownToc_TestLineBreaks() {
    Str markdown = StrL(
        "soft\nline\n\nhard  \nline\n\nslash\\\nline\n\n"
        "`inline\ncode`\n\n```text\ncode\nline\n```\n");
    Str tail = StrL(
        "<p>hard<br />\nline</p>\n<p>slash<br />\nline</p>\n"
        "<p><code>inline code</code></p>\n"
        "<pre><code class=\"language-text\">code\nline\n</code></pre>\n");
    struct {
        int options;
        Str firstParagraph;
    } cases[] = {{CMARK_OPT_DEFAULT, StrL("<p>soft\nline</p>\n")},
                 {CMARK_OPT_HARDBREAKS, StrL("<p>soft<br />\nline</p>\n")}};
    for (auto& c : cases) {
        char* body = MarkdownToHtmlBody(markdown, c.options);
        if (!body) {
            return false;
        }
        Str expected = str::JoinTemp(c.firstParagraph, tail);
        bool ok = str::Eq(Str(body), expected);
        cmark_get_default_mem_allocator()->free(body);
        if (!ok) {
            return false;
        }
    }
    return true;
}
