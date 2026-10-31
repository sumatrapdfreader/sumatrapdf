/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* RTF -> HTML, rendered by MuPDF's HTML engine (like .txt and .pdb).

   rtf bytes --RtfParser--> runs of text with the group's formatting --RtfSink--> html

   The parser keeps a stack of group states (character and paragraph
   formatting, destination). Text is decoded from the font's or document's code
   page (\'hh, plain bytes) or from \uN. Each sink builds paragraphs, tables and
   their runs; footnotes get their own sink and are appended at the end.

   Not converted: headers / footers, EMF / WMF pictures, floating shapes, text
   boxes, exact tab stops and columns. */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "RtfDoc.h"

#include "SumatraLog.h"

constexpr int kDefaultFontSize = 24; // half-points: 12pt
constexpr float kTwipsPerPt = 20.f;
constexpr float kPtPerPx = 0.75f; // at 96 dpi
constexpr uint kDefaultCodePage = 1252;
constexpr int kMaxGroupDepth = 512; // deeper groups are parsed as if flattened
constexpr int kMaxHeadingLevel = 6;
constexpr int kBiBitfields = 3;
constexpr int kBmpFileHeaderSize = 14;
constexpr int kBitmapInfoHeaderSize = 40;

// sorted: keywords are found with a binary search
enum class RtfKw : u8 {
    // clang-format off
    Ansi, Ansicpg, Author, B, Bin, Bkmkstart, Blue, Brdrdash, Brdrdb, Brdrdot, Brdrnil, Brdrnone, Brdrs, Brdrsh,
    Brdrth, Bullet, Caps, Cb, Cell, Cellx, Cf, Chftn, Clbrdrb, Clbrdrl, Clbrdrr, Clbrdrt, Colortbl, Cpg, Deff,
    Deleted, Dibitmap, Do, Emdash, Emfblip, Emspace, Endash, Enspace, F, Fcharset, Fdecor, Fi, Field, Fldinst,
    Fldrslt, Fmodern, Fnil, Fonttbl, Footer, Footerf, Footerl, Footerr, Footnote, Froman, Fs, Fscript, Fswiss,
    Ftech, Green, Header, Headerf, Headerl, Headerr, Highlight, I, Info, Intbl, Jpegblip, Ldblquote, Li, Line,
    Lquote, Mac, Nestcell, Nestrow, Nonshppict, Nosupersub, Outlinelevel, Page, Pagebb, Par, Pard, Pc, Pca, Pich, Pichgoal,
    Picscalex, Picscaley, Pict, Picw, Picwgoal, Plain, Pngblip, Qc, Qj, Ql, Qr, Rdblquote, Red, Revtbl, Ri, Row,
    Rquote, Sa, Sb, Sbkcol, Sbknone, Scaps, Sect, Shppict, Strike, Striked, Stylesheet, Sub, Subject, Super, Tab, Tc, Title, Trowd,
    U, Uc, Ud, Ul, Uld, Uldash, Uldashd, Uldb, Ulhwave, Ulnone, Ulth, Ulw, Ulwave, Upr, V, Wbitmap, Wmetafile, Xe,
    Zwj, Zwnj,
    // clang-format on
};

// same order as RtfKw
static const char* gRtfKeywords[] = {
    // clang-format off
    "ansi", "ansicpg", "author", "b", "bin", "bkmkstart", "blue", "brdrdash", "brdrdb", "brdrdot", "brdrnil",
    "brdrnone", "brdrs", "brdrsh", "brdrth", "bullet", "caps", "cb", "cell", "cellx", "cf", "chftn", "clbrdrb",
    "clbrdrl", "clbrdrr", "clbrdrt", "colortbl", "cpg", "deff", "deleted", "dibitmap", "do", "emdash", "emfblip",
    "emspace", "endash", "enspace", "f", "fcharset", "fdecor", "fi", "field", "fldinst", "fldrslt", "fmodern",
    "fnil", "fonttbl", "footer", "footerf", "footerl", "footerr", "footnote", "froman", "fs", "fscript", "fswiss",
    "ftech", "green", "header", "headerf", "headerl", "headerr", "highlight", "i", "info", "intbl", "jpegblip",
    "ldblquote", "li", "line", "lquote", "mac", "nestcell", "nestrow", "nonshppict", "nosupersub", "outlinelevel",
    "page", "pagebb", "par", "pard", "pc", "pca", "pich", "pichgoal", "picscalex", "picscaley", "pict", "picw", "picwgoal",
    "plain", "pngblip", "qc", "qj", "ql", "qr", "rdblquote", "red", "revtbl", "ri", "row", "rquote", "sa", "sb",
    "sbkcol", "sbknone", "scaps", "sect", "shppict", "strike", "striked", "stylesheet", "sub", "subject", "super", "tab", "tc", "title",
    "trowd", "u", "uc", "ud", "ul", "uld", "uldash", "uldashd", "uldb", "ulhwave", "ulnone", "ulth", "ulw",
    "ulwave", "upr", "v", "wbitmap", "wmetafile", "xe", "zwj", "zwnj",
    // clang-format on
};

static int FindRtfKeyword(Str name) {
    int lo = 0;
    int hi = dimofi(gRtfKeywords) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        Str kw = Str(gRtfKeywords[mid]);
        int n = std::min(len(kw), len(name));
        int cmp = memcmp(kw.s, name.s, n);
        if (cmp == 0) {
            cmp = len(kw) - len(name);
        }
        if (cmp == 0) {
            return mid;
        }
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

// where a group's text goes
enum class RtfDest : u8 {
    Text,
    Skip,
    FontTable,
    ColorTable,
    Info,
    Title,
    Pict,
    FieldInst,
    Bookmark,
};

enum class RtfAlign : u8 {
    Left,
    Center,
    Right,
    Justify,
};

enum class RtfPicKind : u8 {
    Unknown,
    Png,
    Jpeg,
    Dib,
    Unsupported, // EMF, WMF, device-dependent bitmaps
};

struct RtfCharFmt {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool caps = false;
    bool smallCaps = false;
    bool hidden = false;
    bool super = false;
    bool sub = false;
    int fontSize = kDefaultFontSize;
    int font = -1;   // index into RtfParser::fonts, -1: the default font
    int color = 0;   // index into the color table, 0: auto
    int bgColor = 0; // same
    int link = -1;   // index into RtfParser::links
};

struct RtfParaFmt {
    RtfAlign align = RtfAlign::Left;
    int leftIndent = 0; // twips
    int rightIndent = 0;
    int firstIndent = 0;
    int spaceBefore = 0;
    int spaceAfter = 0;
    int outlineLevel = -1; // 0..5 become h1..h6
    bool pageBreakBefore = false;
    bool inTable = false;
};

struct RtfGroup {
    RtfCharFmt cf;
    RtfParaFmt pf;
    RtfDest dest = RtfDest::Text;
    RtfDest uprParentDest = RtfDest::Skip; // \ud shows what \upr hid
    int uc = 1;                            // fallback chars to skip after \uN
    bool startsFootnote = false;
};

struct RtfFont {
    int num = 0;
    uint codePage = 0; // 0: the document's code page
    bool isSymbol = false;
    const char* family = nullptr; // CSS generic family
    int nameIdx = -1;             // into RtfParser::fontNames
};

// html of the document body or of one footnote
struct RtfSink {
    str::Builder html;
    str::Builder para; // runs of the open paragraph
    bool paraHasContent = false;

    // the open run: <a> and <span> written to para
    bool runOpen = false;
    str::Builder runStyle;
    int runLink = -1;

    // \page, written before the next block so a trailing one adds no blank page
    bool breakPending = false;

    bool tableOpen = false;
    str::Builder row;  // finished cells of the open row
    str::Builder cell; // paragraphs of the open cell
    int cellIdx = 0;
};

struct RtfParser {
    Str src;
    int pos = 0;

    Vec<RtfGroup> groups; // enclosing groups; g is the innermost
    RtfGroup g;
    int flattenedDepth = 0; // groups past kMaxGroupDepth
    bool starPending = false;

    uint ansiCodePage = kDefaultCodePage;
    int defaultFontNum = -1;
    int defaultFont = -1;

    Vec<RtfFont> fonts;
    StrVec fontNames;
    RtfFont curFont;
    bool inFontEntry = false;
    str::Builder fontName;

    Vec<int> colors; // 0xRRGGBB, -1: auto
    int red = -1;
    int green = -1;
    int blue = -1;

    str::Builder title;
    str::Builder fieldInst;
    str::Builder bookmark;
    StrVec links;

    Vec<int> cellx; // right cell edges of the current row, in twips
    bool rowBorders = false;
    bool borderPending = false;

    RtfPicKind picKind = RtfPicKind::Unknown;
    int picw = 0;
    int picwGoal = 0;
    int picScaleX = 100;
    str::Builder picData;
    int picNibble = -1;
    int nSkippedPics = 0;

    str::Builder pendingBytes; // undecoded text in the current code page
    int skipFallback = 0;
    int highSurrogate = 0;

    Vec<RtfSink*> sinks;
    StrVec footnotes;
    int footnoteNo = 0;

    explicit RtfParser(Str s) : src(s) {}
    ~RtfParser();

    void Parse();
    Str BuildHtml();

    RtfSink* Sink() { return sinks[len(sinks) - 1]; }
    void PushGroup();
    void PopGroup();
    void SetDest(RtfDest d);
    void ParseControl();
    void Keyword(RtfKw kw, bool hasParam, int param);

    void AddByte(u8 c);
    void FlushText();
    void EmitCodepoint(int cp);
    void EmitUnicode(int u);
    void Emit(Str utf8);
    void WriteText(Str utf8);
    void WriteRaw(Str html);

    int FontIndex(int num);
    uint CodePage();
    void FinishFont();
    void FinishColor();
    TempStr CharStyleTemp(const RtfCharFmt& cf);
    void OpenRun(RtfSink* s);
    void CloseRun(RtfSink* s);

    void EndPara(bool explicitPar);
    void EndCell();
    void EndRow();
    void CloseTable(RtfSink* s);
    void PageBreak();
    void WritePendingBreak(RtfSink* s);
    void FinishSink(RtfSink* s);
    void StartFootnote();
    void EndFootnote();
    void EmitPicture();
    void EmitBookmark();
};

RtfParser::~RtfParser() {
    for (RtfSink* s : sinks) {
        delete s;
    }
}

static void AppendEscaped(str::Builder& out, Str s) {
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        switch (c) {
            case '&':
                out.Append(StrL("&amp;"));
                break;
            case '<':
                out.Append(StrL("&lt;"));
                break;
            case '>':
                out.Append(StrL("&gt;"));
                break;
            case '"':
                out.Append(StrL("&quot;"));
                break;
            default:
                out.AppendChar(c);
        }
    }
}

static int HexVal(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool IsAsciiLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// \fcharset values from the RTF spec
static uint CharsetToCodePage(int charset) {
    switch (charset) {
        case 77:
            return 10000;
        case 128:
            return 932;
        case 129:
            return 949;
        case 130:
            return 1361;
        case 134:
            return 936;
        case 136:
            return 950;
        case 161:
            return 1253;
        case 162:
            return 1254;
        case 163:
            return 1258;
        case 177:
            return 1255;
        case 178:
            return 1256;
        case 186:
            return 1257;
        case 204:
            return 1251;
        case 222:
            return 874;
        case 238:
            return 1250;
        case 255:
            return 437;
    }
    return 0;
}

// list bullets in Symbol / Wingdings fonts (\fcharset2)
static int SymbolToCodepoint(u8 c) {
    switch (c) {
        case 0xa7:
            return 0x25aa; // small square
        case 0xb7:
            return 0x2022; // bullet
        case 0xd8:
            return 0x27a2; // arrowhead
        case 0xfc:
            return 0x2713; // check mark
    }
    return c;
}

static float TwipsToPt(int twips) {
    return (float)twips / kTwipsPerPt;
}

// target of a HYPERLINK field: HYPERLINK "url" \l "anchor" \o "tooltip"
static TempStr HyperlinkTargetTemp(Str inst) {
    str::TrimWs(inst);
    Str kw = StrL("HYPERLINK");
    if (!str::StartsWithI(inst, kw)) {
        return {};
    }
    const char* s = inst.s + len(kw);
    const char* end = inst.s + len(inst);
    auto nextArg = [&]() -> Str {
        while (s < end && str::IsWs(*s)) {
            s++;
        }
        if (s >= end) {
            return {};
        }
        const char* start;
        if (*s == '"') {
            start = ++s;
            while (s < end && *s != '"') {
                s++;
            }
            Str arg(start, (int)(s - start));
            if (s < end) {
                s++;
            }
            return arg;
        }
        start = s;
        while (s < end && !str::IsWs(*s)) {
            s++;
        }
        return Str(start, (int)(s - start));
    };

    Str url;
    Str anchor;
    for (;;) {
        Str arg = nextArg();
        if (str::IsNull(arg)) {
            break;
        }
        if (len(arg) == 2 && arg.s[0] == '\\') {
            Str val = nextArg();
            if (arg.s[1] == 'l') {
                anchor = val;
            }
            continue;
        }
        if (str::IsNull(url)) {
            url = arg;
        }
    }
    if (len(anchor) > 0) {
        return str::JoinTemp(url, StrL("#"), anchor);
    }
    return str::DupTemp(url);
}

static u32 ReadLe32(Str d, int off) {
    const u8* p = (const u8*)d.s + off;
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void AppendLe32(str::Builder& b, u32 v) {
    for (int i = 0; i < 4; i++) {
        b.AppendChar((char)((v >> (i * 8)) & 0xff));
    }
}

// a \dibitmap is a BMP file without its 14-byte file header
static TempStr DibToBmpTemp(Str dib) {
    if (len(dib) < kBitmapInfoHeaderSize) {
        return {};
    }
    u32 hdrSize = ReadLe32(dib, 0);
    if (hdrSize < kBitmapInfoHeaderSize || hdrSize > (u32)len(dib)) {
        return {};
    }
    u32 bitCount = (u8)dib.s[14] | ((u8)dib.s[15] << 8);
    u32 compression = ReadLe32(dib, 16);
    u32 nColors = ReadLe32(dib, 32);
    if (nColors == 0 && bitCount <= 8) {
        nColors = 1u << bitCount;
    }
    u32 masks = (hdrSize == kBitmapInfoHeaderSize && compression == kBiBitfields) ? 12 : 0;
    u32 bitsOffset = kBmpFileHeaderSize + hdrSize + masks + nColors * 4;

    str::Builder b(GetTempArena());
    b.Append(StrL("BM"));
    AppendLe32(b, (u32)(kBmpFileHeaderSize + len(dib)));
    AppendLe32(b, 0);
    AppendLe32(b, bitsOffset);
    b.Append(dib);
    return b.TakeStr();
}

void RtfParser::PushGroup() {
    if (len(groups) >= kMaxGroupDepth) {
        flattenedDepth++;
        return;
    }
    VecAppend(groups, g);
    g.startsFootnote = false;
}

void RtfParser::PopGroup() {
    FlushText();
    if (flattenedDepth > 0) {
        flattenedDepth--;
        return;
    }
    if (len(groups) == 0) {
        return;
    }
    RtfGroup closing = g;
    g = groups[len(groups) - 1];
    VecRemoveLast(groups);

    if (closing.dest == RtfDest::Pict && g.dest != RtfDest::Pict) {
        EmitPicture();
    }
    if (closing.dest == RtfDest::Bookmark && g.dest != RtfDest::Bookmark) {
        EmitBookmark();
    }
    if (closing.dest == RtfDest::FontTable && g.dest != RtfDest::FontTable) {
        FinishFont();
        defaultFont = FontIndex(defaultFontNum);
    }
    if (closing.startsFootnote) {
        EndFootnote();
    }
}

// a skipped group stays skipped, whatever its sub-groups say
void RtfParser::SetDest(RtfDest d) {
    if (g.dest != RtfDest::Skip) {
        g.dest = d;
    }
}

int RtfParser::FontIndex(int num) {
    for (int i = 0; i < len(fonts); i++) {
        if (fonts[i].num == num) {
            return i;
        }
    }
    return -1;
}

uint RtfParser::CodePage() {
    int idx = g.cf.font >= 0 ? g.cf.font : defaultFont;
    if (g.dest == RtfDest::Text && idx >= 0 && fonts[idx].codePage != 0) {
        return fonts[idx].codePage;
    }
    return ansiCodePage;
}

void RtfParser::FinishFont() {
    if (!inFontEntry) {
        return;
    }
    inFontEntry = false;
    Str name = ToStr(fontName);
    str::TrimWs(name);
    curFont.nameIdx = len(name) > 0 ? len(fontNames) : -1;
    if (len(name) > 0) {
        fontNames.Append(name);
    }
    VecAppend(fonts, curFont);
    fontName.Reset();
}

void RtfParser::FinishColor() {
    int c = -1;
    if (red >= 0 || green >= 0 || blue >= 0) {
        c = (std::max(red, 0) << 16) | (std::max(green, 0) << 8) | std::max(blue, 0);
    }
    VecAppend(colors, c);
    red = green = blue = -1;
}

// plain text and \'hh bytes, decoded by FlushText()
void RtfParser::AddByte(u8 c) {
    switch (g.dest) {
        case RtfDest::Skip:
        case RtfDest::Pict:
        case RtfDest::ColorTable:
            return;
        default:
            break;
    }
    if (skipFallback > 0) {
        skipFallback--;
        return;
    }
    highSurrogate = 0;
    pendingBytes.AppendChar((char)c);
}

void RtfParser::FlushText() {
    if (len(pendingBytes) == 0) {
        return;
    }
    Str bytes = ToStr(pendingBytes);
    bool isAscii = true;
    for (int i = 0; i < len(bytes) && isAscii; i++) {
        isAscii = (u8)bytes.s[i] < 0x80;
    }
    int idx = g.cf.font >= 0 ? g.cf.font : defaultFont;
    bool isSymbol = g.dest == RtfDest::Text && idx >= 0 && fonts[idx].isSymbol;
    if (isSymbol) {
        str::Builder b(GetTempArena());
        for (int i = 0; i < len(bytes); i++) {
            char buf[8];
            int n = 0;
            str::Utf8Encode(buf, n, SymbolToCodepoint((u8)bytes.s[i]));
            b.Append(Str(buf, n));
        }
        Emit(ToStr(b));
    } else if (isAscii) {
        Emit(bytes);
    } else {
        TempStr utf8 = strconv::StrToUtf8Temp(bytes, CodePage());
        if (len(utf8) == 0) {
            utf8 = strconv::StrToUtf8Temp(bytes, kDefaultCodePage);
        }
        Emit(utf8);
    }
    pendingBytes.Reset();
}

void RtfParser::EmitCodepoint(int cp) {
    FlushText();
    char buf[8];
    int n = 0;
    str::Utf8Encode(buf, n, cp);
    Emit(Str(buf, n));
}

// \uN is a signed 16-bit UTF-16 unit; pairs make the codepoints above U+FFFF
void RtfParser::EmitUnicode(int u) {
    if (u < 0) {
        u += 0x10000;
    }
    if (u >= 0xd800 && u <= 0xdbff) {
        FlushText();
        highSurrogate = u;
        return;
    }
    if (u >= 0xdc00 && u <= 0xdfff) {
        if (highSurrogate != 0) {
            int cp = 0x10000 + ((highSurrogate - 0xd800) << 10) + (u - 0xdc00);
            highSurrogate = 0;
            EmitCodepoint(cp);
        }
        return;
    }
    highSurrogate = 0;
    EmitCodepoint(u);
}

void RtfParser::Emit(Str utf8) {
    switch (g.dest) {
        case RtfDest::Text:
            if (!g.cf.hidden) {
                WriteText(utf8);
            }
            break;
        case RtfDest::FontTable:
            fontName.Append(utf8);
            break;
        case RtfDest::Title:
            title.Append(utf8);
            break;
        case RtfDest::FieldInst:
            fieldInst.Append(utf8);
            break;
        case RtfDest::Bookmark:
            bookmark.Append(utf8);
            break;
        default:
            break;
    }
}

TempStr RtfParser::CharStyleTemp(const RtfCharFmt& cf) {
    str::Builder b(GetTempArena());
    if (cf.bold) {
        b.Append(StrL("font-weight:bold;"));
    }
    if (cf.italic) {
        b.Append(StrL("font-style:italic;"));
    }
    if (cf.underline && cf.strike) {
        b.Append(StrL("text-decoration:underline line-through;"));
    } else if (cf.underline) {
        b.Append(StrL("text-decoration:underline;"));
    } else if (cf.strike) {
        b.Append(StrL("text-decoration:line-through;"));
    }
    if (cf.smallCaps) {
        b.Append(StrL("font-variant:small-caps;"));
    }
    float size = (float)cf.fontSize / (float)kDefaultFontSize;
    if (cf.super || cf.sub) {
        b.Append(cf.super ? StrL("vertical-align:super;") : StrL("vertical-align:sub;"));
        size *= 0.7f;
    }
    if (size != 1.f) {
        b.Append(fmt("font-size:%gem;", size));
    }
    if (cf.font >= 0 && cf.font != defaultFont) {
        const RtfFont& f = fonts[cf.font];
        Str name = f.nameIdx >= 0 ? fontNames.At(f.nameIdx) : Str();
        if (len(name) > 0 && !str::ContainsChar(name, '"')) {
            b.Append(fmt("font-family:\"%s\"", name));
            if (f.family) {
                b.Append(fmt(",%s", Str(f.family)));
            }
            b.AppendChar(';');
        } else if (f.family) {
            b.Append(fmt("font-family:%s;", Str(f.family)));
        }
    }
    if (cf.color > 0 && cf.color < len(colors) && colors[cf.color] >= 0) {
        int c = colors[cf.color];
        b.Append(fmt("color:#%02x%02x%02x;", (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff));
    }
    if (cf.bgColor > 0 && cf.bgColor < len(colors) && colors[cf.bgColor] >= 0) {
        int c = colors[cf.bgColor];
        b.Append(fmt("background-color:#%02x%02x%02x;", (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff));
    }
    return b.TakeStr();
}

// keep consecutive text with the same formatting in one <span>
void RtfParser::OpenRun(RtfSink* s) {
    TempStr style = CharStyleTemp(g.cf);
    if (s->runOpen && s->runLink == g.cf.link && str::Eq(style, ToStr(s->runStyle))) {
        return;
    }
    CloseRun(s);
    if (g.cf.link >= 0) {
        s->para.Append(StrL("<a href=\""));
        AppendEscaped(s->para, links.At(g.cf.link));
        s->para.Append(StrL("\">"));
    }
    if (len(style) > 0) {
        s->para.Append(StrL("<span style=\""));
        s->para.Append(style);
        s->para.Append(StrL("\">"));
    }
    s->runOpen = true;
    s->runLink = g.cf.link;
    s->runStyle.Reset(style);
}

void RtfParser::CloseRun(RtfSink* s) {
    if (!s->runOpen) {
        return;
    }
    if (len(s->runStyle) > 0) {
        s->para.Append(StrL("</span>"));
    }
    if (s->runLink >= 0) {
        s->para.Append(StrL("</a>"));
    }
    s->runOpen = false;
}

void RtfParser::WriteText(Str utf8) {
    RtfSink* s = Sink();
    OpenRun(s);
    if (g.cf.caps) {
        TempStr up = str::DupTemp(utf8);
        for (int i = 0; i < len(up); i++) {
            if (up.s[i] >= 'a' && up.s[i] <= 'z') {
                up.s[i] = up.s[i] - 'a' + 'A';
            }
        }
        utf8 = up;
    }
    AppendEscaped(s->para, utf8);
    s->paraHasContent = true;
}

void RtfParser::WriteRaw(Str html) {
    if (g.dest != RtfDest::Text) {
        return;
    }
    RtfSink* s = Sink();
    s->para.Append(html);
    s->paraHasContent = true;
}

void RtfParser::CloseTable(RtfSink* s) {
    if (!s->tableOpen) {
        return;
    }
    s->html.Append(StrL("</table>\n"));
    s->tableOpen = false;
}

void RtfParser::EndPara(bool explicitPar) {
    if (g.dest != RtfDest::Text) {
        return;
    }
    FlushText();
    RtfSink* s = Sink();
    CloseRun(s);
    bool inCell = g.pf.inTable;
    if (!s->paraHasContent && (!explicitPar || inCell)) {
        s->para.Reset();
        return;
    }
    if (!inCell) {
        CloseTable(s);
    }
    str::Builder& out = inCell ? s->cell : s->html;

    const RtfParaFmt& pf = g.pf;
    bool isHeading = pf.outlineLevel >= 0 && pf.outlineLevel < kMaxHeadingLevel;
    TempStr tag = isHeading ? fmt("h%d", pf.outlineLevel + 1) : StrL("p");
    if (!inCell) {
        s->breakPending |= pf.pageBreakBefore;
        WritePendingBreak(s);
    }
    str::Builder style(GetTempArena());
    switch (pf.align) {
        case RtfAlign::Center:
            style.Append(StrL("text-align:center;"));
            break;
        case RtfAlign::Right:
            style.Append(StrL("text-align:right;"));
            break;
        case RtfAlign::Justify:
            style.Append(StrL("text-align:justify;"));
            break;
        default:
            break;
    }
    if (pf.leftIndent > 0) {
        style.Append(fmt("margin-left:%gpt;", TwipsToPt(pf.leftIndent)));
    }
    if (pf.rightIndent > 0) {
        style.Append(fmt("margin-right:%gpt;", TwipsToPt(pf.rightIndent)));
    }
    if (pf.firstIndent != 0) {
        style.Append(fmt("text-indent:%gpt;", TwipsToPt(pf.firstIndent)));
    }
    if (pf.spaceBefore > 0) {
        style.Append(fmt("margin-top:%gpt;", TwipsToPt(pf.spaceBefore)));
    }
    if (pf.spaceAfter > 0) {
        style.Append(fmt("margin-bottom:%gpt;", TwipsToPt(pf.spaceAfter)));
    }

    out.Append(fmt("<%s", tag));
    if (len(style) > 0) {
        out.Append(StrL(" style=\""));
        out.Append(ToStr(style));
        out.AppendChar('"');
    }
    out.AppendChar('>');
    // an empty paragraph is a blank line
    out.Append(s->paraHasContent ? ToStr(s->para) : StrL("&#160;"));
    out.Append(fmt("</%s>\n", tag));
    s->para.Reset();
    s->paraHasContent = false;
}

void RtfParser::EndCell() {
    if (g.dest != RtfDest::Text) {
        return;
    }
    bool wasInTable = g.pf.inTable;
    g.pf.inTable = true; // the cell's last paragraph has no \par
    EndPara(false);
    g.pf.inTable = wasInTable;

    RtfSink* s = Sink();
    int idx = s->cellIdx++;
    s->row.Append(StrL("<td"));
    if (idx < len(cellx)) {
        int left = idx > 0 ? cellx[idx - 1] : 0;
        int dx = cellx[idx] - left;
        if (dx > 0) {
            s->row.Append(fmt(" style=\"width:%gpt\"", TwipsToPt(dx)));
        }
    }
    if (rowBorders) {
        s->row.Append(StrL(" class=\"b\""));
    }
    s->row.AppendChar('>');
    s->row.Append(ToStr(s->cell));
    s->row.Append(StrL("</td>"));
    s->cell.Reset();
}

void RtfParser::EndRow() {
    if (g.dest != RtfDest::Text) {
        return;
    }
    RtfSink* s = Sink();
    if (s->paraHasContent || len(s->cell) > 0) {
        EndCell();
    }
    if (s->cellIdx == 0) {
        return;
    }
    if (!s->tableOpen) {
        WritePendingBreak(s);
        s->html.Append(StrL("<table>\n"));
        s->tableOpen = true;
    }
    s->html.Append(StrL("<tr>"));
    s->html.Append(ToStr(s->row));
    s->html.Append(StrL("</tr>\n"));
    s->row.Reset();
    s->cellIdx = 0;
}

void RtfParser::PageBreak() {
    if (g.dest != RtfDest::Text || g.pf.inTable) {
        return;
    }
    EndPara(false);
    RtfSink* s = Sink();
    CloseTable(s);
    s->breakPending = true;
}

// MuPDF ignores page-break-before but honors page-break-after, even on an empty block
void RtfParser::WritePendingBreak(RtfSink* s) {
    if (!s->breakPending) {
        return;
    }
    s->breakPending = false;
    // nothing before it: a break would only add a blank first page
    if (len(s->html) == 0) {
        return;
    }
    s->html.Append(StrL("<div style=\"page-break-after:always\"></div>\n"));
}

void RtfParser::FinishSink(RtfSink* s) {
    FlushText();
    CloseRun(s);
    if (s->paraHasContent) {
        EndPara(false);
    }
    if (s->cellIdx > 0 || len(s->cell) > 0) {
        EndRow();
    }
    CloseTable(s);
}

void RtfParser::StartFootnote() {
    FlushText();
    VecAppend(sinks, new RtfSink());
    g.startsFootnote = true;
}

void RtfParser::EndFootnote() {
    if (len(sinks) < 2) {
        return;
    }
    RtfSink* s = Sink();
    // g is the group around the footnote again: format its last paragraph
    // as text, whatever that group's destination
    RtfDest dest = g.dest;
    g.dest = RtfDest::Text;
    FinishSink(s);
    g.dest = dest;
    footnotes.Append(ToStr(s->html));
    VecRemoveLast(sinks);
    delete s;
}

void RtfParser::EmitPicture() {
    Str data = ToStr(picData);
    Str mime = StrL("image/png");
    switch (picKind) {
        case RtfPicKind::Jpeg:
            mime = StrL("image/jpeg");
            break;
        case RtfPicKind::Png:
            break;
        case RtfPicKind::Dib:
            // MuPDF reads images by their content, whatever the data URI says
            data = DibToBmpTemp(data);
            break;
        default:
            data = {};
            break;
    }
    if (len(data) == 0) {
        nSkippedPics++;
        return;
    }

    float w = 0;
    if (picwGoal > 0) {
        w = TwipsToPt(picwGoal);
    } else if (picw > 0) {
        w = (float)picw * kPtPerPx;
    }
    w = w * (float)picScaleX / 100.f;

    str::Builder img(GetTempArena());
    img.Append(fmt("<img src=\"data:%s;base64,", mime));
    str::AppendBase64(img, (const u8*)data.s, (size_t)len(data));
    picData.Reset(); // data may point into it
    img.AppendChar('"');
    if (w > 0) {
        img.Append(fmt(" style=\"width:%gpt\"", w));
    }
    img.Append(StrL("/>"));
    WriteRaw(ToStr(img));
}

void RtfParser::EmitBookmark() {
    Str name = ToStr(bookmark);
    str::TrimWs(name);
    if (len(name) == 0 || g.dest != RtfDest::Text) {
        return;
    }
    str::Builder a(GetTempArena());
    a.Append(StrL("<a id=\""));
    AppendEscaped(a, name);
    a.Append(StrL("\"></a>"));
    RtfSink* s = Sink();
    s->para.Append(ToStr(a));
    bookmark.Reset();
}

void RtfParser::Keyword(RtfKw kw, bool hasParam, int param) {
    bool on = !hasParam || param != 0;
    RtfCharFmt& cf = g.cf;
    RtfParaFmt& pf = g.pf;
    bool inFontTable = g.dest == RtfDest::FontTable;
    switch (kw) {
        // document
        case RtfKw::Ansi:
            ansiCodePage = kDefaultCodePage;
            break;
        case RtfKw::Ansicpg:
            if (param > 0) {
                ansiCodePage = (uint)param;
            }
            break;
        case RtfKw::Mac:
            ansiCodePage = 10000;
            break;
        case RtfKw::Pc:
            ansiCodePage = 437;
            break;
        case RtfKw::Pca:
            ansiCodePage = 850;
            break;
        case RtfKw::Deff:
            defaultFontNum = param;
            break;

        // destinations
        case RtfKw::Fonttbl:
            SetDest(RtfDest::FontTable);
            break;
        case RtfKw::Colortbl:
            SetDest(RtfDest::ColorTable);
            red = green = blue = -1;
            break;
        case RtfKw::Info:
            SetDest(RtfDest::Info);
            break;
        case RtfKw::Title:
            if (g.dest == RtfDest::Info) {
                g.dest = RtfDest::Title;
                title.Reset();
            }
            break;
        case RtfKw::Author:
        case RtfKw::Subject:
            if (g.dest == RtfDest::Info) {
                g.dest = RtfDest::Skip;
            }
            break;
        case RtfKw::Stylesheet:
        case RtfKw::Header:
        case RtfKw::Headerf:
        case RtfKw::Headerl:
        case RtfKw::Headerr:
        case RtfKw::Footer:
        case RtfKw::Footerf:
        case RtfKw::Footerl:
        case RtfKw::Footerr:
        case RtfKw::Revtbl:
        case RtfKw::Xe:
        case RtfKw::Tc:
        case RtfKw::Nonshppict:
        case RtfKw::Do:
            SetDest(RtfDest::Skip);
            break;
        case RtfKw::Upr:
            g.uprParentDest = g.dest;
            SetDest(RtfDest::Skip);
            break;
        case RtfKw::Ud:
            g.dest = g.uprParentDest;
            break;
        case RtfKw::Shppict:
            break;
        case RtfKw::Field:
            if (g.dest == RtfDest::Text) {
                fieldInst.Reset();
            }
            break;
        case RtfKw::Fldinst:
            SetDest(RtfDest::FieldInst);
            break;
        case RtfKw::Fldrslt:
            if (g.dest != RtfDest::Skip) {
                g.dest = RtfDest::Text;
                TempStr target = HyperlinkTargetTemp(ToStr(fieldInst));
                if (len(target) > 0) {
                    cf.link = len(links);
                    links.Append(target);
                }
            }
            break;
        case RtfKw::Bkmkstart:
            SetDest(RtfDest::Bookmark);
            bookmark.Reset();
            break;
        case RtfKw::Footnote:
            if (g.dest == RtfDest::Text) {
                StartFootnote();
            }
            break;
        case RtfKw::Chftn:
            if (g.dest == RtfDest::Text) {
                if (len(sinks) == 1) {
                    footnoteNo++;
                }
                Emit(fmt("%d", footnoteNo));
            }
            break;

        // fonts and colors
        case RtfKw::F:
            if (inFontTable) {
                FinishFont();
                curFont = RtfFont{};
                curFont.num = param;
                inFontEntry = true;
            } else {
                cf.font = FontIndex(param);
            }
            break;
        case RtfKw::Fcharset:
            if (inFontTable && inFontEntry) {
                curFont.codePage = CharsetToCodePage(param);
                curFont.isSymbol = param == 2;
            }
            break;
        case RtfKw::Cpg:
            if (inFontTable && inFontEntry && param > 0) {
                curFont.codePage = (uint)param;
            }
            break;
        case RtfKw::Froman:
            curFont.family = "serif";
            break;
        case RtfKw::Fswiss:
            curFont.family = "sans-serif";
            break;
        case RtfKw::Fmodern:
            curFont.family = "monospace";
            break;
        case RtfKw::Fscript:
            curFont.family = "cursive";
            break;
        case RtfKw::Fdecor:
            curFont.family = "fantasy";
            break;
        case RtfKw::Ftech:
        case RtfKw::Fnil:
            break;
        case RtfKw::Red:
            red = param;
            break;
        case RtfKw::Green:
            green = param;
            break;
        case RtfKw::Blue:
            blue = param;
            break;

        // character formatting
        case RtfKw::B:
            cf.bold = on;
            break;
        case RtfKw::I:
            cf.italic = on;
            break;
        case RtfKw::Ul:
        case RtfKw::Uld:
        case RtfKw::Uldash:
        case RtfKw::Uldashd:
        case RtfKw::Uldb:
        case RtfKw::Ulhwave:
        case RtfKw::Ulth:
        case RtfKw::Ulw:
        case RtfKw::Ulwave:
            cf.underline = on;
            break;
        case RtfKw::Ulnone:
            cf.underline = false;
            break;
        case RtfKw::Strike:
        case RtfKw::Striked:
            cf.strike = on;
            break;
        case RtfKw::Caps:
            cf.caps = on;
            break;
        case RtfKw::Scaps:
            cf.smallCaps = on;
            break;
        case RtfKw::V:
        case RtfKw::Deleted:
            cf.hidden = on;
            break;
        case RtfKw::Super:
            cf.super = true;
            cf.sub = false;
            break;
        case RtfKw::Sub:
            cf.sub = true;
            cf.super = false;
            break;
        case RtfKw::Nosupersub:
            cf.super = cf.sub = false;
            break;
        case RtfKw::Fs:
            cf.fontSize = hasParam && param > 0 ? param : kDefaultFontSize;
            break;
        case RtfKw::Cf:
            cf.color = param;
            break;
        case RtfKw::Cb:
        case RtfKw::Highlight:
            cf.bgColor = param;
            break;
        case RtfKw::Plain: {
            int link = cf.link;
            cf = RtfCharFmt{};
            cf.link = link;
            break;
        }

        // paragraph formatting
        case RtfKw::Pard:
            pf = RtfParaFmt{};
            break;
        case RtfKw::Intbl:
            pf.inTable = true;
            break;
        case RtfKw::Ql:
            pf.align = RtfAlign::Left;
            break;
        case RtfKw::Qc:
            pf.align = RtfAlign::Center;
            break;
        case RtfKw::Qr:
            pf.align = RtfAlign::Right;
            break;
        case RtfKw::Qj:
            pf.align = RtfAlign::Justify;
            break;
        case RtfKw::Li:
            pf.leftIndent = param;
            break;
        case RtfKw::Ri:
            pf.rightIndent = param;
            break;
        case RtfKw::Fi:
            pf.firstIndent = param;
            break;
        case RtfKw::Sb:
            pf.spaceBefore = param;
            break;
        case RtfKw::Sa:
            pf.spaceAfter = param;
            break;
        case RtfKw::Outlinelevel:
            pf.outlineLevel = param >= 0 && param < kMaxHeadingLevel ? param : -1;
            break;
        case RtfKw::Pagebb:
            pf.pageBreakBefore = on;
            break;

        // breaks
        case RtfKw::Par:
            EndPara(true);
            break;
        case RtfKw::Sect:
            // the next section starts on a new page unless it says \sbknone or \sbkcol
            PageBreak();
            break;
        case RtfKw::Sbkcol:
        case RtfKw::Sbknone:
            if (g.dest == RtfDest::Text) {
                Sink()->breakPending = false;
            }
            break;
        case RtfKw::Line:
            WriteRaw(StrL("<br/>"));
            break;
        case RtfKw::Page:
            PageBreak();
            break;

        // tables
        case RtfKw::Trowd:
            VecReset(cellx);
            rowBorders = false;
            borderPending = false;
            break;
        case RtfKw::Cellx:
            VecAppend(cellx, param);
            break;
        case RtfKw::Cell:
            EndCell();
            break;
        case RtfKw::Row:
            EndRow();
            break;
        case RtfKw::Nestcell:
            Emit(StrL(" "));
            break;
        case RtfKw::Nestrow:
            WriteRaw(StrL("<br/>"));
            break;
        case RtfKw::Clbrdrb:
        case RtfKw::Clbrdrl:
        case RtfKw::Clbrdrr:
        case RtfKw::Clbrdrt:
            borderPending = true;
            break;
        case RtfKw::Brdrs:
        case RtfKw::Brdrth:
        case RtfKw::Brdrsh:
        case RtfKw::Brdrdb:
        case RtfKw::Brdrdot:
        case RtfKw::Brdrdash:
            rowBorders |= borderPending;
            borderPending = false;
            break;
        case RtfKw::Brdrnone:
        case RtfKw::Brdrnil:
            borderPending = false;
            break;

        // pictures
        case RtfKw::Pict:
            SetDest(RtfDest::Pict);
            if (g.dest == RtfDest::Pict) {
                picKind = RtfPicKind::Unknown;
                picw = 0;
                picwGoal = 0;
                picScaleX = 100;
                picData.Reset();
                picNibble = -1;
            }
            break;
        case RtfKw::Pngblip:
            picKind = RtfPicKind::Png;
            break;
        case RtfKw::Jpegblip:
            picKind = RtfPicKind::Jpeg;
            break;
        case RtfKw::Dibitmap:
            picKind = RtfPicKind::Dib;
            break;
        case RtfKw::Emfblip:
        case RtfKw::Wmetafile:
        case RtfKw::Wbitmap:
            picKind = RtfPicKind::Unsupported;
            break;
        case RtfKw::Picw:
            picw = param;
            break;
        case RtfKw::Picwgoal:
            picwGoal = param;
            break;
        case RtfKw::Picscalex:
            picScaleX = param > 0 ? param : 100;
            break;
        case RtfKw::Pich:
        case RtfKw::Pichgoal:
        case RtfKw::Picscaley:
            // the height follows the width (img { height: auto } in the user CSS)
            break;
        case RtfKw::Bin: {
            int n = std::max(param, 0);
            n = std::min(n, len(src) - pos);
            if (g.dest == RtfDest::Pict) {
                picData.Append(Str(src.s + pos, n));
            }
            pos += n;
            break;
        }

        // special characters
        case RtfKw::Tab:
            EmitCodepoint(0x2003); // em space
            break;
        case RtfKw::Bullet:
            EmitCodepoint(0x2022);
            break;
        case RtfKw::Emdash:
            EmitCodepoint(0x2014);
            break;
        case RtfKw::Endash:
            EmitCodepoint(0x2013);
            break;
        case RtfKw::Emspace:
            EmitCodepoint(0x2003);
            break;
        case RtfKw::Enspace:
            EmitCodepoint(0x2002);
            break;
        case RtfKw::Lquote:
            EmitCodepoint(0x2018);
            break;
        case RtfKw::Rquote:
            EmitCodepoint(0x2019);
            break;
        case RtfKw::Ldblquote:
            EmitCodepoint(0x201c);
            break;
        case RtfKw::Rdblquote:
            EmitCodepoint(0x201d);
            break;
        case RtfKw::Zwj:
            EmitCodepoint(0x200d);
            break;
        case RtfKw::Zwnj:
            EmitCodepoint(0x200c);
            break;
        case RtfKw::U:
            EmitUnicode(param);
            skipFallback = g.uc;
            break;
        case RtfKw::Uc:
            g.uc = std::max(param, 0);
            break;
    }
}

// after a backslash: a control word (\b0, \par) or a control symbol (\'e9, \~)
void RtfParser::ParseControl() {
    if (pos >= len(src)) {
        return;
    }
    char c = src.s[pos];
    if (!IsAsciiLetter(c)) {
        pos++;
        switch (c) {
            case '\'': {
                int hi = pos < len(src) ? HexVal(src.s[pos]) : -1;
                int lo = pos + 1 < len(src) ? HexVal(src.s[pos + 1]) : -1;
                if (hi >= 0 && lo >= 0) {
                    pos += 2;
                    AddByte((u8)(hi * 16 + lo));
                }
                break;
            }
            case '*':
                starPending = true;
                break;
            case '~':
                EmitCodepoint(0xa0);
                break;
            case '_':
                EmitCodepoint(0x2011);
                break;
            case '{':
            case '}':
            case '\\':
                AddByte((u8)c);
                break;
            case '\r':
            case '\n':
                FlushText();
                EndPara(true);
                break;
            default:
                // \- optional hyphen, \| and \: formula / index characters
                break;
        }
        return;
    }

    constexpr int kMaxKeywordLen = 32;
    constexpr int kMaxParamDigits = 10;
    int start = pos;
    while (pos < len(src) && IsAsciiLetter(src.s[pos]) && pos - start < kMaxKeywordLen) {
        pos++;
    }
    Str name(src.s + start, pos - start);
    bool hasParam = false;
    bool negative = false;
    i64 param = 0;
    if (pos < len(src) && src.s[pos] == '-') {
        negative = true;
        pos++;
    }
    int digitsStart = pos;
    while (pos < len(src) && src.s[pos] >= '0' && src.s[pos] <= '9') {
        if (pos - digitsStart < kMaxParamDigits) {
            param = param * 10 + (src.s[pos] - '0');
        }
        pos++;
    }
    hasParam = pos > digitsStart;
    param = std::min(param, (i64)INT_MAX);
    if (negative) {
        param = -param;
    }
    if (pos < len(src) && src.s[pos] == ' ') {
        pos++;
    }

    bool star = starPending;
    starPending = false;
    int idx = FindRtfKeyword(name);
    if (idx < 0) {
        // an unknown destination marked as ignorable
        if (star) {
            FlushText();
            SetDest(RtfDest::Skip);
        }
        return;
    }
    FlushText();
    Keyword((RtfKw)idx, hasParam, (int)param);
}

void RtfParser::Parse() {
    VecAppend(sinks, new RtfSink());
    while (pos < len(src)) {
        char c = src.s[pos++];
        switch (c) {
            case '{':
                FlushText();
                PushGroup();
                skipFallback = 0;
                break;
            case '}':
                PopGroup();
                skipFallback = 0;
                break;
            case '\\':
                ParseControl();
                break;
            case '\r':
            case '\n':
                break;
            case '\t':
                FlushText();
                EmitCodepoint(0x2003);
                break;
            default:
                if (g.dest == RtfDest::Pict) {
                    int v = HexVal(c);
                    if (v < 0) {
                        break;
                    }
                    if (picNibble < 0) {
                        picNibble = v;
                    } else {
                        picData.AppendChar((char)(picNibble * 16 + v));
                        picNibble = -1;
                    }
                    break;
                }
                if (c == ';' && g.dest == RtfDest::ColorTable) {
                    FinishColor();
                    break;
                }
                if (c == ';' && g.dest == RtfDest::FontTable) {
                    FlushText();
                    FinishFont();
                    break;
                }
                if ((u8)c >= 0x20) {
                    AddByte((u8)c);
                }
                break;
        }
    }
    FlushText();
    // unterminated groups: unwind so footnotes and pictures are finished
    while (len(groups) > 0 || flattenedDepth > 0) {
        PopGroup();
    }
    while (len(sinks) > 1) {
        EndFootnote();
    }
    FinishSink(sinks[0]);
}

Str RtfParser::BuildHtml() {
    str::Builder out;
    out.Append(StrL("<html><head><meta charset=\"utf-8\"/>"));
    Str t = ToStr(title);
    str::TrimWs(t);
    if (len(t) > 0) {
        out.Append(StrL("<title>"));
        AppendEscaped(out, t);
        out.Append(StrL("</title>"));
    }
    out.Append(
        StrL("<style>\n"
             "p,h1,h2,h3,h4,h5,h6{margin:0}\n"
             "h1,h2,h3,h4,h5,h6{font-size:1em;font-weight:normal}\n"
             "table{border-collapse:collapse}\n"
             "td{vertical-align:top;padding:0 0.3em}\n"
             "td.b{border:1px solid black}\n"
             "</style></head>\n"));

    out.Append(StrL("<body"));
    if (defaultFont >= 0) {
        const RtfFont& f = fonts[defaultFont];
        Str name = f.nameIdx >= 0 ? fontNames.At(f.nameIdx) : Str();
        if (len(name) > 0 && !str::ContainsCharAny(name, StrL("\"'"))) {
            out.Append(StrL(" style=\"font-family:'"));
            AppendEscaped(out, name);
            out.AppendChar('\'');
            if (f.family) {
                out.Append(fmt(",%s", Str(f.family)));
            }
            out.AppendChar('"');
        }
    }
    out.Append(StrL(">\n"));
    out.Append(ToStr(sinks[0]->html));
    if (len(footnotes) > 0) {
        out.Append(StrL("<hr/>\n"));
        for (Str fn : footnotes) {
            out.Append(fn);
        }
    }
    out.Append(StrL("</body></html>\n"));
    if (nSkippedPics > 0) {
        logf("RtfToHtml: skipped %d pictures in formats we don't convert (EMF, WMF, DDB)\n", nSkippedPics);
    }
    return out.TakeStr();
}

// RTF document -> html for MuPDF's HTML engine; {} if it isn't RTF.
// Caller frees the result.
Str RtfToHtml(Str rtf) {
    if (!str::StartsWith(rtf, StrL("{\\rtf"))) {
        return {};
    }
    RtfParser p(rtf);
    p.Parse();
    return p.BuildHtml();
}

EngineBase* CreateEngineRtfFromFile(Str path, PasswordUI* pwdUI) {
    Str rtf = file::ReadFileWithArena(path, GetTempArena());
    Str html = RtfToHtml(rtf);
    if (str::IsNull(html)) {
        return nullptr;
    }
    EngineBase* engine = CreateEngineMupdfFromData(html, StrL("document.html"), pwdUI);
    str::Free(html);
    if (engine) {
        engine->SetFilePath(path);
        str::ReplaceWithCopy(&engine->defaultExt, StrL(".rtf"));
    }
    return engine;
}

#if IS_DEBUG
bool RtfDoc_UnitTestKeywords() {
    if (dimofi(gRtfKeywords) != (int)RtfKw::Zwnj + 1) {
        return false;
    }
    for (int i = 1; i < dimofi(gRtfKeywords); i++) {
        if (strcmp(gRtfKeywords[i - 1], gRtfKeywords[i]) >= 0) {
            return false;
        }
    }
    for (int i = 0; i < dimofi(gRtfKeywords); i++) {
        if (FindRtfKeyword(Str(gRtfKeywords[i])) != i) {
            return false;
        }
    }
    return FindRtfKeyword(StrL("rtf")) < 0 && FindRtfKeyword(StrL("zzz")) < 0 && FindRtfKeyword(StrL("a")) < 0;
}
#endif
