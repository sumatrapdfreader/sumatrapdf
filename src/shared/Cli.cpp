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
#include "Annotation.h"
#include "base/Pixmap.h"
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
    static const char* names[] = {"--pages", "--password", "--password-env", "--new-password", "--new-password-env",
                                  "-o",      "--output",   "--dpi"};
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

//--- annotations

static TempStr HexColorTemp(PdfColor c) {
    if (c == 0 || c == (PdfColor)kColorUnset) {
        return {};
    }
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return fmt("#%02x%02x%02x", r, g, b);
}

static TempStr IsoTimeTemp(time_t t) {
    if (t <= 0) {
        return {};
    }
    struct tm tmv;
#if OS_WIN
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    return fmt("%04d-%02d-%02dT%02d:%02d:%02dZ", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
               tmv.tm_min, tmv.tm_sec);
}

static bool IsTextMarkup(AnnotationType tp) {
    return tp == AnnotationType::Highlight || tp == AnnotationType::Underline || tp == AnnotationType::Squiggly ||
           tp == AnnotationType::StrikeOut;
}

// the page text a highlight (underline, ...) is over: the characters whose
// box's center is in one of its quads
static TempStr MarkedTextTemp(const PageText& pt, const Vec<RectF>& quads) {
    str::Builder out;
    const u8* p = (const u8*)pt.text.s;
    const u8* end = p + len(pt.text);
    for (int i = 0; i < pt.nCodepoints && p < end; i++) {
        int n = utf8RuneLen(p);
        if (n < 1 || p + n > end) {
            n = 1;
        }
        Rect c = pt.coords ? pt.coords[i] : Rect();
        PointF center((float)c.x + (float)c.dx / 2, (float)c.y + (float)c.dy / 2);
        for (const RectF& q : quads) {
            if (q.Contains(center)) {
                out.Append(Str((const char*)p, n));
                break;
            }
        }
        p += n;
    }
    TempStr res = ToStrTemp(out);
    return str::ReplaceTemp(res, StrL("\n"), StrL(" "));
}

static int CmdAnnotations(CliCtx& cx) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    if (!EngineMupdfIsPdf(engine)) {
        SafeEngineRelease(&engine);
        return Fail(cx, kCliNotSupported, StrL("only a PDF has annotations"));
    }
    Vec<int> pages;
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    for (int pageNo : pages) {
        EngineMupdfLoadAnnotsForPage(engine, pageNo);
    }
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    str::Builder out;
    int n = 0;
    for (int pageNo : pages) {
        PageText pt{};
        bool haveText = false;
        for (Annotation* a : annots) {
            if (PageNo(a) != pageNo) {
                continue;
            }
            AnnotationType tp = Type(a);
            // a popup is the note window of another annotation, a widget a form field
            if (tp == AnnotationType::Popup || tp == AnnotationType::Widget) {
                continue;
            }
            RectF r = GetRect(a);
            TempStr marked;
            if (IsTextMarkup(tp)) {
                if (!haveText) {
                    pt = engine->ExtractPageText(pageNo);
                    haveText = true;
                }
                marked = MarkedTextTemp(pt, GetQuadPointsAsRect(a));
            }
            Str contents = Contents(a);
            Str author = Author(a);
            TempStr color = HexColorTemp(GetColor(a));
            TempStr modified = IsoTimeTemp(ModificationDate(a));
            if (cx.json) {
                out.Append(fmt("%s{%s,\"type\":%s,\"rect\":[%.1f,%.1f,%.1f,%.1f]", n > 0 ? StrL(",") : StrL(""),
                               JPage(engine, pageNo), JStr(AnnotationTypeName(tp)), r.x, r.y, r.dx, r.dy));
                if (len(marked) > 0) {
                    out.Append(fmt(",\"text\":%s", JStr(marked)));
                }
                if (len(contents) > 0) {
                    out.Append(fmt(",\"contents\":%s", JStr(contents)));
                }
                if (len(author) > 0) {
                    out.Append(fmt(",\"author\":%s", JStr(author)));
                }
                if (len(color) > 0) {
                    out.Append(fmt(",\"color\":%s", JStr(color)));
                }
                if (len(modified) > 0) {
                    out.Append(fmt(",\"modified\":%s", JStr(modified)));
                }
                out.Append(StrL("}"));
            } else {
                out.Append(fmt("page %d: %s", pageNo, AnnotationTypeName(tp)));
                if (len(marked) > 0) {
                    out.Append(fmt(" \"%s\"", marked));
                }
                if (len(contents) > 0) {
                    out.Append(fmt(": %s", str::ReplaceTemp(contents, StrL("\n"), StrL(" "))));
                }
                if (len(author) > 0) {
                    out.Append(fmt(" (%s)", author));
                }
                out.Append(StrL("\n"));
            }
            n++;
        }
        if (haveText) {
            FreePageText(&pt);
        }
    }
    if (cx.json) {
        Out(fmt("{\"count\":%d,\"annotations\":[%s]}\n", n, ToStr(out)));
    } else {
        Out(ToStr(out));
    }
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- render

// -o or --output
static Str OutputArg(CliCtx& cx) {
    Str dest = OptValue(cx, StrL("-o"));
    return len(dest) > 0 ? dest : OptValue(cx, StrL("--output"));
}

static int CmdRender(CliCtx& cx) {
    Str dest = OutputArg(cx);
    if (len(dest) == 0) {
        return Fail(cx, kCliUsage, StrL("needs -o <file.png> (.png, .jpg, .webp ...)"));
    }
    Str dpiStr = OptValue(cx, StrL("--dpi"));
    int dpi = len(dpiStr) > 0 ? ParseInt(dpiStr) : 150;
    if (dpi < 10 || dpi > 2400) {
        return Fail(cx, kCliUsage, fmt("--dpi %s: use 10 to 2400", dpiStr));
    }
    TempStr withExt = WithDefaultImageExtTemp(dest);
    if (len(withExt) == 0) {
        return Fail(cx, kCliUsage, fmt("can't write images as '%s'", path::GetExtTemp(dest)));
    }
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    if (len(OptValue(cx, StrL("--pages"))) == 0) {
        // one image unless asked for more: the first page
        VecAppend(pages, 1);
    } else if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    // several pages: <N> in the name is the page number, else -<N> is added
    TempStr templ = EnsurePagePlaceholderTemp(withExt, len(pages) > 1);
    float zoom = SaveSelectionZoom(engine, (float)dpi);
    str::Builder out;
    int nOk = 0;
    for (int pageNo : pages) {
        TempStr path = str::ReplaceTemp(templ, StrL("<N>"), fmt("%d", pageNo));
        RenderPageArgs args(pageNo, zoom, 0, nullptr, RenderTarget::Export);
        Pixmap* px = engine->RenderPage(args);
        bool ok = false;
        int w = 0, h = 0;
        if (px) {
            px->xres = (float)dpi;
            px->yres = (float)dpi;
            w = px->width;
            h = px->height;
            dir::CreateForFile(path);
            ok = SavePixmapAsImageFile(px, path);
            FreePixmap(px);
        }
        if (!ok) {
            SafeEngineRelease(&engine);
            return Fail(cx, kCliFailed, fmt("couldn't render page %d to '%s'", pageNo, path));
        }
        if (cx.json) {
            out.Append(fmt("%s{\"page\":%d,\"file\":%s,\"width\":%d,\"height\":%d}", nOk > 0 ? StrL(",") : StrL(""),
                           pageNo, JStr(path), w, h));
        } else {
            out.Append(fmt("%s\n", path));
        }
        nOk++;
    }
    if (cx.json) {
        Out(fmt("{\"dpi\":%d,\"images\":[%s]}\n", dpi, ToStr(out)));
    } else {
        Out(ToStr(out));
    }
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- PDF tools: pages, merge, compress, decompress, encrypt, decrypt

// the same file, however the two paths spell it
static bool SamePath(Str a, Str b) {
    TempStr fa = path::NormalizeTemp(a);
    TempStr fb = path::NormalizeTemp(b);
#if OS_WIN || OS_DARWIN
    return str::EqI(fa, fb);
#else
    return str::Eq(fa, fb);
#endif
}

// -o, which a PDF tool must have and which mustn't be one of its inputs
static Str PdfOutputArg(CliCtx& cx, const StrVec& inputs, int* code) {
    Str dest = OutputArg(cx);
    if (len(dest) == 0) {
        *code = Fail(cx, kCliUsage, StrL("needs -o <file.pdf>: the input is never changed"));
        return {};
    }
    for (Str in : inputs) {
        if (SamePath(in, dest)) {
            *code = Fail(cx, kCliUsage, fmt("won't write over the input '%s': give another -o", in));
            return {};
        }
    }
    return dest;
}

// opens a PDF to check it: exists, is a PDF, the password is right. Returns
// its page count, 0 after reporting the problem
static int CheckPdf(CliCtx& cx, Str path, int* code) {
    EngineBase* engine = OpenDoc(cx, path, code);
    if (!engine) {
        return 0;
    }
    int n = engine->PageCount();
    bool isPdf = EngineMupdfIsPdf(engine);
    SafeEngineRelease(&engine);
    if (!isPdf) {
        *code = Fail(cx, kCliNotSupported, fmt("'%s' isn't a PDF", path));
        return 0;
    }
    return n;
}

// what a PDF tool reports on success: the file it wrote, its pages and size
static int ReportPdfWritten(CliCtx& cx, Str dest, int nPages, Str extra = {}) {
    i64 size = file::GetSize(dest);
    if (size < 0) {
        return Fail(cx, kCliFailed, fmt("'%s' wasn't written", dest));
    }
    if (cx.json) {
        Out(fmt("{\"file\":%s,\"pages\":%d,\"bytes\":%lld%s}\n", JStr(dest), nPages, (long long)size, extra));
    } else {
        Out(fmt("%s: %d %s, %lld bytes\n", dest, nPages, nPages == 1 ? StrL("page") : StrL("pages"), (long long)size));
    }
    return kCliOk;
}

// "3,1,2-4": the pages in the order given, repeats allowed (pages order)
static bool ParsePageOrder(Str s, int pageCount, Vec<int>& pages) {
    StrVec parts;
    Split(&parts, s, StrL(","), true);
    for (Str part : parts) {
        Vec<int> one;
        if (!ParseDeletePages(part, pageCount, one)) {
            return false;
        }
        for (int p : one) {
            VecAppend(pages, p);
        }
    }
    return len(pages) > 0;
}

static int CmdPages(CliCtx& cx) {
    if (len(cx.positional) < 3) {
        return Fail(cx, kCliUsage,
                    StrL("usage: pages keep|delete|order|rotate <file.pdf> <range> [degrees] -o <out.pdf>"));
    }
    Str action = cx.positional[0];
    Str path = cx.positional[1];
    Str range = cx.positional[2];
    bool keep = str::Eq(action, StrL("keep"));
    bool del = str::Eq(action, StrL("delete"));
    bool order = str::Eq(action, StrL("order"));
    bool rotate = str::Eq(action, StrL("rotate"));
    if (!keep && !del && !order && !rotate) {
        return Fail(cx, kCliUsage, fmt("'%s': use keep, delete, order or rotate", action));
    }
    int degrees = 0;
    if (rotate) {
        degrees = len(cx.positional) > 3 ? ParseInt(cx.positional[3]) : 0;
        degrees = ((degrees % 360) + 360) % 360;
        if (degrees == 0 || degrees % 90 != 0) {
            return Fail(cx, kCliUsage, StrL("rotate needs the degrees: 90, 180 or 270 (clockwise; -90 is 270)"));
        }
    }
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    int n = CheckPdf(cx, path, &code);
    if (n == 0) {
        return code;
    }
    Vec<int> picked;
    bool ok = order ? ParsePageOrder(range, n, picked) : ParseDeletePages(range, n, picked);
    if (!ok) {
        return Fail(cx, kCliUsage, fmt("'%s' isn't a page range of 1-%d, e.g. 1-3,7,10-N", range, n));
    }
    if (del && len(picked) >= n) {
        return Fail(cx, kCliUsage, StrL("can't delete every page"));
    }
    Vec<PdfMergeSource> srcs;
    VecAppend(srcs, PdfMergeSource{path, cx.password});
    Vec<PdfMergePage> pages;
    if (keep || order) {
        for (int p : picked) {
            VecAppend(pages, PdfMergePage{0, p, 0});
        }
    } else {
        for (int p = 1; p <= n; p++) {
            bool in = false;
            for (int q : picked) {
                in = in || q == p;
            }
            if (del && in) {
                continue;
            }
            VecAppend(pages, PdfMergePage{0, p, rotate && in ? degrees : 0});
        }
    }
    dir::CreateForFile(dest);
    if (!EngineMupdfMergePdfs(srcs, pages, dest)) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, len(pages));
}

static int CmdMerge(CliCtx& cx) {
    if (len(cx.positional) < 2) {
        return Fail(cx, kCliUsage, StrL("usage: merge <a.pdf> <b.pdf> ... -o <out.pdf>"));
    }
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, cx.positional, &code);
    if (len(dest) == 0) {
        return code;
    }
    Vec<PdfMergeSource> srcs;
    Vec<PdfMergePage> pages;
    for (int i = 0; i < len(cx.positional); i++) {
        int n = CheckPdf(cx, cx.positional[i], &code);
        if (n == 0) {
            return code;
        }
        VecAppend(srcs, PdfMergeSource{cx.positional[i], cx.password});
        for (int p = 1; p <= n; p++) {
            VecAppend(pages, PdfMergePage{i, p, 0});
        }
    }
    dir::CreateForFile(dest);
    if (!EngineMupdfMergePdfs(srcs, pages, dest)) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, len(pages));
}

// mupdf's clean, as the GUI's PDF tools run it. extra: the flags for the tool
static int RunClean(CliCtx& cx, const char* const* extra, int nExtra, Str newPassword = {}) {
    Str path = cx.positional[0];
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    int n = CheckPdf(cx, path, &code);
    if (n == 0) {
        return code;
    }
    i64 before = file::GetSize(path);
    Vec<char*> argv;
    VecAppend(argv, (char*)"clean");
    if (len(cx.password) > 0) {
        VecAppend(argv, (char*)"-p");
        VecAppend(argv, CStrTemp(cx.password));
    }
    for (int i = 0; i < nExtra; i++) {
        Str a(extra[i]);
        // <pwd>: the new password
        VecAppend(argv, str::Eq(a, StrL("<pwd>")) ? CStrTemp(newPassword) : (char*)extra[i]);
    }
    VecAppend(argv, CStrTemp(path));
    VecAppend(argv, CStrTemp(dest));
    dir::CreateForFile(dest);
    fz_set_optind(0);
    if (pdfclean_main(len(argv), argv.els) != 0) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, n, fmt(",\"inputBytes\":%lld", (long long)before));
}

static int CmdCompress(CliCtx& cx) {
    static const char* args[] = {"-gggg", "-e", "100", "-f", "-i", "-t", "-Z"};
    return RunClean(cx, args, dimofi(args));
}

static int CmdDecompress(CliCtx& cx) {
    static const char* args[] = {"-d"};
    return RunClean(cx, args, dimofi(args));
}

static int CmdEncrypt(CliCtx& cx) {
    Str pwd = OptValue(cx, StrL("--new-password"));
    Str pwdEnv = OptValue(cx, StrL("--new-password-env"));
    if (len(pwdEnv) > 0) {
        const char* v = getenv(CStrTemp(pwdEnv));
        if (!v) {
            return Fail(cx, kCliUsage, fmt("--new-password-env: %s isn't set", pwdEnv));
        }
        pwd = Str(v);
    }
    if (len(pwd) == 0) {
        return Fail(cx, kCliUsage, StrL("needs --new-password <pw> or --new-password-env <NAME>"));
    }
    static const char* args[] = {"-E", "aes-256", "-U", "<pwd>", "-O", "<pwd>"};
    return RunClean(cx, args, dimofi(args), pwd);
}

static int CmdDecrypt(CliCtx& cx) {
    static const char* args[] = {"-D"};
    return RunClean(cx, args, dimofi(args));
}

//--- markdown

// for matching a block's text against the table of contents and the running
// headers: lower case, digits as '#', runs of spaces as one
static TempStr NormalizeForMatchTemp(Str s) {
    str::Builder out;
    bool space = false;
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = len(out) > 0;
            continue;
        }
        if (space) {
            out.AppendChar(' ');
            space = false;
        }
        if (c >= '0' && c <= '9') {
            c = '#';
        } else if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        out.AppendChar(c);
    }
    return ToStrTemp(out);
}

static void CollectTocTitles(TocItem* item, int depth, StrVec& titles, Vec<int>& depths) {
    for (TocItem* it = item; it; it = it->next) {
        if (len(it->title) > 0) {
            titles.Append(NormalizeForMatchTemp(it->title));
            VecAppend(depths, depth);
        }
        CollectTocTitles(it->child, depth + 1, titles, depths);
    }
}

static bool IsBulletStart(Str s, int* skip) {
    static const char* bullets[] = {"\xe2\x80\xa2 ", "\xe2\x97\xa6 ", "\xe2\x96\xaa ", "\xe2\x80\x93 ", "- ", "* ",
                                    "\xc2\xb7 "};
    for (const char* b : bullets) {
        if (str::StartsWith(s, Str(b))) {
            *skip = (int)strlen(b);
            return true;
        }
    }
    return false;
}

// "12. " or "3) ": the length of the number and its mark
static int NumberedStart(Str s) {
    int i = 0;
    while (i < len(s) && i < 4 && s.s[i] >= '0' && s.s[i] <= '9') {
        i++;
    }
    if (i == 0 || i + 1 >= len(s) || (s.s[i] != '.' && s.s[i] != ')') || s.s[i + 1] != ' ') {
        return 0;
    }
    return i;
}

// what markdown would take as markup at the start of a paragraph
static TempStr EscapeLineStartTemp(Str s) {
    if (len(s) > 0 && (s.s[0] == '#' || s.s[0] == '>' || s.s[0] == '|' || s.s[0] == '=')) {
        return fmt("\\%s", s);
    }
    return str::DupTemp(s);
}

struct MdDoc {
    float bodySize = 0;
    // font sizes that are headings, largest first: index + 1 is the level
    Vec<float> headingSizes;
    StrVec tocTitles;
    Vec<int> tocDepths;
    // the normalized text of blocks that open or close most pages
    StrVec running;
};

static int HeadingLevelForSize(MdDoc& d, float size) {
    for (int i = 0; i < len(d.headingSizes); i++) {
        if (fabsf(d.headingSizes[i] - size) < 0.5f) {
            return i + 1;
        }
    }
    return 0;
}

// the spans of one block as markdown: lines joined into a paragraph, styles as
// ** and *, monospaced runs as `code`
static TempStr BlockInlineTemp(const StyledPage& p, int from, int to, bool keepLines) {
    str::Builder out;
    int line = -1;
    bool joined = false;
    for (int i = from; i < to; i++) {
        const StyledSpan& s = p.spans[i];
        if (s.line != line) {
            if (line >= 0) {
                if (keepLines) {
                    out.AppendChar('\n');
                } else if (joined) {
                    // "hyph-" + "enated": the hyphen goes
                    Str t = ToStr(out);
                    if (len(t) > 0 && t.s[len(t) - 1] == '-') {
                        out.RemoveLast();
                    }
                } else {
                    out.AppendChar(' ');
                }
            }
            line = s.line;
            joined = s.lineJoined;
        }
        Str text = s.text;
        if (keepLines) {
            out.Append(text);
            continue;
        }
        // markers around the words, not around the spaces next to them.
        // (not TrimWSInPlace: it would cut the span's own text)
        int a = 0;
        int b = len(text);
        while (a < b && text.s[a] == ' ') {
            a++;
        }
        while (b > a && text.s[b - 1] == ' ') {
            b--;
        }
        if (a == b) {
            out.Append(text);
            continue;
        }
        Str lead(text.s, a);
        Str trimmed(text.s + a, b - a);
        Str trail(text.s + b, len(text) - b);
        const char* mark = s.mono ? "`" : (s.bold && s.italic) ? "***" : s.bold ? "**" : s.italic ? "*" : "";
        out.Append(lead);
        out.Append(Str(mark));
        out.Append(trimmed);
        out.Append(Str(mark));
        out.Append(trail);
    }
    TempStr res = ToStrTemp(out);
    // two styled runs next to each other: "**a****b**" is "**ab**"
    res = str::ReplaceTemp(res, StrL("****"), StrL(""));
    res = str::ReplaceTemp(res, StrL("``"), StrL(""));
    while (!keepLines && str::Contains(res, StrL("  "))) {
        res = str::ReplaceTemp(res, StrL("  "), StrL(" "));
    }
    return res;
}

// the plain text of spans [from, to), for matching
static TempStr BlockPlainTemp(const StyledPage& p, int from, int to) {
    str::Builder out;
    for (int i = from; i < to; i++) {
        if (i > from && p.spans[i].line != p.spans[i - 1].line) {
            out.AppendChar(' ');
        }
        out.Append(p.spans[i].text);
    }
    return ToStrTemp(out);
}

struct MdImages {
    // where images go, relative to the markdown file; empty: no images
    Str dir;
    Str dirRel;
    int n = 0;
    StrVec written;
};

static void AppendImage(MdImages& imgs, const StyledImage& im, int pageNo, str::Builder& out) {
    if (len(imgs.dir) == 0) {
        return;
    }
    imgs.n++;
    TempStr name = fmt("page-%d-%d.png", pageNo, imgs.n);
    TempStr path = path::JoinTemp(imgs.dir, name);
    dir::CreateForFile(path);
    if (!file::WriteFile(path, im.png)) {
        return;
    }
    imgs.written.Append(path);
    out.Append(fmt("![](%s/%s)\n\n", imgs.dirRel, name));
}

// one page of a mupdf document as markdown
static void StyledPageToMarkdown(MdDoc& d, StyledPage& p, int pageNo, MdImages& imgs, str::Builder& out) {
    int nSpans = len(p.spans);
    int img = 0;
    int i = 0;
    while (i < nSpans || img < len(p.images)) {
        int block = i < nSpans ? p.spans[i].block : INT_MAX;
        // pictures come where they are in the reading order
        if (img < len(p.images) && p.images[img].block < block) {
            AppendImage(imgs, p.images[img], pageNo, out);
            img++;
            continue;
        }
        int end = i;
        float maxSize = 0;
        int nChars = 0, nMono = 0, nBold = 0;
        while (end < nSpans && p.spans[end].block == block) {
            const StyledSpan& s = p.spans[end];
            maxSize = std::max(maxSize, s.size);
            nChars += len(s.text);
            nMono += s.mono ? len(s.text) : 0;
            nBold += s.bold ? len(s.text) : 0;
            end++;
        }
        TempStr plain = BlockPlainTemp(p, i, end);
        TempStr norm = NormalizeForMatchTemp(plain);
        bool isRunning = false;
        for (Str r : d.running) {
            isRunning = isRunning || str::Eq(r, norm);
        }
        if (isRunning || len(norm) == 0) {
            i = end;
            continue;
        }
        int nLines = p.spans[end - 1].line - p.spans[i].line + 1;
        // a heading: a title from the table of contents, or a short block in a
        // heading's size
        int level = 0;
        for (int t = 0; t < len(d.tocTitles); t++) {
            if (str::Eq(d.tocTitles[t], norm)) {
                level = std::min(d.tocDepths[t] + 1, 6);
                break;
            }
        }
        if (level == 0 && nLines <= 3 && len(plain) < 200) {
            level = HeadingLevelForSize(d, maxSize);
        }
        // a line of bold on its own in the body's size: the smallest heading
        // (a man page's NAME, a report's section titles)
        if (level == 0 && nLines == 1 && nBold == nChars && len(plain) < 80) {
            level = std::min(len(d.headingSizes) + 1, 4);
        }
        if (level > 0) {
            Str text = plain;
            str::TrimWSInPlace(text, str::TrimOpt::Both);
            for (int k = 0; k < level; k++) {
                out.AppendChar('#');
            }
            out.Append(fmt(" %s\n\n", text));
        } else if (nMono * 10 >= nChars * 9) {
            out.Append(fmt("```\n%s\n```\n\n", BlockInlineTemp(p, i, end, true)));
        } else {
            // a list: each line that starts with a bullet or a number is an item
            int skip = 0;
            TempStr first = str::DupTemp(p.spans[i].text);
            str::TrimWSInPlace(first, str::TrimOpt::Left);
            bool bullet = IsBulletStart(first, &skip);
            int num = bullet ? 0 : NumberedStart(first);
            TempStr text = BlockInlineTemp(p, i, end, false);
            str::TrimWSInPlace(text, str::TrimOpt::Both);
            if (bullet) {
                Str rest = text;
                IsBulletStart(rest, &skip);
                out.Append(fmt("- %s\n\n", Str(rest.s + skip, len(rest) - skip)));
            } else if (num > 0) {
                out.Append(fmt("%s\n\n", text));
            } else {
                out.Append(fmt("%s\n\n", EscapeLineStartTemp(text)));
            }
        }
        i = end;
    }
}

// a page of a document without styles (DjVu, comics, ...): its text, a
// paragraph per run of lines
static void PlainPageToMarkdown(EngineBase* engine, int pageNo, str::Builder& out) {
    PageText pt = engine->ExtractPageText(pageNo);
    StrVec lines;
    Split(&lines, pt.text, StrL("\n"), false);
    str::Builder para;
    auto flush = [&]() {
        if (len(para) > 0) {
            out.Append(fmt("%s\n\n", EscapeLineStartTemp(ToStr(para))));
            para.Reset();
        }
    };
    for (Str l : lines) {
        str::TrimWSInPlace(l, str::TrimOpt::Both);
        if (len(l) == 0) {
            flush();
            continue;
        }
        if (len(para) > 0) {
            para.AppendChar(' ');
        }
        para.Append(l);
    }
    flush();
    FreePageText(&pt);
}

// the font size most text is in, and the larger ones that are rare enough to
// be headings (at most 3 levels)
static void FindSizes(MdDoc& d, Vec<StyledPage*>& pages) {
    struct Count {
        float size;
        int n;
    };
    Vec<Count> counts;
    for (StyledPage* p : pages) {
        for (StyledSpan& s : p->spans) {
            float sz = roundf(s.size * 2) / 2;
            bool found = false;
            for (Count& c : counts) {
                if (c.size == sz) {
                    c.n += len(s.text);
                    found = true;
                }
            }
            if (!found) {
                VecAppend(counts, Count{sz, len(s.text)});
            }
        }
    }
    int best = 0;
    for (Count& c : counts) {
        if (c.n > best) {
            best = c.n;
            d.bodySize = c.size;
        }
    }
    Vec<float> bigger;
    for (Count& c : counts) {
        // a heading is clearly larger and much rarer than the body text
        if (c.size >= d.bodySize * 1.15f && c.n * 4 < best) {
            VecAppend(bigger, c.size);
        }
    }
    std::sort(bigger.begin(), bigger.end(), [](float a, float b) { return a > b; });
    for (int i = 0; i < len(bigger) && i < 3; i++) {
        VecAppend(d.headingSizes, bigger[i]);
    }
}

// blocks that open or close most pages with the same text (digits aside):
// running headers, footers, page numbers
static void FindRunning(MdDoc& d, Vec<StyledPage*>& pages) {
    if (len(pages) < 3) {
        return;
    }
    StrVec texts;
    Vec<int> counts;
    for (StyledPage* p : pages) {
        int n = len(p->spans);
        if (n == 0) {
            continue;
        }
        int blocks[2] = {p->spans[0].block, p->spans[n - 1].block};
        for (int k = 0; k < 2; k++) {
            if (k == 1 && blocks[1] == blocks[0]) {
                continue;
            }
            int from = 0;
            while (from < n && p->spans[from].block != blocks[k]) {
                from++;
            }
            int to = from;
            while (to < n && p->spans[to].block == blocks[k]) {
                to++;
            }
            TempStr norm = NormalizeForMatchTemp(BlockPlainTemp(*p, from, to));
            if (len(norm) == 0 || len(norm) > 120) {
                continue;
            }
            int idx = -1;
            for (int t = 0; t < len(texts); t++) {
                if (str::Eq(texts[t], norm)) {
                    idx = t;
                }
            }
            if (idx < 0) {
                texts.Append(norm);
                VecAppend(counts, 1);
            } else {
                counts[idx]++;
            }
        }
    }
    for (int t = 0; t < len(texts); t++) {
        if (counts[t] * 2 > len(pages)) {
            d.running.Append(texts[t]);
        }
    }
}

static int CmdMarkdown(CliCtx& cx) {
    Str dest = OutputArg(cx);
    Str src = cx.positional[0];
    if (len(dest) > 0 && SamePath(src, dest)) {
        return Fail(cx, kCliUsage, StrL("won't write over the input: give another -o"));
    }
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, src, &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    bool pageMarkers = !HasOpt(cx, StrL("--no-page-markers"));
    MdImages imgs;
    // pictures go next to the markdown file, in <name>_images
    if (len(dest) > 0 && !HasOpt(cx, StrL("--no-images"))) {
        TempStr base = path::GetPathNoExtTemp(path::GetBaseNameTemp(dest));
        imgs.dirRel = fmt("%s_images", base);
        imgs.dir = path::JoinTemp(path::GetDirTemp(dest), imgs.dirRel);
    }
    MdDoc d;
    Vec<StyledPage*> styled;
    bool haveStyles = true;
    for (int pageNo : pages) {
        auto* p = new StyledPage();
        if (!EngineMupdfGetStyledPage(engine, pageNo, *p)) {
            haveStyles = false;
        }
        VecAppend(styled, p);
    }
    if (haveStyles) {
        FindSizes(d, styled);
        FindRunning(d, styled);
        TocTree* toc = engine->HasToc() ? engine->GetToc() : nullptr;
        if (toc && toc->root) {
            CollectTocTitles(toc->root->child ? toc->root->child : toc->root, 0, d.tocTitles, d.tocDepths);
        }
    }
    str::Builder out;
    TempStr title = engine->GetPropertyTemp(DocProp::Title);
    for (int k = 0; k < len(pages); k++) {
        if (pageMarkers) {
            out.Append(fmt("<!-- page %d -->\n\n", pages[k]));
        }
        if (haveStyles) {
            StyledPageToMarkdown(d, *styled[k], pages[k], imgs, out);
        } else {
            PlainPageToMarkdown(engine, pages[k], out);
        }
    }
    for (StyledPage* p : styled) {
        delete p;
    }
    Str md = ToStr(out);
    if (len(dest) > 0) {
        dir::CreateForFile(dest);
        if (!file::WriteFile(dest, md)) {
            SafeEngineRelease(&engine);
            return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
        }
    }
    if (cx.json) {
        str::Builder j;
        j.Append(fmt("{\"pages\":%d", len(pages)));
        if (len(title) > 0) {
            j.Append(fmt(",\"title\":%s", JStr(title)));
        }
        if (len(dest) > 0) {
            j.Append(fmt(",\"file\":%s,\"images\":[", JStr(dest)));
            for (int k = 0; k < len(imgs.written); k++) {
                j.Append(fmt("%s%s", k > 0 ? StrL(",") : StrL(""), JStr(imgs.written[k])));
            }
            j.Append(StrL("]}\n"));
        } else {
            j.Append(fmt(",\"markdown\":%s}\n", JStr(md)));
        }
        Out(ToStr(j));
    } else if (len(dest) > 0) {
        Out(fmt("%s: %d pages, %d images\n", dest, len(pages), len(imgs.written)));
    } else {
        Out(md);
    }
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
    {"annotations", "<file>", "a PDF's annotations: highlights with the text they mark, notes, shapes",
     "--pages <range>  only these pages", "annotations paper.pdf\nannotations paper.pdf --json", CmdAnnotations},
    {"render", "<file> -o <image>", "pages as images (PNG, JPEG, WebP, ... by the file's extension)",
     "-o <file>        the image; for several pages <N> in it is the page number\n--pages <range>  these pages "
     "(default: the first)\n--dpi <n>        resolution, default 150",
     "render paper.pdf -o page.png\nrender comic.cbz --pages 1-3 -o pages/<N>.jpg --dpi 96", CmdRender},
    {"markdown", "<file> [-o <file.md>]",
     "the document as Markdown, for reading or for an LLM: headings, lists, emphasis, code and pictures",
     "-o <file>          write it there, pictures in <name>_images next to it (else to stdout, without pictures)\n"
     "--pages <range>    only these pages\n--no-page-markers  without the <!-- page N --> before each page\n"
     "--no-images        without the pictures",
     "markdown report.pdf -o report.md\nmarkdown book.epub --pages 1-20 | head\nmarkdown thesis.pdf --json",
     CmdMarkdown},
    {"pages", "keep|delete|order|rotate <file.pdf> <range> [degrees] -o <out.pdf>",
     "a PDF with some of its pages: kept, deleted, put in another order or turned", "-o <file>        the new PDF",
     "pages keep scan.pdf 1-3,7 -o short.pdf\npages delete scan.pdf 2 -o out.pdf\n"
     "pages order scan.pdf 3,1,2 -o out.pdf\npages rotate scan.pdf 2-N 90 -o out.pdf",
     CmdPages},
    {"merge", "<a.pdf> <b.pdf> ... -o <out.pdf>", "PDFs one after another in a new PDF", "-o <file>        the new PDF",
     "merge a.pdf b.pdf -o both.pdf", CmdMerge},
    {"compress", "<file.pdf> -o <out.pdf>", "a smaller copy of a PDF (same quality: unused objects and streams go)",
     "-o <file>        the new PDF", "compress big.pdf -o small.pdf --json", CmdCompress},
    {"decompress", "<file.pdf> -o <out.pdf>", "a copy of a PDF with its streams uncompressed, for reading its insides",
     "-o <file>        the new PDF", "decompress a.pdf -o readable.pdf", CmdDecompress},
    {"encrypt", "<file.pdf> -o <out.pdf> --new-password <pw>", "a copy of a PDF that needs a password (AES-256)",
     "-o <file>        the new PDF\n--new-password <pw>  or --new-password-env <NAME>",
     "encrypt a.pdf -o locked.pdf --new-password-env PDF_PW", CmdEncrypt},
    {"decrypt", "<file.pdf> -o <out.pdf> --password <pw>", "a copy of an encrypted PDF without the password",
     "-o <file>        the new PDF", "decrypt locked.pdf -o open.pdf --password-env PDF_PW", CmdDecrypt},
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
        out.Append(fmt("  %-12s %s\n", Str(c.name), Str(c.summary)));
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
