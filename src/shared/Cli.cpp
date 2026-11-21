/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/JsonParser.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "EngineAll.h"
struct MainWindow;
struct WindowTab;
#include "PdfToolsCommon.h"
#include "Cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>

// exit codes, the same for every command
constexpr int kCliOk = 0;
// the command ran and failed (a file could not be written, ...)
constexpr int kCliFailed = 1;
// the command line was wrong: an unknown command or option, a missing argument
constexpr int kCliUsage = 2;
// the document doesn't exist or isn't one SumatraPDF can open
constexpr int kCliCantOpen = 3;
// the document is encrypted and no password, or a wrong one, was given
constexpr int kCliPassword = 4;
// the command doesn't apply to this kind of document (e.g. a PDF tool on an EPUB)
constexpr int kCliNotSupported = 5;

struct CliCtx;
using CliCmdFn = int (*)(CliCtx& cx);

struct CliCmd {
    const char* name;
    // after the command name in the usage line
    const char* args;
    const char* summary;
    // the command's own options, a line each ("--name <v>  what it does")
    const char* options;
    // example invocations, a line each, without "SumatraPDF cli "
    const char* examples;
    CliCmdFn fn;
};

struct CliCtx {
    const CliCmd* cmd = nullptr;
    bool json = false;
    Str password;
    StrVec positional;
    // --name value and --flag (value "1"), in order
    StrVec optNames;
    StrVec optValues;
};

//--- output

static void Out(Str s) {
    if (len(s) > 0) {
        fwrite(s.s, 1, (size_t)len(s), stdout);
    }
}

static void Err(Str s) {
    if (len(s) > 0) {
        fwrite(s.s, 1, (size_t)len(s), stderr);
    }
}

// a JSON string literal, quotes included
static TempStr JStr(Str s) {
    return fmt("\"%s\"", json::EscapeStrTemp(s));
}

// Reports an error and returns its exit code. With --json the error is the
// output: {"error": {"code": <exit code>, "message": ...}}
static int Fail(CliCtx& cx, int code, Str msg) {
    if (cx.json) {
        Out(fmt("{\"error\":{\"code\":%d,\"message\":%s}}\n", code, JStr(msg)));
    } else {
        TempStr who = cx.cmd ? fmt("SumatraPDF cli %s", Str(cx.cmd->name)) : str::DupTemp(StrL("SumatraPDF cli"));
        Err(fmt("%s: %s\n", who, msg));
    }
    return code;
}

//--- options

static bool HasOpt(CliCtx& cx, Str name) {
    for (int i = 0; i < len(cx.optNames); i++) {
        if (str::Eq(cx.optNames[i], name)) {
            return true;
        }
    }
    return false;
}

// the last value given for --name, or empty
static Str OptValue(CliCtx& cx, Str name) {
    Str res;
    for (int i = 0; i < len(cx.optNames); i++) {
        if (str::Eq(cx.optNames[i], name)) {
            res = cx.optValues[i];
        }
    }
    return res;
}

// the options that take a value; any other --name is a flag
static bool OptTakesValue(Str name) {
    static const char* names[] = {"--pages", "--password", "--password-env", "-o", "--output", "--dpi", "--format"};
    for (const char* n : names) {
        if (str::Eq(name, Str(n))) {
            return true;
        }
    }
    return false;
}

//--- documents

struct CliPasswordUI : PasswordUI {
    Str password;
    bool asked = false;
    bool given = false;
    Str GetPassword(Str, u8*, u8*, bool* saveKey) override {
        *saveKey = false;
        asked = true;
        if (given || len(password) == 0) {
            return {};
        }
        given = true;
        return str::Dup(password);
    }
};

// Opens the document, or reports why it can't and sets *code. Chaptered
// documents (EPUB, ...) are laid out in full, so page numbers mean what the
// window shows.
static EngineBase* OpenDoc(CliCtx& cx, Str path, int* code) {
    if (!file::Exists(path)) {
        *code = Fail(cx, kCliCantOpen, fmt("'%s' doesn't exist", path));
        return nullptr;
    }
    CliPasswordUI pwdUI;
    pwdUI.password = cx.password;
    EngineBase* engine = CreateEngineFromFile(path, &pwdUI, true);
    if (!engine) {
        if (pwdUI.asked) {
            Str why = len(cx.password) > 0 ? StrL("the password is wrong") : StrL("encrypted: give --password");
            *code = Fail(cx, kCliPassword, fmt("'%s': %s", path, why));
        } else {
            *code = Fail(cx, kCliCantOpen, fmt("'%s' isn't a document SumatraPDF can open", path));
        }
        return nullptr;
    }
    EnsureFullLayout(engine);
    return engine;
}

// the pages --pages picks ("1-3,7,10-N"), all pages without it
static bool PagesArg(CliCtx& cx, EngineBase* engine, Vec<int>& pages, int* code) {
    int n = engine->PageCount();
    Str s = OptValue(cx, StrL("--pages"));
    if (len(s) == 0) {
        for (int i = 1; i <= n; i++) {
            VecAppend(pages, i);
        }
        return true;
    }
    if (!ParseDeletePages(s, n, pages)) {
        *code = Fail(cx, kCliUsage, fmt("--pages '%s' isn't a page range of 1-%d, e.g. 1-3,7,10-N", s, n));
        return false;
    }
    return true;
}

// the page's own label (i, ii, A-1) when it has one other than its number
static TempStr PageLabelTemp(EngineBase* engine, int pageNo) {
    TempStr label = engine->GetPageLabeTemp(pageNo);
    if (len(label) == 0 || str::Eq(label, fmt("%d", pageNo))) {
        return {};
    }
    return label;
}

// "page": 3 plus "label": "iii" when the page has one
static TempStr JPage(EngineBase* engine, int pageNo) {
    TempStr label = PageLabelTemp(engine, pageNo);
    if (len(label) == 0) {
        return fmt("\"page\":%d", pageNo);
    }
    return fmt("\"page\":%d,\"label\":%s", pageNo, JStr(label));
}

// the document format, as the file's usual extension without the dot
static TempStr FormatNameTemp(Str path) {
    TempStr ext = GetExtForFileTypeTemp(GuessFileType(path, true));
    if (len(ext) == 0) {
        ext = path::GetExtTemp(path);
    }
    str::TrimPrefix(ext, StrL("."));
    return str::ToLowerInPlace(str::DupTemp(ext));
}

//--- info

// a PDF date ("D:20091222171933-05'00'") as ISO 8601 ("2009-12-22T17:19:33-05:00");
// anything else (EPUB dates already are ISO) as it is
static TempStr IsoDateTemp(Str date) {
    Str s = date;
    str::TrimPrefix(s, StrL("D:"));
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    Str rest = str::Parse(s, "%4d%2d%2d%2d%2d%2d", &y, &mo, &d, &h, &mi, &sec);
    if (!rest.s) {
        return str::DupTemp(date);
    }
    TempStr res = fmt("%04d-%02d-%02dT%02d:%02d:%02d", y, mo, d, h, mi, sec);
    if (len(rest) > 0 && rest.s[0] == 'Z') {
        return fmt("%sZ", res);
    }
    int tzH = 0, tzM = 0;
    if (len(rest) >= 3 && (rest.s[0] == '+' || rest.s[0] == '-')) {
        Str tz(rest.s + 1, len(rest) - 1);
        if (!str::Parse(tz, "%2d'%2d", &tzH, &tzM).s) {
            str::Parse(tz, "%2d", &tzH);
        }
        return fmt("%s%c%02d:%02d", res, rest.s[0], tzH, tzM);
    }
    return res;
}

static int CmdInfo(CliCtx& cx) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    struct Prop {
        const char* name;
        DocProp prop;
    };
    static const Prop props[] = {
        {"title", DocProp::Title},           {"author", DocProp::Author},
        {"subject", DocProp::Subject},       {"keywords", DocProp::Keywords},
        {"creator", DocProp::CreatorApp},    {"producer", DocProp::PdfProducer},
        {"created", DocProp::CreationDate},  {"modified", DocProp::ModificationDate},
        {"pdfVersion", DocProp::PdfVersion}, {"encryption", DocProp::Encryption},
    };
    auto propTemp = [&](DocProp prop) {
        TempStr v = engine->GetPropertyTemp(prop);
        bool isDate = prop == DocProp::CreationDate || prop == DocProp::ModificationDate;
        return isDate && len(v) > 0 ? IsoDateTemp(v) : v;
    };
    int nPages = engine->PageCount();
    RectF box = nPages > 0 ? engine->PageMediabox(1) : RectF();
    bool hasToc = engine->HasToc();
    Str path = cx.positional[0];
    str::Builder out;
    if (cx.json) {
        out.Append(fmt("{\"file\":%s,\"format\":%s,\"pages\":%d", JStr(path), JStr(FormatNameTemp(path)), nPages));
        for (const Prop& p : props) {
            TempStr v = propTemp(p.prop);
            if (len(v) > 0) {
                out.Append(fmt(",\"%s\":%s", Str(p.name), JStr(v)));
            }
        }
        out.Append(fmt(",\"pageSize\":{\"width\":%.1f,\"height\":%.1f},\"hasToc\":%s}\n", box.dx, box.dy,
                       hasToc ? StrL("true") : StrL("false")));
    } else {
        out.Append(fmt("file:       %s\nformat:     %s\npages:      %d\n", path, FormatNameTemp(path), nPages));
        for (const Prop& p : props) {
            TempStr v = propTemp(p.prop);
            if (len(v) > 0) {
                TempStr name = fmt("%s:", Str(p.name));
                out.Append(fmt("%-12s%s\n", name, v));
            }
        }
        out.Append(
            fmt("pageSize:   %.1f x %.1f pt\ntoc:        %s\n", box.dx, box.dy, hasToc ? StrL("yes") : StrL("no")));
    }
    Out(ToStr(out));
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- text

static int CmdText(CliCtx& cx) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    if (cx.json) {
        Out(StrL("{\"pages\":["));
    }
    for (int i = 0; i < len(pages); i++) {
        PageText pt = engine->ExtractPageText(pages[i]);
        Str text = pt.text;
        if (cx.json) {
            Out(fmt("%s{%s,\"text\":%s}", i > 0 ? StrL(",") : StrL(""), JPage(engine, pages[i]), JStr(text)));
        } else {
            // pages are separated by a form feed, as pdftotext does
            if (i > 0) {
                Out(StrL("\f"));
            }
            Out(text);
            if (len(text) > 0 && text.s[len(text) - 1] != '\n') {
                Out(StrL("\n"));
            }
        }
        FreePageText(&pt);
    }
    if (cx.json) {
        Out(StrL("]}\n"));
    }
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- search

// a page's text as codepoints, with each one's byte offset (and one past the
// end) for cutting the snippet out of the UTF-8 text
struct Codepoints {
    Vec<u32> cps;
    Vec<int> offsets;
};

static void DecodeText(Str s, Codepoints& out) {
    const u8* p = (const u8*)s.s;
    const u8* start = p;
    const u8* end = p + len(s);
    while (p < end) {
        int n = utf8RuneLen(p);
        if (n < 1 || p + n > end) {
            n = 1;
        }
        u32 cp = p[0];
        if (n == 2) {
            cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
        } else if (n == 3) {
            cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
        } else if (n == 4) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
        }
        VecAppend(out.cps, cp);
        VecAppend(out.offsets, (int)(p - start));
        p += n;
    }
    VecAppend(out.offsets, (int)(p - start));
}

static u32 FoldCase(u32 cp, bool matchCase) {
    if (matchCase || cp > 0xffff) {
        return cp;
    }
    return (u32)towlower((wint_t)cp);
}

static bool IsWordChar(u32 cp) {
    return cp > 0x7f || iswalnum((wint_t)cp) || cp == '_';
}

static int CmdSearch(CliCtx& cx) {
    if (len(cx.positional) < 2) {
        return Fail(cx, kCliUsage, StrL("needs a document and the text to search for"));
    }
    Str query = cx.positional[1];
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    bool matchCase = HasOpt(cx, StrL("--match-case"));
    bool wholeWord = HasOpt(cx, StrL("--whole-word"));
    Codepoints q;
    DecodeText(query, q);
    int nq = len(q.cps);
    if (nq == 0) {
        SafeEngineRelease(&engine);
        return Fail(cx, kCliUsage, StrL("the text to search for is empty"));
    }
    for (u32& cp : q.cps) {
        cp = FoldCase(cp, matchCase);
    }
    constexpr int kContext = 40;
    int nHits = 0;
    str::Builder out;
    for (int pageNo : pages) {
        PageText pt = engine->ExtractPageText(pageNo);
        Codepoints t;
        DecodeText(pt.text, t);
        int n = len(t.cps);
        for (int i = 0; i + nq <= n; i++) {
            bool match = true;
            for (int j = 0; j < nq && match; j++) {
                match = FoldCase(t.cps[i + j], matchCase) == q.cps[j];
            }
            if (match && wholeWord) {
                match = (i == 0 || !IsWordChar(t.cps[i - 1])) && (i + nq == n || !IsWordChar(t.cps[i + nq]));
            }
            if (!match) {
                continue;
            }
            // the hit's box: the union of its characters' boxes, in points
            // from the page's top left
            Rect r;
            for (int j = 0; pt.coords && j < nq && i + j < pt.nCodepoints; j++) {
                Rect c = pt.coords[i + j];
                r = r.IsEmpty() ? c : r.Union(c);
            }
            int from = std::max(0, i - kContext);
            int to = std::min(n, i + nq + kContext);
            TempStr snippet = str::DupTemp(Str(pt.text.s + t.offsets[from], t.offsets[to] - t.offsets[from]));
            snippet = str::ReplaceTemp(snippet, StrL("\n"), StrL(" "));
            if (cx.json) {
                out.Append(fmt("%s{%s,\"rect\":[%d,%d,%d,%d],\"text\":%s}", nHits > 0 ? StrL(",") : StrL(""),
                               JPage(engine, pageNo), r.x, r.y, r.dx, r.dy, JStr(snippet)));
            } else {
                TempStr label = PageLabelTemp(engine, pageNo);
                TempStr where = len(label) > 0 ? fmt("page %d (%s)", pageNo, label) : fmt("page %d", pageNo);
                out.Append(fmt("%s: %s\n", where, snippet));
            }
            nHits++;
            i += nq - 1;
        }
        FreePageText(&pt);
    }
    if (cx.json) {
        Out(fmt("{\"query\":%s,\"count\":%d,\"hits\":[%s]}\n", JStr(query), nHits, ToStr(out)));
    } else {
        Out(ToStr(out));
        Err(fmt("%d hits\n", nHits));
    }
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- toc

static void TocJson(EngineBase* engine, TocItem* item, str::Builder& out) {
    out.Append(StrL("["));
    for (TocItem* it = item; it; it = it->next) {
        out.Append(fmt("%s{\"title\":%s", it != item ? StrL(",") : StrL(""), JStr(it->title)));
        if (it->pageNo > 0) {
            out.Append(fmt(",%s", JPage(engine, it->pageNo)));
        }
        if (it->child) {
            out.Append(StrL(",\"children\":"));
            TocJson(engine, it->child, out);
        }
        out.Append(StrL("}"));
    }
    out.Append(StrL("]"));
}

static void TocText(TocItem* item, int depth, str::Builder& out) {
    for (TocItem* it = item; it; it = it->next) {
        TempStr page = it->pageNo > 0 ? fmt("  %d", it->pageNo) : TempStr{};
        for (int i = 0; i < depth; i++) {
            out.Append(StrL("  "));
        }
        out.Append(fmt("%s%s\n", it->title, page));
        TocText(it->child, depth + 1, out);
    }
}

static int CmdToc(CliCtx& cx) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    TocTree* toc = engine->HasToc() ? engine->GetToc() : nullptr;
    TocItem* first = toc && toc->root ? toc->root->child : nullptr;
    // a tree's root is either a real first item or a placeholder over them
    if (toc && toc->root && !first) {
        first = toc->root;
    }
    str::Builder out;
    if (cx.json) {
        out.Append(StrL("{\"toc\":"));
        TocJson(engine, first, out);
        out.Append(StrL("}\n"));
    } else {
        TocText(first, 0, out);
    }
    Out(ToStr(out));
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- the command table and help

static const CliCmd gCmds[] = {
    {"info", "<file>", "the document's format, page count, size and metadata", "",
     "info book.epub\ninfo report.pdf --json", CmdInfo},
    {"text", "<file>", "the text of the document, page by page (pages are separated by a form feed)",
     "--pages <range>  only these pages, e.g. 1-3,7,10-N", "text paper.pdf\ntext paper.pdf --pages 3-5 --json",
     CmdText},
    {"search", "<file> <text>", "every place the text occurs, with its page number",
     "--match-case     upper and lower case must match\n--whole-word     only whole words\n--pages <range>  "
     "only these pages",
     "search thesis.pdf \"error rate\"\nsearch book.epub Darcy --whole-word --json", CmdSearch},
    {"toc", "<file>", "the table of contents (bookmarks), with page numbers", "",
     "toc manual.chm\ntoc book.epub --json", CmdToc},
};

static const CliCmd* FindCmd(Str name) {
    for (const CliCmd& c : gCmds) {
        if (str::Eq(name, Str(c.name))) {
            return &c;
        }
    }
    return nullptr;
}

static void PrintCmdHelp(const CliCmd& c) {
    str::Builder out;
    out.Append(fmt("SumatraPDF cli %s %s\n\n%s\n\noptions:\n", Str(c.name), Str(c.args), Str(c.summary)));
    if (*c.options) {
        StrVec lines;
        Split(&lines, Str(c.options), StrL("\n"), true);
        for (Str l : lines) {
            out.Append(fmt("  %s\n", l));
        }
    }
    out.Append(StrL("  --json           machine-readable output\n"));
    out.Append(StrL("  --password <pw>  for an encrypted document (or --password-env <NAME>)\n\nexamples:\n"));
    StrVec lines;
    Split(&lines, Str(c.examples), StrL("\n"), true);
    for (Str l : lines) {
        out.Append(fmt("  SumatraPDF cli %s\n", l));
    }
    Out(ToStr(out));
}

static void PrintHelp() {
    str::Builder out;
    out.Append(
        StrL("SumatraPDF cli <command> [options]: read and convert documents from a terminal, a script or "
             "an AI agent.\nWorks on every format SumatraPDF opens: PDF, EPUB, MOBI, FB2, XPS, DjVu, CHM, "
             "comic books and more.\nNever changes the files it is given.\n\ncommands:\n"));
    for (const CliCmd& c : gCmds) {
        out.Append(fmt("  %-10s %s\n", Str(c.name), Str(c.summary)));
    }
    out.Append(
        StrL("\nEvery command takes --json and --help.\n\nexit codes:\n"
             "  0  success\n  1  the command failed\n  2  wrong command line\n"
             "  3  the document doesn't exist or can't be opened\n"
             "  4  the document is encrypted: no password or a wrong one\n"
             "  5  the command doesn't apply to this kind of document\n"));
    Out(ToStr(out));
}

int CliMain(int argc, char** argv) {
    CliCtx cx;
    if (argc < 1 || str::Eq(Str(argv[0]), StrL("--help")) || str::Eq(Str(argv[0]), StrL("-h")) ||
        str::Eq(Str(argv[0]), StrL("help"))) {
        if (argc >= 2 && str::Eq(Str(argv[0]), StrL("help")) && FindCmd(Str(argv[1]))) {
            PrintCmdHelp(*FindCmd(Str(argv[1])));
            return kCliOk;
        }
        PrintHelp();
        return argc < 1 ? kCliUsage : kCliOk;
    }
    cx.cmd = FindCmd(Str(argv[0]));
    for (int i = 1; i < argc; i++) {
        if (str::Eq(Str(argv[i]), StrL("--json"))) {
            cx.json = true;
        }
    }
    if (!cx.cmd) {
        return Fail(cx, kCliUsage, fmt("unknown command '%s' (SumatraPDF cli --help lists them)", Str(argv[0])));
    }
    for (int i = 1; i < argc; i++) {
        Str a(argv[i]);
        if (str::Eq(a, StrL("--help")) || str::Eq(a, StrL("-h"))) {
            PrintCmdHelp(*cx.cmd);
            return kCliOk;
        }
        if (str::Eq(a, StrL("--json"))) {
            continue;
        }
        if (len(a) > 1 && a.s[0] == '-') {
            if (OptTakesValue(a)) {
                if (i + 1 >= argc) {
                    return Fail(cx, kCliUsage, fmt("%s needs a value", a));
                }
                cx.optNames.Append(a);
                cx.optValues.Append(Str(argv[++i]));
            } else {
                cx.optNames.Append(a);
                cx.optValues.Append(StrL("1"));
            }
            continue;
        }
        cx.positional.Append(a);
    }
    cx.password = OptValue(cx, StrL("--password"));
    Str pwdEnv = OptValue(cx, StrL("--password-env"));
    if (len(pwdEnv) > 0) {
        const char* v = getenv(CStrTemp(pwdEnv));
        if (!v) {
            return Fail(cx, kCliUsage, fmt("--password-env: %s isn't set", pwdEnv));
        }
        cx.password = Str(v);
    }
    if (len(cx.positional) < 1 && *cx.cmd->args) {
        return Fail(cx, kCliUsage, fmt("usage: SumatraPDF cli %s %s", Str(cx.cmd->name), Str(cx.cmd->args)));
    }
    // the engines read a few settings (ebook font, ...): the defaults, without
    // reading or writing the user's settings file
    if (!gSettings) {
        gSettings = NewSettings(Str{});
    }
    int res = cx.cmd->fn(cx);
    fflush(stdout);
    fflush(stderr);
    return res;
}
