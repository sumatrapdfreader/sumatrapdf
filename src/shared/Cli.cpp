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
#include "PdfCreator.h"
#include "ImageReader.h"
#include "SumatraConfig.h"
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
    static const char* names[] = {"--pages",  "--password", "--password-env", "--new-password", "--new-password-env",
                                  "-o",       "--output",   "--dpi",          "--install",      "--opacity",
                                  "--color",  "--size",     "--format",       "--position",     "--start",
                                  "--margin", "--every",    "--ranges",       "--text",         "--contents",
                                  "--at",     "--rect",     "--key",          "--key-password", "--key-password-env",
                                  "--field",  "--reason",   "--location"};
    for (const char* n : names) {
        if (str::Eq(name, Str(n))) {
            return true;
        }
    }
    return false;
}

// -o or --output
static Str OutputArg(CliCtx& cx) {
    Str dest = OptValue(cx, StrL("-o"));
    return len(dest) > 0 ? dest : OptValue(cx, StrL("--output"));
}

// a number option: def without it; false after reporting a bad one
static bool FloatOpt(CliCtx& cx, Str name, float def, float lo, float hi, float* out, int* code) {
    Str v = OptValue(cx, name);
    *out = def;
    if (len(v) == 0) {
        return true;
    }
    char* end = nullptr;
    double d = strtod(CStrTemp(v), &end);
    if (!end || *end != 0 || d < lo || d > hi) {
        *code = Fail(cx, kCliUsage, fmt("%s %s: use a number from %g to %g", name, v, (double)lo, (double)hi));
        return false;
    }
    *out = (float)d;
    return true;
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

//--- images

// the image format of encoded bytes, as a file extension; empty if unknown
static Str ImageExt(Str d) {
    const u8* b = (const u8*)d.s;
    int n = len(d);
    if (n > 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff) {
        return StrL(".jpg");
    }
    if (n > 8 && memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0) {
        return StrL(".png");
    }
    if (n > 6 && (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0)) {
        return StrL(".gif");
    }
    if (n > 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0) {
        return StrL(".webp");
    }
    if (n > 12 && (memcmp(b, "\x00\x00\x00\x0cjP  ", 8) == 0 || memcmp(b, "\xff\x4f\xff\x51", 4) == 0)) {
        return StrL(".jp2");
    }
    if (n > 4 && (memcmp(b, "II*\x00", 4) == 0 || memcmp(b, "MM\x00*", 4) == 0)) {
        return StrL(".tif");
    }
    if (n > 2 && b[0] == 'B' && b[1] == 'M') {
        return StrL(".bmp");
    }
    return {};
}

// the pictures in a document's pages, as files in the folder -o: the bytes
// the document has where they are a format of their own (a PDF's JPEG, a
// comic's pages), else as PNG
static int CmdImages(CliCtx& cx) {
    Str dir = OutputArg(cx);
    if (len(dir) == 0) {
        return Fail(cx, kCliUsage, StrL("needs -o <folder> for the images"));
    }
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
    TempStr base = path::GetPathNoExtTemp(path::GetBaseNameTemp(cx.positional[0]));
    str::Builder out;
    int nImages = 0;
    for (int pageNo : pages) {
        // the page's elements are only there once it's fully loaded
        engine->BenchLoadPage(pageNo);
        Vec<IPageElement*> els = engine->GetElements(pageNo);
        int k = 0;
        for (IPageElement* el : els) {
            if (el->GetKind() != kindPageElementImage) {
                continue;
            }
            Str data = engine->GetImageDataForPageElement(el);
            Str ext = ImageExt(data);
            if (len(ext) == 0) {
                str::Free(data);
                data = engine->GetImagePngForPageElement(el);
                ext = StrL(".png");
            }
            if (len(data) == 0) {
                continue;
            }
            k++;
            TempStr dest = path::JoinTemp(dir, fmt("%s-p%d-%d%s", base, pageNo, k, ext));
            dir::CreateForFile(dest);
            bool ok = file::WriteFile(dest, data);
            int size = len(data);
            str::Free(data);
            if (!ok) {
                SafeEngineRelease(&engine);
                return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
            }
            RectF r = el->GetRect();
            if (cx.json) {
                out.Append(fmt("%s{\"page\":%d,\"file\":%s,\"bytes\":%d,\"rect\":[%.1f,%.1f,%.1f,%.1f]}",
                               nImages > 0 ? StrL(",") : StrL(""), pageNo, JStr(dest), size, r.x, r.y, r.dx, r.dy));
            } else {
                out.Append(fmt("%s\n", dest));
            }
            nImages++;
        }
    }
    Out(cx.json ? fmt("{\"count\":%d,\"images\":[%s]}\n", nImages, ToStr(out)) : ToStrTemp(out));
    SafeEngineRelease(&engine);
    return kCliOk;
}

//--- render

static int CmdRender(CliCtx& cx) {
    Str dest = OutputArg(cx);
    if (len(dest) == 0) {
        return Fail(cx, kCliUsage, StrL("needs -o <file.png> (.png, .jpg, .bmp or .gif)"));
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

// a PDF per group of pages: one each (default), --every N, or --ranges
// "1-3;4-9;10-N"; into the folder -o, as <name>-<first>[-<last>].pdf
static int CmdSplit(CliCtx& cx) {
    Str path = cx.positional[0];
    Str dir = OutputArg(cx);
    if (len(dir) == 0) {
        return Fail(cx, kCliUsage, StrL("needs -o <folder> for the PDFs"));
    }
    float every = 0;
    int code = kCliOk;
    if (!FloatOpt(cx, StrL("--every"), 1, 1, 100000, &every, &code)) {
        return code;
    }
    int n = CheckPdf(cx, path, &code);
    if (n == 0) {
        return code;
    }
    // the groups, as page lists
    Vec<int> groupStarts;
    Vec<int> pages;
    Str ranges = OptValue(cx, StrL("--ranges"));
    if (len(ranges) > 0) {
        StrVec parts;
        Split(&parts, ranges, StrL(";"), true);
        for (Str part : parts) {
            Vec<int> group;
            if (!ParseDeletePages(part, n, group)) {
                return Fail(cx, kCliUsage, fmt("--ranges: '%s' isn't a page range of 1-%d", part, n));
            }
            VecAppend(groupStarts, len(pages));
            for (int p : group) {
                VecAppend(pages, p);
            }
        }
    } else {
        for (int p = 1; p <= n; p++) {
            if ((p - 1) % (int)every == 0) {
                VecAppend(groupStarts, len(pages));
            }
            VecAppend(pages, p);
        }
    }
    VecAppend(groupStarts, len(pages));
    // page numbers padded to the same width, so the files sort in order
    int width = len(fmt("%d", n));
    TempStr base = path::GetPathNoExtTemp(path::GetBaseNameTemp(path));
    Vec<PdfMergeSource> srcs;
    VecAppend(srcs, PdfMergeSource{path, cx.password});
    str::Builder out;
    for (int g = 0; g + 1 < len(groupStarts); g++) {
        int from = groupStarts[g];
        int to = groupStarts[g + 1];
        int first = pages[from];
        int last = pages[to - 1];
        auto padded = [width](int v) {
            TempStr num = fmt("%d", v);
            while (len(num) < width) {
                num = fmt("0%s", num);
            }
            return num;
        };
        TempStr name = to - from == 1 ? fmt("%s-%s.pdf", base, padded(first))
                                      : fmt("%s-%s-%s.pdf", base, padded(first), padded(last));
        TempStr dest = path::JoinTemp(dir, name);
        if (SamePath(dest, path)) {
            return Fail(cx, kCliUsage, fmt("won't write over the input '%s'", path));
        }
        Vec<PdfMergePage> group;
        for (int k = from; k < to; k++) {
            VecAppend(group, PdfMergePage{0, pages[k], 0});
        }
        dir::CreateForFile(dest);
        if (!EngineMupdfMergePdfs(srcs, group, dest)) {
            return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
        }
        if (cx.json) {
            out.Append(fmt("%s{\"file\":%s,\"first\":%d,\"last\":%d,\"pages\":%d}", g > 0 ? StrL(",") : StrL(""),
                           JStr(dest), first, last, to - from));
        } else {
            out.Append(fmt("%s: %d %s\n", dest, to - from, to - from == 1 ? StrL("page") : StrL("pages")));
        }
    }
    Out(cx.json ? fmt("{\"files\":[%s]}\n", ToStr(out)) : ToStrTemp(out));
    return kCliOk;
}

// a PDF with a page per image, in the order given; mupdf's decoders first
// (JPEG, PNG, GIF, BMP, TIFF, JPEG 2000 keep their bytes), then SumatraPDF's
// (WebP, HEIC, AVIF, ...)
static int CmdImagesToPdf(CliCtx& cx) {
    if (len(cx.positional) < 1) {
        return Fail(cx, kCliUsage, StrL("usage: images-to-pdf <image> ... -o <out.pdf>"));
    }
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, cx.positional, &code);
    if (len(dest) == 0) {
        return code;
    }
    TempStr producer = fmt("SumatraPDF %s", currentVersion);
    PdfCreator::SetProducerName(producer);
    PdfCreator c;
    for (Str path : cx.positional) {
        Str data = file::ReadFile(path);
        if (len(data) == 0) {
            str::Free(data);
            return Fail(cx, kCliCantOpen, fmt("can't read '%s'", path));
        }
        bool ok = c.AddPageFromImageData(data);
        if (!ok && !ImageDecodedPixmapWouldBeHuge(data)) {
            Pixmap* px = PixmapFromData(data);
            ok = px && c.AddPageFromPixmap(px);
            FreePixmap(px);
        }
        str::Free(data);
        if (!ok) {
            return Fail(cx, kCliCantOpen, fmt("'%s' isn't an image SumatraPDF can read", path));
        }
    }
    dir::CreateForFile(dest);
    if (!c.SaveToFile(dest)) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, len(cx.positional));
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

// a list item's number at the start of a line: "12. ", "3) ", "a) ", "(iv) ",
// "(3) ". Returns the length of the number and its mark, 0 if there's none.
// "2024. was a year" isn't one: a list doesn't go past 3 digits
static int NumberedStart(Str s) {
    int n = len(s);
    int i = 0;
    bool paren = n > 0 && s.s[0] == '(';
    if (paren) {
        i++;
    }
    int start = i;
    while (i < n && i - start < 4 && s.s[i] >= '0' && s.s[i] <= '9') {
        i++;
    }
    if (i == start) {
        // a letter (a, b, ...) or a roman number (iv, xii) in lower case
        while (i < n && i - start < 5 && s.s[i] >= 'a' && s.s[i] <= 'z') {
            i++;
        }
        int nLetters = i - start;
        bool roman = nLetters > 0;
        for (int k = start; k < i; k++) {
            roman = roman && strchr("ivxlc", s.s[k]) != nullptr;
        }
        if (nLetters == 0 || (nLetters > 1 && !roman)) {
            return 0;
        }
    } else if (i - start > 3) {
        return 0;
    }
    if (i >= n) {
        return 0;
    }
    char mark = s.s[i];
    bool ok = paren ? mark == ')' : (mark == '.' || mark == ')');
    if (!ok || i + 1 >= n || s.s[i + 1] != ' ') {
        return 0;
    }
    return i + 1;
}

// what markdown would take as markup at the start of a paragraph
// "- ", "+ ", "#", ">", "|", "=" and "2024. " / "3) " (a list item's number)
static TempStr EscapeLineStartTemp(Str s) {
    int n = len(s);
    if (n == 0) {
        return {};
    }
    char c = s.s[0];
    if (c == '#' || c == '>' || c == '|' || c == '=' || ((c == '-' || c == '+') && n > 1 && s.s[1] == ' ')) {
        return fmt("\\%s", s);
    }
    int i = 0;
    while (i < n && i < 9 && s.s[i] >= '0' && s.s[i] <= '9') {
        i++;
    }
    if (i > 0 && i < n && (s.s[i] == '.' || s.s[i] == ')') && (i + 1 == n || s.s[i + 1] == ' ')) {
        return fmt("%s\\%s", Str(s.s, i), Str(s.s + i, n - i));
    }
    return str::DupTemp(s);
}

// an & that starts an entity: &amp; &#38; &#x26;
static bool StartsEntity(Str s, int i) {
    int n = len(s);
    int k = i + 1;
    if (k < n && s.s[k] == '#') {
        k++;
        bool hex = k < n && (s.s[k] == 'x' || s.s[k] == 'X');
        if (hex) {
            k++;
        }
        int start = k;
        while (k < n && (isdigit((u8)s.s[k]) || (hex && isxdigit((u8)s.s[k])))) {
            k++;
        }
        return k > start && k < n && s.s[k] == ';';
    }
    int start = k;
    while (k < n && isalnum((u8)s.s[k])) {
        k++;
    }
    return k > start && isalpha((u8)s.s[start]) && k < n && s.s[k] == ';';
}

// what markdown takes as markup inside a line: \ * _ [ ] ` < and an & that
// starts an entity. # and > are text there
static TempStr EscapeInlineTemp(Str s) {
    str::Builder out;
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        if (strchr("\\*_[]`<", c) || (c == '&' && StartsEntity(s, i))) {
            out.AppendChar('\\');
        }
        out.AppendChar(c);
    }
    return ToStrTemp(out);
}

// a heading's text: inline escapes, and a # at the end isn't a closing #
static TempStr EscapeHeadingTemp(Str s) {
    TempStr t = EscapeInlineTemp(s);
    if (len(t) > 0 && t.s[len(t) - 1] == '#') {
        return fmt("%s\\#", Str(t.s, len(t) - 1));
    }
    return t;
}

// whether s has a letter or a digit: spaces and punctuation alone aren't marked bold
static bool HasWordChar(Str s) {
    for (int i = 0; i < len(s); i++) {
        u8 c = (u8)s.s[i];
        if (isalnum(c) || c >= 0x80) {
            return true;
        }
    }
    return false;
}

// where each line of spans [from, to) starts, and to at the end
static void LineStarts(const StyledPage& p, int from, int to, Vec<int>& starts) {
    for (int i = from; i < to; i++) {
        if (i == from || p.spans[i].line != p.spans[i - 1].line) {
            VecAppend(starts, i);
        }
    }
    VecAppend(starts, to);
}

// whether the line starting with span i is a list item: a bullet (whose bytes,
// spaces before it included, go in *skip) or a number (which stays)
static bool LineListMarker(const StyledPage& p, int i, bool* bullet, int* skip) {
    Str t = p.spans[i].text;
    int lead = 0;
    while (lead < len(t) && t.s[lead] == ' ') {
        lead++;
    }
    Str rest(t.s + lead, len(t) - lead);
    int n = 0;
    if (IsBulletStart(rest, &n)) {
        *bullet = true;
        *skip = lead + n;
        return true;
    }
    *bullet = false;
    *skip = lead;
    return NumberedStart(rest) > 0;
}

struct MdDoc {
    float bodySize = 0;
    // font sizes that are headings, largest first
    Vec<float> headingSizes;
    // the level of each of headingSizes: 1 to 3
    Vec<int> headingLevels;
    int nHeadingLevels = 0;
    StrVec tocTitles;
    Vec<int> tocDepths;
    // the running headers' HeaderKeyTemp()
    StrVec running;
};

static int HeadingLevelForSize(MdDoc& d, float size) {
    for (int i = 0; i < len(d.headingSizes); i++) {
        if (fabsf(d.headingSizes[i] - size) < 0.5f) {
            return d.headingLevels[i];
        }
    }
    return 0;
}

static bool IsLowerStart(Str s) {
    if (len(s) == 0) {
        return false;
    }
    Codepoints cp;
    DecodeText(Str(s.s, std::min(len(s), 4)), cp);
    return len(cp.cps) > 0 && iswlower((wint_t)cp.cps[0]);
}

// "exam-" + "ple": a word broken across lines, which loses its hyphen. Before
// a capital it's a real hyphen ("well- Known")
static bool IsBrokenWord(Str before, Str after) {
    int n = len(before);
    return n > 1 && before.s[n - 1] == '-' && isalpha((u8)before.s[n - 2]) && IsLowerStart(after);
}

// a footnote mark or an exponent ("dnia" + a raised 1) is set smaller and
// raised, a chemical's subscript (the 2 of H2O) smaller and lowered: 1 for a
// raised span, -1 for a lowered one, 0 for one on the line's baseline. The
// line's main text is its largest
static int SpanShift(const StyledPage& p, int span) {
    const StyledSpan& s = p.spans[span];
    if (s.box.IsEmpty()) {
        return 0;
    }
    float mainSize = 0;
    float mainBottom = 0;
    for (int k = span; k >= 0 && p.spans[k].line == s.line; k--) {
        if (p.spans[k].size > mainSize + 0.5f && !p.spans[k].box.IsEmpty()) {
            mainSize = p.spans[k].size;
            mainBottom = p.spans[k].box.y + p.spans[k].box.dy;
        }
    }
    for (int k = span + 1; k < len(p.spans) && p.spans[k].line == s.line; k++) {
        if (p.spans[k].size > mainSize + 0.5f && !p.spans[k].box.IsEmpty()) {
            mainSize = p.spans[k].size;
            mainBottom = p.spans[k].box.y + p.spans[k].box.dy;
        }
    }
    if (s.size > mainSize * 0.85f) {
        return 0;
    }
    float bottom = s.box.y + s.box.dy;
    if (mainBottom - bottom > mainSize * 0.2f) {
        return 1;
    }
    // smaller text on the baseline ends above the main text's bottom (its
    // descent is smaller), so ending below it at all is lowered text
    if (bottom - mainBottom > mainSize * 0.02f) {
        return -1;
    }
    return 0;
}

// the spans of one block as markdown: lines joined into a paragraph, styles as
// ** and *, monospaced runs as `code`, raised and lowered text as <sup> and
// <sub>. skipFirst: bytes of the first span to leave out (a list item's bullet)
static TempStr BlockInlineTemp(const StyledPage& p, int from, int to, bool keepLines, int skipFirst = 0) {
    str::Builder out;
    int line = -1;
    for (int i = from; i < to; i++) {
        const StyledSpan& s = p.spans[i];
        if (s.line != line) {
            if (line >= 0) {
                if (keepLines) {
                    out.AppendChar('\n');
                } else if (IsBrokenWord(ToStr(out), s.text)) {
                    out.RemoveLast();
                } else {
                    out.AppendChar(' ');
                }
            }
            line = s.line;
        }
        Str text = s.text;
        if (i == from && skipFirst > 0) {
            text = Str(text.s + std::min(skipFirst, len(text)), std::max(0, len(text) - skipFirst));
        }
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
        if (int shift = SpanShift(p, i)) {
            Str tag = shift > 0 ? StrL("sup") : StrL("sub");
            out.Append(fmt("%s<%s>%s</%s>%s", lead, tag, EscapeInlineTemp(trimmed), tag, trail));
            continue;
        }
        const char* mark = s.mono ? "`" : (s.bold && s.italic) ? "***" : s.bold ? "**" : s.italic ? "*" : "";
        if (!s.mono && !HasWordChar(trimmed)) {
            mark = "";
        }
        out.Append(lead);
        out.Append(Str(mark));
        // inside `code` everything is text
        out.Append(s.mono ? str::DupTemp(trimmed) : EscapeInlineTemp(trimmed));
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

// the markdown of a document as it's made: paragraphs, headings, ... and the
// page markers, put together at the end, when a paragraph a page break or a
// picture split can be joined back
enum class MdKind {
    Para,
    List,
    Heading,
    Code,
    Image,
    Marker,
    Table,
};

struct MdChunks {
    StrVec texts;
    Vec<MdKind> kinds;
    void Add(MdKind kind, Str text) {
        texts.Append(text);
        VecAppend(kinds, kind);
    }
};

// paragraph next carries on paragraph prev: it starts in lower case and prev
// doesn't end a sentence
static bool ParaContinues(Str prev, Str next) {
    if (!IsLowerStart(next) || len(prev) == 0) {
        return false;
    }
    char last = prev.s[len(prev) - 1];
    if (strchr(".!?:;\")", last) || str::EndsWith(prev, StrL("\xe2\x80\x9d"))) {
        return false;
    }
    return true;
}

static TempStr JoinChunksTemp(MdChunks& c) {
    str::Builder out;
    for (int i = 0; i < len(c.texts); i++) {
        Str t = c.texts[i];
        if (i > 0) {
            MdKind prev = c.kinds[i - 1];
            MdKind kind = c.kinds[i];
            if (prev == MdKind::Para && kind == MdKind::Para && ParaContinues(c.texts[i - 1], t)) {
                // a word broken across the page break
                if (IsBrokenWord(ToStr(out), t)) {
                    out.RemoveLast();
                } else {
                    out.AppendChar(' ');
                }
            } else if (prev == MdKind::List && kind == MdKind::List) {
                // one list, though its items came in several blocks
                out.AppendChar('\n');
            } else {
                out.Append(StrL("\n\n"));
            }
        }
        out.Append(t);
    }
    if (len(out) > 0) {
        out.AppendChar('\n');
    }
    return ToStrTemp(out);
}

static void AppendImage(MdImages& imgs, const StyledImage& im, int pageNo, MdChunks& out) {
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
    out.Add(MdKind::Image, fmt("![](%s/%s)", imgs.dirRel, name));
}

// one page of a mupdf document as markdown
struct MdTable;
static void FindTables(const StyledPage& p, Vec<MdTable*>& tables);
static TempStr TableMarkdownTemp(const StyledPage& p, const MdTable& t);
static int TableFirstSpan(const MdTable* t);
static bool TableHasSpan(const MdTable* t, int span);
static void FreeTables(Vec<MdTable*>& tables);

static void StyledPageToMarkdown(MdDoc& d, StyledPage& p, int pageNo, MdImages& imgs, MdChunks& out) {
    int nSpans = len(p.spans);
    // tables first: their lines are neither headings nor paragraphs
    Vec<MdTable*> tables;
    FindTables(p, tables);
    Vec<int> inTable;
    for (int k = 0; k < nSpans; k++) {
        int t = -1;
        for (int ti = 0; ti < len(tables) && t < 0; ti++) {
            t = TableHasSpan(tables[ti], k) ? ti : -1;
        }
        VecAppend(inTable, t);
    }
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
        if (inTable[i] >= 0) {
            // the table where its first span is, its other spans skipped
            MdTable* t = tables[inTable[i]];
            if (TableFirstSpan(t) == i) {
                out.Add(MdKind::Table, TableMarkdownTemp(p, *t));
            }
            i++;
            continue;
        }
        int end = i;
        float maxSize = 0;
        int nChars = 0, nMono = 0, nBold = 0;
        // a block's lines in one size class: a heading glued to the
        // paragraph after it, or headings of several sizes, are runs apart
        auto lineLevel = [&](int span) {
            float size = 0;
            for (int k = span; k < nSpans && p.spans[k].line == p.spans[span].line; k++) {
                size = std::max(size, p.spans[k].size);
            }
            return HeadingLevelForSize(d, size);
        };
        int runLevel = lineLevel(i);
        while (end < nSpans && p.spans[end].block == block && inTable[end] < 0) {
            if (end > i && p.spans[end].line != p.spans[end - 1].line && lineLevel(end) != runLevel) {
                break;
            }
            const StyledSpan& s = p.spans[end];
            maxSize = std::max(maxSize, s.size);
            nChars += len(s.text);
            nMono += s.mono ? len(s.text) : 0;
            nBold += s.bold ? len(s.text) : 0;
            end++;
        }
        TempStr plain = BlockPlainTemp(p, i, end);
        TempStr norm = NormalizeForMatchTemp(plain);
        if (len(norm) == 0) {
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
            level = std::min(d.nHeadingLevels + 1, 4);
        }
        if (level > 0) {
            Str text = plain;
            str::TrimWSInPlace(text, str::TrimOpt::Both);
            str::Builder h;
            for (int k = 0; k < level; k++) {
                h.AppendChar('#');
            }
            h.Append(fmt(" %s", EscapeHeadingTemp(text)));
            out.Add(MdKind::Heading, ToStr(h));
        } else if (nMono * 10 >= nChars * 9) {
            out.Add(MdKind::Code, fmt("```\n%s\n```", BlockInlineTemp(p, i, end, true)));
        } else {
            bool bullet = false;
            int skip = 0;
            if (LineListMarker(p, i, &bullet, &skip)) {
                // a list: each line that starts with a bullet or a number is an
                // item, the lines after it until the next one carry it on
                Vec<int> starts;
                LineStarts(p, i, end, starts);
                str::Builder items;
                for (int k = 0; k + 1 < len(starts);) {
                    LineListMarker(p, starts[k], &bullet, &skip);
                    int e = k + 1;
                    bool b2;
                    int s2;
                    while (e + 1 < len(starts) && !LineListMarker(p, starts[e], &b2, &s2)) {
                        e++;
                    }
                    TempStr item = BlockInlineTemp(p, starts[k], starts[e], false, skip);
                    str::TrimWSInPlace(item, str::TrimOpt::Both);
                    items.Append(
                        fmt("%s%s%s", len(items) > 0 ? StrL("\n") : StrL(""), bullet ? StrL("- ") : StrL(""), item));
                    k = e;
                }
                out.Add(MdKind::List, ToStr(items));
            } else {
                TempStr text = BlockInlineTemp(p, i, end, false);
                str::TrimWSInPlace(text, str::TrimOpt::Both);
                out.Add(MdKind::Para, EscapeLineStartTemp(text));
            }
        }
        i = end;
    }
    FreeTables(tables);
}

// a page of a document without styles (DjVu, comics, ...): its text, a
// paragraph per run of lines
static void PlainPageToMarkdown(EngineBase* engine, int pageNo, MdChunks& out) {
    PageText pt = engine->ExtractPageText(pageNo);
    StrVec lines;
    Split(&lines, pt.text, StrL("\n"), false);
    str::Builder para;
    auto flush = [&]() {
        if (len(para) > 0) {
            out.Add(MdKind::Para, EscapeLineStartTemp(EscapeInlineTemp(ToStr(para))));
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
    // largest first; sizes within 1.5pt of the one before are one level (a
    // title set at 48 here and 47 there), and all below the third are the third
    std::sort(bigger.begin(), bigger.end(), [](float a, float b) { return a > b; });
    int level = 0;
    for (int i = 0; i < len(bigger); i++) {
        if (i == 0 || bigger[i - 1] - bigger[i] > 1.5f) {
            level = std::min(level + 1, 3);
        }
        VecAppend(d.headingSizes, bigger[i]);
        VecAppend(d.headingLevels, level);
        d.nHeadingLevels = level;
    }
}

// running headers and footers, and page numbers, sit in the top or bottom
// 12% of a page
constexpr float kMarginBand = 0.12f;

// a roman number as a page prints it: all one case, in the standard form and
// under 400 (no front matter is longer), so words like "mix" (1009) and "did"
// aren't one
static bool IsRomanWord(Str w) {
    if (len(w) == 0 || len(w) > 9) {
        return false;
    }
    bool lower = false, upper = false;
    int total = 0;
    int prev = 0;
    for (int i = len(w) - 1; i >= 0; i--) {
        char c = w.s[i];
        lower = lower || (c >= 'a' && c <= 'z');
        upper = upper || (c >= 'A' && c <= 'Z');
        int v = 0;
        switch (c | 0x20) {
            case 'i':
                v = 1;
                break;
            case 'v':
                v = 5;
                break;
            case 'x':
                v = 10;
                break;
            case 'l':
                v = 50;
                break;
            case 'c':
                v = 100;
                break;
            case 'd':
                v = 500;
                break;
            case 'm':
                v = 1000;
                break;
            default:
                return false;
        }
        total += v < prev ? -v : v;
        prev = std::max(prev, v);
    }
    if ((lower && upper) || total <= 0 || total >= 400) {
        return false;
    }
    // written the standard way: the value written back is the same word
    static const int values[] = {100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char* romans[] = {"c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};
    str::Builder back;
    int n = total;
    for (int i = 0; i < (int)dimofi(values); i++) {
        while (n >= values[i]) {
            back.Append(Str(romans[i]));
            n -= values[i];
        }
    }
    return str::EqI(ToStr(back), w);
}

// digits (up to 4) or a roman number
static bool IsNumberWord(Str w) {
    if (len(w) == 0) {
        return false;
    }
    bool digits = len(w) <= 4;
    for (int i = 0; i < len(w) && digits; i++) {
        digits = w.s[i] >= '0' && w.s[i] <= '9';
    }
    return digits || IsRomanWord(w);
}

// "12", "xii", "Page 3", "3 of 10", "- 4 -", "5 / 9"
static bool IsPageNumberText(Str s) {
    TempStr t = str::DupTemp(s);
    for (int i = 0; i < len(t); i++) {
        if (strchr("/|\t", t.s[i])) {
            t.s[i] = ' ';
        }
    }
    StrVec words;
    Split(&words, t, StrL(" "), true);
    int numbers = 0;
    for (Str w : words) {
        // dashes and brackets around a number: "- 4 -", "[5]"
        int a = 0, b = len(w);
        auto isTrim = [](Str x, int i) {
            u8 c = (u8)x.s[i];
            return strchr("-.()[]", (char)c) != nullptr || c == 0xe2 || c == 0x80 || c == 0x93 || c == 0x94;
        };
        while (a < b && isTrim(w, a)) {
            a++;
        }
        while (b > a && isTrim(w, b - 1)) {
            b--;
        }
        Str core(w.s + a, b - a);
        if (len(core) == 0 || str::EqI(core, StrL("page")) || str::EqI(core, StrL("pg")) || str::EqI(core, StrL("p")) ||
            str::EqI(core, StrL("of"))) {
            continue;
        }
        if (!IsNumberWord(core)) {
            return false;
        }
        numbers++;
    }
    return numbers >= 1 && numbers <= 2;
}

// a line of a page: its spans [from, to)
struct MdLine {
    int from = 0;
    int to = 0;
    float size = 0;
};

static void PageLines(const StyledPage& p, Vec<MdLine>& out) {
    for (int i = 0; i < len(p.spans);) {
        MdLine l;
        l.from = i;
        while (i < len(p.spans) && p.spans[i].line == p.spans[l.from].line) {
            l.size = std::max(l.size, p.spans[i].size);
            i++;
        }
        l.to = i;
        VecAppend(out, l);
    }
}

static bool InTopMargin(const StyledPage& p, const StyledSpan& s) {
    float h = p.mediabox.dy;
    return h > 0 && s.lineBox.y + s.lineBox.dy <= p.mediabox.y + h * kMarginBand;
}

static bool InBottomMargin(const StyledPage& p, const StyledSpan& s) {
    float h = p.mediabox.dy;
    return h > 0 && s.lineBox.y >= p.mediabox.y + h * (1 - kMarginBand);
}

// what makes a line in a margin the same running header on another page: top
// or bottom, its height and its text without the page numbers at either end.
// Empty for a line outside the margins or with nothing but numbers.
static TempStr HeaderKeyTemp(const StyledPage& p, const MdLine& l) {
    const StyledSpan& s = p.spans[l.from];
    bool top = InTopMargin(p, s);
    if (!top && !InBottomMargin(p, s)) {
        return {};
    }
    StrVec words;
    Split(&words, BlockPlainTemp(p, l.from, l.to), StrL(" "), true);
    int a = 0, b = len(words);
    while (a < b && IsNumberWord(words[a])) {
        a++;
    }
    while (b > a && IsNumberWord(words[b - 1])) {
        b--;
    }
    if (a == b) {
        return {};
    }
    str::Builder core;
    for (int i = a; i < b; i++) {
        core.Append(fmt("%s%s", i > a ? StrL(" ") : StrL(""), words[i]));
    }
    return fmt("%s %d %s", top ? StrL("top") : StrL("bottom"), (int)roundf(s.lineBox.y / 4), ToStr(core));
}

// lines no larger than body text at the same place in a margin on more than
// half the pages (of 4 at least, so a short document keeps its title)
static void FindRunning(MdDoc& d, Vec<StyledPage*>& pages) {
    if (len(pages) < 4) {
        return;
    }
    StrVec keys;
    Vec<int> counts;
    for (StyledPage* p : pages) {
        Vec<MdLine> lines;
        PageLines(*p, lines);
        StrVec here;
        for (MdLine& l : lines) {
            if (l.size > d.bodySize + 1) {
                continue;
            }
            TempStr key = HeaderKeyTemp(*p, l);
            if (len(key) == 0 || here.Contains(key)) {
                continue;
            }
            here.Append(key);
            int idx = keys.Find(key);
            if (idx < 0) {
                keys.Append(key);
                VecAppend(counts, 1);
            } else {
                counts[idx]++;
            }
        }
    }
    for (int i = 0; i < len(keys); i++) {
        if (counts[i] * 2 > len(pages)) {
            d.running.Append(keys[i]);
        }
    }
}

// takes the running headers out of a page, and a block that is only a page
// number in a margin
static void DropRunning(MdDoc& d, StyledPage& p) {
    Vec<MdLine> lines;
    PageLines(p, lines);
    Vec<bool> drop;
    for (int i = 0; i < len(p.spans); i++) {
        VecAppend(drop, false);
    }
    for (MdLine& l : lines) {
        TempStr key = HeaderKeyTemp(p, l);
        if (len(key) > 0 && l.size <= d.bodySize + 1 && d.running.Contains(key)) {
            for (int i = l.from; i < l.to; i++) {
                drop[i] = true;
            }
        }
    }
    for (int i = 0; i < len(p.spans);) {
        int block = p.spans[i].block;
        int end = i;
        bool margin = true, bodySized = true;
        str::Builder text;
        while (end < len(p.spans) && p.spans[end].block == block) {
            const StyledSpan& s = p.spans[end];
            if (!drop[end]) {
                margin = margin && (InTopMargin(p, s) || InBottomMargin(p, s));
                // a number in a heading's size is a title ("1"), not a page number
                bodySized = bodySized && s.size <= d.bodySize + 1;
                text.Append(fmt(" %s", s.text));
            }
            end++;
        }
        if (margin && bodySized && IsPageNumberText(ToStr(text))) {
            for (int k = i; k < end; k++) {
                drop[k] = true;
            }
        }
        i = end;
    }
    Vec<StyledSpan> kept;
    for (int i = 0; i < len(p.spans); i++) {
        if (drop[i]) {
            str::Free(p.spans[i].text);
        } else {
            VecAppend(kept, p.spans[i]);
        }
    }
    p.spans = std::move(kept);
}

// --- tables

// inside a row, a gap this many times the text size parts two cells (a word
// space is about a quarter of it)
constexpr float kCellGap = 1.5f;
// a cell longer than this is prose, not a table cell
constexpr int kMaxCellChars = 80;
// rows of a table: at least a header and two more
constexpr int kMinTableRows = 3;

// a cell: spans [from, to) of one line; from < 0 for an empty one
struct MdCell {
    int from = -1;
    int to = -1;
    float x0 = 0;
    float x1 = 0;
};

// a row: the lines that share a baseline, as cells [cellFrom, cellTo) of the
// page's cells, left to right
struct MdRow {
    int cellFrom = 0;
    int cellTo = 0;
    float y0 = 0;
    float y1 = 0;
    float size = 0;
    // pictures before it: a table doesn't go on past one
    int group = 0;
};

struct MdRows {
    Vec<MdRow> rows;
    Vec<MdCell> cells;
};

static TempStr CellPlainTemp(const StyledPage& p, const MdCell& c) {
    TempStr t = BlockPlainTemp(p, c.from, c.to);
    str::TrimWSInPlace(t, str::TrimOpt::Both);
    return t;
}

static int WordCount(Str s) {
    StrVec words;
    Split(&words, s, StrL(" "), true);
    return len(words);
}

// the rows of a page, top to bottom
static void PageRows(const StyledPage& p, MdRows& out) {
    struct RowCell {
        int row;
        MdCell c;
    };
    Vec<MdRow> rows;
    Vec<RowCell> rowCells;
    Vec<MdLine> lines;
    PageLines(p, lines);
    for (MdLine& l : lines) {
        const StyledSpan& first = p.spans[l.from];
        float y0 = first.lineBox.y;
        float y1 = first.lineBox.y + first.lineBox.dy;
        int group = 0;
        for (const StyledImage& im : p.images) {
            group += im.block < first.block ? 1 : 0;
        }
        // the row it is in: one whose height its middle is in
        float mid = (y0 + y1) / 2;
        int row = -1;
        for (int r = 0; r < len(rows); r++) {
            if (rows[r].group == group && mid >= rows[r].y0 && mid <= rows[r].y1) {
                row = r;
            }
        }
        if (row < 0) {
            MdRow r;
            r.y0 = y0;
            r.y1 = y1;
            r.group = group;
            VecAppend(rows, r);
            row = len(rows) - 1;
        }
        rows[row].size = std::max(rows[row].size, l.size);
        // the line's cells: split at wide gaps between its spans
        int firstCell = len(rowCells);
        for (int i = l.from; i < l.to; i++) {
            const StyledSpan& s = p.spans[i];
            if (s.box.IsEmpty()) {
                continue;
            }
            float x0 = s.box.x;
            float x1 = s.box.x + s.box.dx;
            RowCell* last = len(rowCells) > firstCell ? &rowCells[len(rowCells) - 1] : nullptr;
            if (last && x0 - last->c.x1 < s.size * kCellGap) {
                last->c.to = i + 1;
                last->c.x1 = std::max(last->c.x1, x1);
                continue;
            }
            VecAppend(rowCells, RowCell{row, MdCell{i, i + 1, x0, x1}});
        }
    }
    Vec<int> order;
    for (int r = 0; r < len(rows); r++) {
        VecAppend(order, r);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return rows[a].y0 < rows[b].y0; });
    std::sort(rowCells.begin(), rowCells.end(),
              [](const RowCell& a, const RowCell& b) { return a.row != b.row ? a.row < b.row : a.c.x0 < b.c.x0; });
    for (int r : order) {
        MdRow row = rows[r];
        row.cellFrom = len(out.cells);
        for (RowCell& rc : rowCells) {
            if (rc.row == r) {
                VecAppend(out.cells, rc.c);
            }
        }
        row.cellTo = len(out.cells);
        VecAppend(out.rows, row);
    }
}

// a row that could be in a table: two cells or more, all short
static bool IsTableRow(const StyledPage& p, const MdRows& rs, const MdRow& r) {
    if (r.cellTo - r.cellFrom < 2) {
        return false;
    }
    for (int c = r.cellFrom; c < r.cellTo; c++) {
        if (len(CellPlainTemp(p, rs.cells[c])) > kMaxCellChars) {
            return false;
        }
    }
    return true;
}

// a table found on a page: nRows * nCols cells, in their columns
struct MdTable {
    int nRows = 0;
    int nCols = 0;
    Vec<MdCell> grid;
    // the first span of the table in reading order, where it goes
    int firstSpan = INT_MAX;
};

// rows [from, to) placed in the columns of the row with the most cells.
// False if a row has two cells in one column or a column is mostly empty
static bool PlaceInColumns(const MdRows& rs, int from, int to, MdTable& t) {
    int widest = from;
    for (int i = from; i < to; i++) {
        const MdRow& r = rs.rows[i];
        if (r.cellTo - r.cellFrom > rs.rows[widest].cellTo - rs.rows[widest].cellFrom) {
            widest = i;
        }
    }
    const MdRow& w = rs.rows[widest];
    t.nCols = w.cellTo - w.cellFrom;
    t.nRows = to - from;
    Vec<int> filled;
    for (int c = 0; c < t.nCols; c++) {
        VecAppend(filled, 0);
    }
    for (int i = from; i < to; i++) {
        const MdRow& r = rs.rows[i];
        int base = len(t.grid);
        for (int c = 0; c < t.nCols; c++) {
            VecAppend(t.grid, MdCell());
        }
        for (int k = r.cellFrom; k < r.cellTo; k++) {
            const MdCell& cell = rs.cells[k];
            // the column whose cell in the widest row overlaps it most, else
            // whose middle is nearest
            float mid = (cell.x0 + cell.x1) / 2;
            int best = 0;
            float bestScore = -1e9f;
            for (int c = 0; c < t.nCols; c++) {
                const MdCell& col = rs.cells[w.cellFrom + c];
                float overlap = std::min(cell.x1, col.x1) - std::max(cell.x0, col.x0);
                float score = overlap > 0 ? 1000 + overlap : -fabsf(mid - (col.x0 + col.x1) / 2);
                if (score > bestScore) {
                    bestScore = score;
                    best = c;
                }
            }
            if (t.grid[base + best].from >= 0) {
                return false;
            }
            t.grid[base + best] = cell;
            filled[best]++;
        }
    }
    for (int c = 0; c < t.nCols; c++) {
        if (filled[c] * 2 < t.nRows) {
            return false;
        }
    }
    return true;
}

// prose in two columns lines up like a table: its cells are sentences
static bool LooksLikeProse(const StyledPage& p, const MdTable& t) {
    int n = 0, wordy = 0;
    for (const MdCell& c : t.grid) {
        if (c.from >= 0) {
            n++;
            wordy += WordCount(CellPlainTemp(p, c)) >= 5 ? 1 : 0;
        }
    }
    return wordy * 2 > n;
}

// a list with a hanging indent lines up like a table: its first column is
// the bullets or numbers
static bool LooksLikeList(const StyledPage& p, const MdTable& t) {
    for (int i = 0; i < t.nRows; i++) {
        const MdCell& c = t.grid[i * t.nCols];
        if (c.from < 0) {
            return false;
        }
        TempStr text = fmt("%s ", CellPlainTemp(p, c));
        int skip = 0;
        if (!IsBulletStart(text, &skip) && NumberedStart(text) == 0) {
            return false;
        }
    }
    return true;
}

static void FindTables(const StyledPage& p, Vec<MdTable*>& tables) {
    MdRows rs;
    PageRows(p, rs);
    int n = len(rs.rows);
    for (int i = 0; i < n;) {
        // a run of table rows close to each other, with no picture between
        int end = i;
        while (end < n && IsTableRow(p, rs, rs.rows[end])) {
            if (end > i) {
                const MdRow& a = rs.rows[end - 1];
                const MdRow& b = rs.rows[end];
                if (b.group != a.group || b.y0 - a.y1 > 2.5f * std::max(b.size, 1.f)) {
                    break;
                }
            }
            end++;
        }
        if (end - i < kMinTableRows) {
            i = std::max(end, i + 1);
            continue;
        }
        auto* t = new MdTable();
        if (PlaceInColumns(rs, i, end, *t) && !LooksLikeProse(p, *t) && !LooksLikeList(p, *t)) {
            for (const MdCell& c : t->grid) {
                if (c.from >= 0) {
                    t->firstSpan = std::min(t->firstSpan, c.from);
                }
            }
            VecAppend(tables, t);
        } else {
            delete t;
        }
        i = end;
    }
}

static void FreeTables(Vec<MdTable*>& tables) {
    for (MdTable* t : tables) {
        delete t;
    }
    VecClear(tables);
}

static int TableFirstSpan(const MdTable* t) {
    return t->firstSpan;
}

static bool TableHasSpan(const MdTable* t, int span) {
    for (const MdCell& c : t->grid) {
        if (c.from >= 0 && span >= c.from && span < c.to) {
            return true;
        }
    }
    return false;
}

// a table as markdown: the first row is its header, | in a cell is escaped
static TempStr TableMarkdownTemp(const StyledPage& p, const MdTable& t) {
    str::Builder out;
    for (int i = 0; i < t.nRows; i++) {
        out.AppendChar('|');
        for (int c = 0; c < t.nCols; c++) {
            const MdCell& mc = t.grid[i * t.nCols + c];
            TempStr cell;
            if (mc.from >= 0) {
                cell = BlockInlineTemp(p, mc.from, mc.to, false);
                str::TrimWSInPlace(cell, str::TrimOpt::Both);
                cell = str::ReplaceTemp(cell, StrL("|"), StrL("\\|"));
            }
            out.Append(len(cell) > 0 ? fmt(" %s |", cell) : StrL(" |"));
        }
        if (i == 0) {
            out.Append(StrL("\n|"));
            for (int c = 0; c < t.nCols; c++) {
                out.Append(StrL(" --- |"));
            }
        }
        if (i + 1 < t.nRows) {
            out.AppendChar('\n');
        }
    }
    return ToStrTemp(out);
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
        for (StyledPage* p : styled) {
            DropRunning(d, *p);
        }
        TocTree* toc = engine->HasToc() ? engine->GetToc() : nullptr;
        if (toc && toc->root) {
            CollectTocTitles(toc->root->child ? toc->root->child : toc->root, 0, d.tocTitles, d.tocDepths);
        }
    }
    MdChunks out;
    TempStr title = engine->GetPropertyTemp(DocProp::Title);
    for (int k = 0; k < len(pages); k++) {
        if (pageMarkers) {
            out.Add(MdKind::Marker, fmt("<!-- page %d -->", pages[k]));
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
    Str md = JoinChunksTemp(out);
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

//--- watermark, number

// "#rrggbb" or "rrggbb"
static bool ParseHexColor(Str s, u8* r, u8* g, u8* b) {
    str::TrimPrefix(s, StrL("#"));
    if (len(s) != 6) {
        return false;
    }
    int v[3];
    for (int k = 0; k < 3; k++) {
        int hi = 0, lo = 0;
        char ch = s.s[2 * k], cl = s.s[2 * k + 1];
        if (!isxdigit((u8)ch) || !isxdigit((u8)cl)) {
            return false;
        }
        hi = isdigit((u8)ch) ? ch - '0' : (ch | 0x20) - 'a' + 10;
        lo = isdigit((u8)cl) ? cl - '0' : (cl | 0x20) - 'a' + 10;
        v[k] = hi * 16 + lo;
    }
    *r = (u8)v[0];
    *g = (u8)v[1];
    *b = (u8)v[2];
    return true;
}

// the stamps on the PDF given, saved as -o
static int StampPdf(CliCtx& cx, Str path, Vec<PdfStamp>& stamps, Str dest, int nPages) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, path, &code);
    if (!engine) {
        return code;
    }
    Str err;
    bool ok =
        EngineMupdfStampText(engine, stamps, &err) && (dir::CreateForFile(dest), EngineMupdfSaveCopy(engine, dest));
    SafeEngineRelease(&engine);
    if (!ok) {
        return Fail(cx, len(err) > 0 ? kCliUsage : kCliFailed, len(err) > 0 ? err : fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, nPages);
}

static int CmdWatermark(CliCtx& cx) {
    if (len(cx.positional) < 2) {
        return Fail(cx, kCliUsage, StrL("usage: watermark <file.pdf> <text> -o <out.pdf>"));
    }
    Str path = cx.positional[0];
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    float opacity = 0, size = 0;
    if (!FloatOpt(cx, StrL("--opacity"), kWatermarkOpacity, 0.01f, 1, &opacity, &code) ||
        !FloatOpt(cx, StrL("--size"), 0, 1, 1000, &size, &code)) {
        return code;
    }
    u8 r = kWatermarkGray, g = kWatermarkGray, b = kWatermarkGray;
    Str color = OptValue(cx, StrL("--color"));
    if (len(color) > 0 && !ParseHexColor(color, &r, &g, &b)) {
        return Fail(cx, kCliUsage, fmt("--color %s: use #rrggbb", color));
    }
    int n = CheckPdf(cx, path, &code);
    if (n == 0) {
        return code;
    }
    // the page range needs the document: CheckPdf opened it once already
    EngineBase* engine = OpenDoc(cx, path, &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    bool havePages = PagesArg(cx, engine, pages, &code);
    SafeEngineRelease(&engine);
    if (!havePages) {
        return code;
    }
    PdfStamp tmpl;
    tmpl.pos = PdfStampPos::Diagonal;
    tmpl.size = size;
    tmpl.r = r;
    tmpl.g = g;
    tmpl.b = b;
    tmpl.opacity = opacity;
    Vec<PdfStamp> stamps;
    PageStamps(pages, tmpl, cx.positional[1], 1, stamps);
    return StampPdf(cx, path, stamps, dest, n);
}

static int CmdNumber(CliCtx& cx) {
    Str path = cx.positional[0];
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    float size = 0, margin = 0, start = 0;
    if (!FloatOpt(cx, StrL("--size"), kPageNumberSize, 1, 200, &size, &code) ||
        !FloatOpt(cx, StrL("--margin"), kPageNumberMargin, 0, 500, &margin, &code) ||
        !FloatOpt(cx, StrL("--start"), 1, -100000, 100000, &start, &code)) {
        return code;
    }
    u8 r = 0, g = 0, b = 0;
    Str color = OptValue(cx, StrL("--color"));
    if (len(color) > 0 && !ParseHexColor(color, &r, &g, &b)) {
        return Fail(cx, kCliUsage, fmt("--color %s: use #rrggbb", color));
    }
    struct NamedPos {
        const char* name;
        PdfStampPos pos;
    };
    static const NamedPos positions[] = {
        {"top-left", PdfStampPos::TopLeft},           {"top-center", PdfStampPos::TopCenter},
        {"top-right", PdfStampPos::TopRight},         {"bottom-left", PdfStampPos::BottomLeft},
        {"bottom-center", PdfStampPos::BottomCenter}, {"bottom-right", PdfStampPos::BottomRight},
    };
    Str posName = OptValue(cx, StrL("--position"));
    PdfStampPos pos = PdfStampPos::BottomCenter;
    if (len(posName) > 0) {
        bool found = false;
        for (const NamedPos& np : positions) {
            if (str::EqI(posName, Str(np.name))) {
                pos = np.pos;
                found = true;
            }
        }
        if (!found) {
            return Fail(cx, kCliUsage,
                        fmt("--position %s: use top-left, top-center, top-right, bottom-left, bottom-center or "
                            "bottom-right",
                            posName));
        }
    }
    Str format = OptValue(cx, StrL("--format"));
    if (len(format) == 0) {
        format = StrL("{n}");
    }
    int n = CheckPdf(cx, path, &code);
    if (n == 0) {
        return code;
    }
    EngineBase* engine = OpenDoc(cx, path, &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    bool havePages = PagesArg(cx, engine, pages, &code);
    SafeEngineRelease(&engine);
    if (!havePages) {
        return code;
    }
    // the pages numbered count from --start; {total} is the last number
    PdfStamp tmpl;
    tmpl.pos = pos;
    tmpl.size = size;
    tmpl.margin = margin;
    tmpl.r = r;
    tmpl.g = g;
    tmpl.b = b;
    Vec<PdfStamp> stamps;
    PageStamps(pages, tmpl, format, (int)start, stamps);
    return StampPdf(cx, path, stamps, dest, n);
}

//--- annotate

// "x,y" or "x,y,w,h": numbers separated by commas; false if not n of them
static bool ParseFloats(Str s, float* out, int n) {
    StrVec parts;
    Split(&parts, s, StrL(","), true);
    if (len(parts) != n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char* end = nullptr;
        TempStr part = str::DupTemp(parts[i]);
        str::TrimWSInPlace(part, str::TrimOpt::Both);
        out[i] = strtof(CStrTemp(part), &end);
        if (!end || *end != 0 || len(part) == 0) {
            return false;
        }
    }
    return true;
}

// the boxes of every place `text` is on a page, a box per line it is on
static void FindTextBoxes(EngineBase* engine, int pageNo, Str text, bool matchCase, Vec<Vec<RectF>*>& out) {
    PageText pt = engine->ExtractPageText(pageNo);
    Codepoints t, q;
    DecodeText(pt.text, t);
    DecodeText(text, q);
    int n = len(t.cps), nq = len(q.cps);
    for (int i = 0; nq > 0 && i + nq <= n && pt.coords; i++) {
        bool match = true;
        for (int j = 0; j < nq && match; j++) {
            match = FoldCase(t.cps[i + j], matchCase) == FoldCase(q.cps[j], matchCase);
        }
        if (!match) {
            continue;
        }
        auto* rects = new Vec<RectF>();
        for (int j = i; j < i + nq && j < pt.nCodepoints; j++) {
            Rect c = pt.coords[j];
            if (c.IsEmpty()) {
                continue;
            }
            RectF r((float)c.x, (float)c.y, (float)c.dx, (float)c.dy);
            // a character on the line of the last box (its middle within it) widens it
            RectF* last = len(*rects) > 0 ? &(*rects)[len(*rects) - 1] : nullptr;
            float mid = r.y + r.dy / 2;
            if (last && mid >= last->y && mid <= last->y + last->dy) {
                *last = last->Union(r);
            } else {
                VecAppend(*rects, r);
            }
        }
        VecAppend(out, rects);
        i += nq - 1;
    }
    FreePageText(&pt);
}

static int CmdAnnotate(CliCtx& cx) {
    if (len(cx.positional) < 2) {
        return Fail(cx, kCliUsage, StrL("usage: annotate <file.pdf> <type> ... -o <out.pdf>"));
    }
    Str path = cx.positional[0];
    Str type = cx.positional[1];
    struct NamedType {
        const char* name;
        AnnotationType type;
    };
    static const NamedType types[] = {
        {"highlight", AnnotationType::Highlight}, {"underline", AnnotationType::Underline},
        {"strikeout", AnnotationType::StrikeOut}, {"squiggly", AnnotationType::Squiggly},
        {"note", AnnotationType::Text},           {"freetext", AnnotationType::FreeText},
        {"square", AnnotationType::Square},       {"circle", AnnotationType::Circle},
    };
    AnnotationType at = AnnotationType::Unknown;
    for (const NamedType& nt : types) {
        if (str::EqI(type, Str(nt.name))) {
            at = nt.type;
        }
    }
    if (at == AnnotationType::Unknown) {
        return Fail(cx, kCliUsage,
                    fmt("'%s': use highlight, underline, strikeout, squiggly, note, freetext, square or circle", type));
    }
    bool markup = IsTextMarkup(at);
    bool shape = at == AnnotationType::Square || at == AnnotationType::Circle;
    Str text = OptValue(cx, StrL("--text"));
    Str contents = OptValue(cx, StrL("--contents"));
    float at4[4] = {};
    if (markup && len(text) == 0) {
        return Fail(cx, kCliUsage, fmt("%s needs --text <the words to mark>", type));
    }
    if (!markup && !shape && !ParseFloats(OptValue(cx, StrL("--at")), at4, 2)) {
        return Fail(cx, kCliUsage, fmt("%s needs --at x,y (points from the page's top left)", type));
    }
    if (shape && !ParseFloats(OptValue(cx, StrL("--rect")), at4, 4)) {
        return Fail(cx, kCliUsage, fmt("%s needs --rect x,y,w,h (points from the page's top left)", type));
    }
    if ((at == AnnotationType::Text || at == AnnotationType::FreeText) && len(contents) == 0) {
        return Fail(cx, kCliUsage, fmt("%s needs --contents <its text>", type));
    }
    Str color = OptValue(cx, StrL("--color"));
    u8 r = 0, g = 0, b = 0;
    if (len(color) > 0 && !ParseHexColor(color, &r, &g, &b)) {
        return Fail(cx, kCliUsage, fmt("--color %s: use #rrggbb", color));
    }
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    if (CheckPdf(cx, path, &code) == 0) {
        return code;
    }
    EngineBase* engine = OpenDoc(cx, path, &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    // a note or a shape goes on one page: --pages or the first
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    if (!markup) {
        int first = pages[0];
        VecReset(pages);
        VecAppend(pages, first);
    }
    AnnotCreateArgs args;
    args.annotType = at;
    args.content = contents;
    if (len(color) > 0) {
        ParseColor(args.col, fmt("#%02x%02x%02x", r, g, b));
    }
    if (shape) {
        args.hasRect = true;
        args.rect = RectF(at4[0], at4[1], at4[2], at4[3]);
    }
    int n = 0;
    str::Builder out;
    bool failed = false;
    EngineMupdfBeginOperation(engine, "cli annotate");
    for (int pageNo : pages) {
        // a markup goes on every place the text is; a note or a shape once
        Vec<Vec<RectF>*> places;
        if (markup) {
            FindTextBoxes(engine, pageNo, text, HasOpt(cx, StrL("--match-case")), places);
        }
        int nPlaces = markup ? len(places) : 1;
        for (int k = 0; k < nPlaces && !failed; k++) {
            Annotation* annot = EngineMupdfCreateAnnotation(engine, pageNo, PointF(at4[0], at4[1]), &args);
            if (!annot) {
                failed = true;
                break;
            }
            if (markup) {
                SetQuadPointsAsRect(annot, *places[k]);
            }
            // creating a note doesn't set its text (free text's it does)
            if (at == AnnotationType::Text) {
                SetContents(annot, contents);
            }
            RectF rect = GetRect(annot);
            if (cx.json) {
                out.Append(fmt("%s{\"page\":%d,\"type\":%s,\"rect\":[%.1f,%.1f,%.1f,%.1f]}",
                               n > 0 ? StrL(",") : StrL(""), pageNo, JStr(AnnotationTypeName(at)), rect.x, rect.y,
                               rect.dx, rect.dy));
            } else {
                out.Append(fmt("page %d: %s\n", pageNo, AnnotationTypeName(at)));
            }
            n++;
        }
        for (Vec<RectF>* v : places) {
            delete v;
        }
    }
    EngineMupdfEndOperation(engine);
    if (failed || (markup && n == 0)) {
        SafeEngineRelease(&engine);
        return Fail(cx, failed ? kCliFailed : kCliUsage,
                    failed ? StrL("couldn't make the annotation") : fmt("'%s' isn't on the pages", text));
    }
    dir::CreateForFile(dest);
    bool ok = EngineMupdfSaveCopy(engine, dest);
    SafeEngineRelease(&engine);
    if (!ok) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", dest));
    }
    if (cx.json) {
        Out(fmt("{\"file\":%s,\"count\":%d,\"annotations\":[%s]}\n", JStr(dest), n, ToStr(out)));
    } else {
        Out(fmt("%s%s: %d annotations\n", ToStr(out), dest, n));
    }
    return kCliOk;
}

//--- sign, verify

// a copy of the PDF signed with the certificate and key in a .pfx / .p12
static int CmdSign(CliCtx& cx) {
    Str path = cx.positional[0];
    Str key = OptValue(cx, StrL("--key"));
    if (len(key) == 0) {
        return Fail(cx, kCliUsage, StrL("needs --key <file.pfx>: the certificate and private key to sign with"));
    }
    if (!file::Exists(key)) {
        return Fail(cx, kCliCantOpen, fmt("'%s' doesn't exist", key));
    }
    Str keyPassword = OptValue(cx, StrL("--key-password"));
    Str keyPasswordEnv = OptValue(cx, StrL("--key-password-env"));
    if (len(keyPasswordEnv) > 0) {
        const char* v = getenv(CStrTemp(keyPasswordEnv));
        if (!v) {
            return Fail(cx, kCliUsage, fmt("--key-password-env: %s isn't set", keyPasswordEnv));
        }
        keyPassword = Str(v);
    }
    float rect[4] = {};
    Str rectStr = OptValue(cx, StrL("--rect"));
    if (len(rectStr) > 0 && !ParseFloats(rectStr, rect, 4)) {
        return Fail(cx, kCliUsage, fmt("--rect %s: use x,y,w,h in points from the page's top left", rectStr));
    }
    StrVec inputs;
    inputs.Append(path);
    int code = kCliOk;
    Str dest = PdfOutputArg(cx, inputs, &code);
    if (len(dest) == 0) {
        return code;
    }
    if (CheckPdf(cx, path, &code) == 0) {
        return code;
    }
    EngineBase* engine = OpenDoc(cx, path, &code);
    if (!engine) {
        return code;
    }
    Vec<int> pages;
    if (!PagesArg(cx, engine, pages, &code)) {
        SafeEngineRelease(&engine);
        return code;
    }
    PdfSignArgs args;
    args.certPath = key;
    args.certPassword = keyPassword;
    args.fieldName = OptValue(cx, StrL("--field"));
    args.reason = OptValue(cx, StrL("--reason"));
    args.location = OptValue(cx, StrL("--location"));
    args.pageNo = pages[0];
    if (len(rectStr) > 0) {
        args.rect = RectF(rect[0], rect[1], rect[2], rect[3]);
    }
    Str err;
    // the signature is made while saving
    bool ok =
        EngineMupdfSignDocument(engine, args, &err) && (dir::CreateForFile(dest), EngineMupdfSaveCopy(engine, dest));
    int nPages = engine->PageCount();
    SafeEngineRelease(&engine);
    if (!ok) {
        return Fail(cx, kCliFailed, len(err) > 0 ? fmt("couldn't sign: %s", err) : fmt("couldn't write '%s'", dest));
    }
    return ReportPdfWritten(cx, dest, nPages);
}

// the signatures and whether each holds: valid (the bytes as signed, by a
// trusted certificate), untrusted (the bytes as signed, by a certificate not
// trusted here) or invalid (changed since signing); exit 0 only when there is
// a signature and every one is valid
static int CmdVerify(CliCtx& cx) {
    int code = kCliOk;
    EngineBase* engine = OpenDoc(cx, cx.positional[0], &code);
    if (!engine) {
        return code;
    }
    if (!EngineMupdfIsPdf(engine)) {
        SafeEngineRelease(&engine);
        return Fail(cx, kCliNotSupported, StrL("only a PDF is checked for signatures"));
    }
    Vec<PdfSigCheck*> checks;
    bool checked = EngineMupdfVerifySignatures(engine, checks);
    SafeEngineRelease(&engine);
    if (!checked) {
        DeleteVecMembers(checks);
        return Fail(cx, kCliNotSupported, StrL("signatures can't be checked on this platform"));
    }
    str::Builder out;
    int n = 0;
    bool allValid = true;
    for (PdfSigCheck* c : checks) {
        if (!c->isSigned) {
            continue;
        }
        Str verdict = !c->digestOk ? StrL("invalid") : c->certOk ? StrL("valid") : StrL("untrusted");
        allValid = allValid && c->digestOk && c->certOk;
        if (cx.json) {
            out.Append(fmt("%s{\"page\":%d,\"signer\":%s,\"verdict\":%s,\"digestOk\":%s,\"certOk\":%s",
                           n > 0 ? StrL(",") : StrL(""), c->pageNo, JStr(c->signer), JStr(verdict),
                           c->digestOk ? StrL("true") : StrL("false"), c->certOk ? StrL("true") : StrL("false")));
            if (len(c->certError) > 0) {
                out.Append(fmt(",\"certError\":%s", JStr(c->certError)));
            }
            out.Append(fmt(",\"changedAfter\":%s,\"timestamp\":%s}", c->changedAfter ? StrL("true") : StrL("false"),
                           c->isTimestamp ? StrL("true") : StrL("false")));
        } else {
            TempStr where = c->pageNo > 0 ? fmt("page %d", c->pageNo) : str::DupTemp(StrL("not on a page"));
            out.Append(fmt("%s (%s): %s", len(c->signer) > 0 ? c->signer : StrL("unknown signer"), where, verdict));
            if (len(c->certError) > 0) {
                out.Append(fmt(", %s", c->certError));
            }
            if (c->changedAfter) {
                out.Append(StrL(", the file was changed after it"));
            }
            out.AppendChar('\n');
        }
        n++;
    }
    DeleteVecMembers(checks);
    bool ok = n > 0 && allValid;
    if (cx.json) {
        Out(fmt("{\"count\":%d,\"valid\":%s,\"signatures\":[%s]}\n", n, ok ? StrL("true") : StrL("false"), ToStr(out)));
    } else {
        Out(n > 0 ? ToStrTemp(out) : str::DupTemp(StrL("no signatures\n")));
    }
    return ok ? kCliOk : kCliFailed;
}

//--- agent-skill

// SKILL.md for Claude Code, Codex and other agents: when to use the cli and
// how. <EXE> is this executable's path.
static const char* kSkillMd = R"(---
name: sumatrapdf
description: Read and convert documents with the SumatraPDF command line - PDF, EPUB, MOBI, FB2, XPS, DjVu, CHM and comic books (CBZ, CBR). Gets the text or Markdown, search hits with page numbers, the table of contents, annotations with the text they mark, metadata and page images; keeps, deletes, reorders and rotates PDF pages, merges, compresses, encrypts and decrypts PDFs. Use it whenever a task needs what is inside such a file.
---

# SumatraPDF command line

Run: `"<EXE>" cli <command> [options]`

Add `--json` to every command: the output is then one JSON object on stdout.
On an error that object is `{"error": {"code": <exit code>, "message": "..."}}`.
`"<EXE>" cli <command> --help` shows a command's options and examples.

The cli never changes the files it is given: the commands that make a file
need `-o <output>` and refuse to write over an input.

## Reading a document

- `info <file>`: format, page count, page size, title, author, dates
- `markdown <file> [-o out.md]`: the whole document as Markdown, with
  `<!-- page N -->` before each page. Far fewer tokens than page images: read
  this first. With -o, pictures go to `out_images/` next to it
- `text <file> [--pages 3-5]`: plain text, page by page
- `search <file> "words" [--whole-word] [--match-case]`: every hit with its
  page number and some context; then read those pages with `text --pages`
- `toc <file>`: the table of contents with page numbers
- `annotations <file.pdf>`: highlights (with the text they mark), notes, shapes
- `images <file> -o folder/`: the pictures in the pages, a PDF's JPEGs as they
  are, others as PNG
- `render <file> -o page.png [--pages 2] [--dpi 150]`: pages as images, for
  figures, scans and layout; `<N>` in the name is the page number

Page ranges: `1-3,7,10-N` (N is the last page). Page numbers are 1-based.

## Changing a PDF (always into a new file)

- `pages keep <file.pdf> 1-3,7 -o out.pdf`
- `pages delete <file.pdf> 2 -o out.pdf`
- `pages order <file.pdf> 3,1,2 -o out.pdf`
- `pages rotate <file.pdf> 2-N 90 -o out.pdf` (clockwise: 90, 180, 270)
- `merge a.pdf b.pdf -o both.pdf`
- `images-to-pdf scan1.jpg scan2.png -o scans.pdf`
- `split <file.pdf> -o folder/ [--every 10 | --ranges "1-3;4-N"]`
- `compress <file.pdf> -o small.pdf`
- `encrypt <file.pdf> -o locked.pdf --new-password-env NAME`
- `sign <file.pdf> --key me.pfx --key-password-env NAME [--pages 2 --rect x,y,w,h] -o signed.pdf`
- `verify <file.pdf>`: each signature valid, untrusted (intact, signer not
  trusted here) or invalid; exit 0 only if all are valid
- `decrypt <file.pdf> -o open.pdf --password-env NAME`
- `watermark <file.pdf> DRAFT -o out.pdf [--opacity 0.25] [--color #808080]`
- `number <file.pdf> -o out.pdf [--format "Page {n} of {total}"] [--position bottom-center]`
- `annotate <file.pdf> highlight --text "words" -o out.pdf` (also underline,
  strikeout, squiggly; `note --at x,y --contents ...`; `square --rect x,y,w,h`)

An encrypted document needs `--password <pw>`, or better `--password-env NAME`
so the password isn't on the command line.

## Exit codes

- 0: success
- 1: the command failed
- 2: wrong command line
- 3: the document doesn't exist or can't be opened
- 4: encrypted: no password or a wrong one
- 5: the command doesn't apply to this kind of document (a PDF tool on an EPUB)
)";

static TempStr HomeDirTemp() {
#if OS_WIN
    const char* home = getenv("USERPROFILE");
#else
    const char* home = getenv("HOME");
#endif
    return home ? str::DupTemp(Str(home)) : TempStr{};
}

static int CmdAgentSkill(CliCtx& cx) {
    TempStr md = str::ReplaceTemp(Str(kSkillMd), StrL("<EXE>"), GetSelfExePathTemp());
    Str where = OptValue(cx, StrL("--install"));
    if (len(where) == 0) {
        if (cx.json) {
            Out(fmt("{\"skill\":%s}\n", JStr(md)));
        } else {
            Out(md);
        }
        return kCliOk;
    }
    // where each agent looks for skills
    TempStr dir;
    if (str::Eq(where, StrL("claude"))) {
        dir = path::JoinTemp(HomeDirTemp(), StrL(".claude/skills"));
    } else if (str::Eq(where, StrL("codex"))) {
        dir = path::JoinTemp(HomeDirTemp(), StrL(".codex/skills"));
    } else if (str::Eq(where, StrL("agents"))) {
        dir = path::JoinTemp(HomeDirTemp(), StrL(".agents/skills"));
    } else {
        dir = str::DupTemp(where);
    }
    if (len(dir) == 0) {
        return Fail(cx, kCliFailed, StrL("can't find the home directory"));
    }
    TempStr path = path::JoinTemp(path::JoinTemp(dir, StrL("sumatrapdf")), StrL("SKILL.md"));
    path = path::NormalizeTemp(path);
    dir::CreateForFile(path);
    if (!file::WriteFile(path, md)) {
        return Fail(cx, kCliFailed, fmt("couldn't write '%s'", path));
    }
    if (cx.json) {
        Out(fmt("{\"file\":%s}\n", JStr(path)));
    } else {
        Out(fmt("installed %s\n", path));
    }
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
    {"images", "<file> -o <folder>",
     "the pictures in the pages, as the document has them (a PDF's JPEGs, a comic's pages) or as PNG",
     "-o <folder>      where they go, as <name>-p<page>-<n>.<ext>\n--pages <range>  only these pages",
     "images report.pdf -o pics/\nimages comic.cbz --pages 1-5 -o pages/ --json", CmdImages},
    {"render", "<file> -o <image>", "pages as images (PNG, JPEG, BMP or GIF, by the file's extension)",
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
    {"images-to-pdf", "<image> ... -o <out.pdf>",
     "a PDF with a page per image (JPEG, PNG, GIF, TIFF, WebP, HEIC, AVIF, ...), in the order given",
     "-o <file>        the new PDF", "images-to-pdf scan1.jpg scan2.jpg -o scans.pdf", CmdImagesToPdf},
    {"split", "<file.pdf> -o <folder>", "a PDF per page, or per group of pages, named <name>-<first>[-<last>].pdf",
     "-o <folder>      where the PDFs go\n--every <n>      n pages to a PDF (default 1)\n--ranges <r;r>   a PDF per "
     "range, e.g. \"1-3;4-9;10-N\"",
     "split book.pdf -o chapters/ --ranges \"1-12;13-40;41-N\"\nsplit scan.pdf -o pages/", CmdSplit},
    {"compress", "<file.pdf> -o <out.pdf>", "a smaller copy of a PDF (same quality: unused objects and streams go)",
     "-o <file>        the new PDF", "compress big.pdf -o small.pdf --json", CmdCompress},
    {"decompress", "<file.pdf> -o <out.pdf>", "a copy of a PDF with its streams uncompressed, for reading its insides",
     "-o <file>        the new PDF", "decompress a.pdf -o readable.pdf", CmdDecompress},
    {"encrypt", "<file.pdf> -o <out.pdf> --new-password <pw>", "a copy of a PDF that needs a password (AES-256)",
     "-o <file>        the new PDF\n--new-password <pw>  or --new-password-env <NAME>",
     "encrypt a.pdf -o locked.pdf --new-password-env PDF_PW", CmdEncrypt},
    {"decrypt", "<file.pdf> -o <out.pdf> --password <pw>", "a copy of an encrypted PDF without the password",
     "-o <file>        the new PDF", "decrypt locked.pdf -o open.pdf --password-env PDF_PW", CmdDecrypt},
    {"watermark", "<file.pdf> <text> -o <out.pdf>", "a copy of a PDF with text across its pages (Latin text)",
     "-o <file>        the new PDF\n--pages <range>  only these pages\n--opacity <0-1>  default 0.25\n"
     "--color <#rgb>   default #808080\n--size <pt>      default: across 60% of the page's diagonal",
     "watermark report.pdf DRAFT -o draft.pdf\nwatermark a.pdf \"Do not copy\" --color #cc0000 --opacity 0.15 -o b.pdf",
     CmdWatermark},
    {"number", "<file.pdf> -o <out.pdf>", "a copy of a PDF with page numbers on its pages (Latin text)",
     "-o <file>          the new PDF\n--format <text>    {n} is the number, {total} the last one; default {n}\n"
     "--position <where> top-left, top-center, top-right, bottom-left, bottom-center (default), bottom-right\n"
     "--start <n>        the first page's number, default 1\n--pages <range>    only these pages, numbered from "
     "the first of them\n--size <pt>        default 10\n--margin <pt>      from the page's edge, default 30\n"
     "--color <#rgb>     default #000000",
     "number report.pdf -o numbered.pdf\nnumber book.pdf --pages 5-N --format \"Page {n} of {total}\" -o out.pdf",
     CmdNumber},
    {"annotate", "<file.pdf> <type> ... -o <out.pdf>",
     "a copy of a PDF with annotations: words highlighted (underlined, ...), a note, a text box or a shape",
     "-o <file>          the new PDF\n"
     "highlight, underline, strikeout, squiggly: --text <words>  every place they are on --pages (default all)\n"
     "note, freetext:     --at x,y --contents <text>  on the first of --pages\n"
     "square, circle:     --rect x,y,w,h  on the first of --pages\n"
     "--color <#rgb>     the annotation's color\n--match-case       for --text\n"
     "x, y, w, h are points from the page's top left, as `search` gives them",
     "annotate paper.pdf highlight --text \"main result\" -o marked.pdf\n"
     "annotate paper.pdf note --pages 3 --at 72,100 --contents \"check this\" -o noted.pdf",
     CmdAnnotate},
    {"sign", "<file.pdf> --key <file.pfx> -o <out.pdf>",
     "a copy of a PDF signed with the certificate and private key in a .pfx / .p12 file",
     "-o <file>                  the signed PDF\n--key <file.pfx>           the certificate and key\n"
     "--key-password <pw>        or --key-password-env <NAME>\n--pages <n>                the page the "
     "signature goes on (default 1)\n--rect x,y,w,h             where on it (default: mupdf's box)\n"
     "--field <name>             an unsigned signature field to fill in instead\n--reason, --location <text>  "
     "shown in the signature",
     "sign contract.pdf --key me.pfx --key-password-env PFX_PASS --pages 2 --rect 72,700,200,60 -o signed.pdf",
     CmdSign},
    {"verify", "<file.pdf>",
     "the signatures and whether each holds: valid, untrusted (intact, signer not trusted) or invalid; exit 0 "
     "only if all are valid",
     "", "verify signed.pdf\nverify contract.pdf --json", CmdVerify},
    {"agent-skill", "[--install claude|codex|agents|<dir>]",
     "the SKILL.md that tells an AI agent when and how to use these commands",
     "--install <where>  write it to ~/.claude/skills/sumatrapdf (claude), ~/.codex/skills (codex),\n"
     "                   ~/.agents/skills (agents) or <dir>/sumatrapdf; without it, print it",
     "agent-skill --install claude\nagent-skill > SKILL.md", CmdAgentSkill},
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
    // "[...]": the arguments are optional
    if (len(cx.positional) < 1 && *cx.cmd->args && cx.cmd->args[0] != '[') {
        return Fail(cx, kCliUsage, fmt("usage: SumatraPDF cli %s %s", Str(cx.cmd->name), Str(cx.cmd->args)));
    }
    // the engines read a few settings (ebook font, ...): the defaults, without
    // reading or writing the user's settings file
    if (!gSettings) {
        gSettings = NewSettings(Str{});
    }
    int res = cx.cmd->fn(cx);
    WaitForCbxPrefetches();
    fflush(stdout);
    fflush(stderr);
    return res;
}
