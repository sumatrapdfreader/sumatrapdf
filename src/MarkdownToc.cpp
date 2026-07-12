/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirIter.h"
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
    const char* exts[] = {"table", "strikethrough", "autolink", "tagfilter", "tasklist", "autoheaderid"};
    for (const char* ext : exts) {
        cmark_syntax_extension* syntax = cmark_find_syntax_extension(ext);
        if (syntax) {
            cmark_parser_attach_syntax_extension(parser, syntax);
        }
    }
}

static void AppendSlugChar(str::Builder* out, unsigned int c) {
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        out->AppendChar((char)c);
        return;
    }
    if (c >= 0x80) {
        out->AppendChar((char)c);
        return;
    }
    out->AppendChar('-');
}

// Matches ext/cmark-gfm/extensions/autoheaderid.c slug rules.
Str MarkdownHeadingSlug(Arena* a, Str title) {
    str::Builder out;
    bool minusPending = false;
    const u8* p = (const u8*)title.s;
    const u8* e = p + title.len;
    while (p < e) {
        unsigned int c = *p++;
        if (c >= 0x80) {
            if (c >= 0xF0) {
                goto bad;
            }
            if (c >= 0xE0) {
                if (p + 2 > e) {
                    goto bad;
                }
                c = ((c & 0x0F) << 12) + ((p[0] & 0x3f) << 6) + (p[1] & 0x3f);
                p += 2;
            } else if (c >= 0xC0) {
                if (p + 1 > e) {
                    goto bad;
                }
                c = ((c & 0x1F) << 6) + (p[0] & 0x3f);
                p += 1;
            } else {
                goto bad;
            }
        }

        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        } else if (c >= 'a' && c <= 'z') {
        } else if (c >= '0' && c <= '9') {
        } else if (c >= 0xa0 && c <= 0xbf) {
            goto bad;
        } else if ((c >= 0xc0 && c <= 0xc5) || (c >= 0xE0 && c <= 0xeb) || (c >= 0x100 && c <= 0x105)) {
            c = 'a';
        } else if (c == 0xC7 || c == 0xE7) {
            c = 'c';
        } else if ((c >= 0xc8 && c <= 0xCB) || (c >= 0xE8 && c <= 0xeb)) {
            c = 'e';
        } else if ((c >= 0xcc && c <= 0xcf) || (c >= 0xEC && c <= 0xef)) {
            c = 'i';
        } else if (c == 0xD1 || c == 0xF1) {
            c = 'n';
        } else if ((c >= 0xd2 && c <= 0xd8) || (c >= 0xEC && c <= 0xef)) {
            c = 'o';
        } else if ((c >= 0xd9 && c <= 0xdc) || (c >= 0xF9 && c <= 0xfc)) {
            c = 'u';
        } else if (c >= 0x80) {
        } else {
        bad:
            c = '-';
        }

        if (c == '-') {
            minusPending = true;
        } else if (c) {
            if (minusPending && out.len > 0) {
                out.AppendChar('-');
            }
            AppendSlugChar(&out, c);
            minusPending = false;
        }
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
    if (!tocsOut.SetSize(n)) {
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
    if (numThreads < 1) {
        numThreads = 1;
    }
    if (numThreads > n) {
        numThreads = n;
    }

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

Str MarkdownToHtmlPage(Str markdown) {
    if (!markdown) {
        return {};
    }

    EnsureCmarkPluginsRegistered();

    int options = CMARK_OPT_UNSAFE | CMARK_OPT_LIBERAL_HTML_TAG;
    cmark_parser* parser = cmark_parser_new(options);
    AttachGfmExtensions(parser);
    cmark_parser_feed(parser, markdown.s, (size_t)markdown.len);
    cmark_node* doc = cmark_parser_finish(parser);
    if (!doc) {
        cmark_parser_free(parser);
        return {};
    }

    // Render before cmark_parser_free(); the extensions list is owned by the parser.
    cmark_llist* extensions = cmark_parser_get_syntax_extensions(parser);
    char* body = cmark_render_html(doc, options, extensions);
    cmark_parser_free(parser);
    cmark_node_free(doc);
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