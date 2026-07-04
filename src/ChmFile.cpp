/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm_lib.h>
#include "base/ByteReader.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "GumboHelpers.h"

#include "DocProperties.h"
#include "EbookBase.h"
#include "ChmFile.h"

ChmFile::~ChmFile() {
    chm_close(chmHandle);
    str::Free(title);
    str::Free(tocPath);
    str::Free(indexPath);
    str::Free(homePath);
    str::Free(creator);
    str::Free(data);
}

// Resolve a CHM object by its path, normalizing the leading slash and
// tolerating backslashes in URLs the way Microsoft's HTML Help viewer does.
// Returns true and fills `info` on success.
static bool ChmResolveObject(struct chmFile* chmHandle, Str fileName, struct chmUnitInfo* info) {
    if (!fileName) {
        return false;
    }
    if (!str::StartsWith(fileName, "/")) {
        fileName = str::JoinTemp(StrL("/"), fileName);
    } else if (str::StartsWith(fileName, "///")) {
        fileName = Str(fileName.s + 2, fileName.len - 2);
    }

    int res = chm_resolve_object(chmHandle, fileName.s, info);
    if (CHM_RESOLVE_SUCCESS != res && str::ContainsChar(fileName, '\\')) {
        TempStr fileNameTemp = str::DupTemp(fileName);
        str::TransCharsInPlace(fileNameTemp, StrL("\\"), StrL("/"));
        res = chm_resolve_object(chmHandle, fileNameTemp.s, info);
    }
    return CHM_RESOLVE_SUCCESS == res;
}

bool ChmFile::HasData(Str fileName) const {
    struct chmUnitInfo info{};
    return ChmResolveObject(chmHandle, fileName, &info);
}

TempStr ChmFile::GetDataTemp(Str fileName) const {
    struct chmUnitInfo info{};
    if (!ChmResolveObject(chmHandle, fileName, &info)) {
        return {};
    }
    size_t n = (size_t)info.length;
    if (n > 128 * 1024 * 1024) {
        // limit to 128 MB
        return {};
    }

    // +1 for 0 terminator for C string compatibility
    u8* d = AllocArrayTemp<u8>((int)(n + 1));
    if (!d) {
        return {};
    }
    if (!chm_retrieve_object(chmHandle, &info, d, 0, n)) {
        return {};
    }

    return Str((char*)(d), (int)(n));
}

TempStr SmartToUtf8Temp(Str s, uint codepage) {
    if (str::StartsWith(s, UTF8_BOM)) {
        return str::DupTemp(Str(s.s + 3, s.len - 3));
    }
    if (CP_UTF8 == codepage) {
        return str::DupTemp(s);
    }
    return strconv::ToMultiByteTemp(s, codepage, CP_UTF8);
}

static Str GetCharZ(Str d, size_t off) {
    u8* data = (u8*)d.s;
    size_t n = (size_t)d.len;
    if (off >= n) {
        return {};
    }
    ReportIf(!memchr(data + off, '\0', n - off + 1)); // data is zero-terminated
    u8* str = data + off;
    Str s = Str((char*)str);
    if (str::IsEmpty(s)) {
        return {};
    }
    return str::Dup(s);
}

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
void ChmFile::ParseWindowsData() {
    TempStr windowsData = GetDataTemp("/#WINDOWS");
    TempStr stringsData = GetDataTemp("/#STRINGS");

    if (len(windowsData) == 0 || len(stringsData) == 0) {
        return;
    }
    size_t windowsLen = (size_t)windowsData.len;
    if (windowsLen <= 8) {
        return;
    }

    ByteReader rw(windowsData);
    size_t entries = rw.DWordLE(0);
    size_t entrySize = rw.DWordLE(4);
    if (entrySize < 188) {
        return;
    }

    for (size_t i = 0; i < entries && ((i + (size_t)1) * entrySize) <= windowsLen; i++) {
        size_t off = 8 + i * entrySize;
        if (str::IsNull(title)) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x14);
            title = GetCharZ(stringsData, strOff);
        }
        if (str::IsNull(tocPath)) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x60);
            tocPath = GetCharZ(stringsData, strOff);
        }
        if (str::IsNull(indexPath)) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x64);
            indexPath = GetCharZ(stringsData, strOff);
        }
        if (str::IsNull(homePath)) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x68);
            homePath = GetCharZ(stringsData, strOff);
        }
    }
}

#define CP_CHM_DEFAULT 1252

static uint LcidToCodepage(DWORD lcid) {
    // cf. http://msdn.microsoft.com/en-us/library/bb165625(v=VS.90).aspx
    // flat array of (lcid, codepage) pairs
    static const u16 lcidToCodepage[] = {
        1025,
        1256, //
        2052,
        936, //
        1028,
        950, //
        1029,
        1250, //
        1032,
        1253, //
        1037,
        1255, //
        1038,
        1250, //
        1041,
        932, //
        1042,
        949, //
        1045,
        1250, //
        1049,
        1251, //
        1051,
        1250, //
        1060,
        1250, //
        1055,
        1254, //
        1026,
        1251, //
        4,
        936, //
        // more Cyrillic (1251) locales: Ukrainian, Belarusian, Serbian (Cyrillic),
        // Macedonian, Kazakh, Kyrgyz, Tatar, Mongolian, Azeri (Cyrillic)
        1058,
        1251, //
        1059,
        1251, //
        3098,
        1251, //
        2074,
        1251, //
        1071,
        1251, //
        1087,
        1251, //
        1088,
        1251, //
        1092,
        1251, //
        1104,
        1251, //
        2092,
        1251, //
    };

    for (int i = 0; i < dimofi(lcidToCodepage); i += 2) {
        if (lcid == lcidToCodepage[i]) {
            return lcidToCodepage[i + 1];
        }
    }

    return CP_CHM_DEFAULT;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#SYSTEM
bool ChmFile::ParseSystemData() {
    TempStr d = GetDataTemp("/#SYSTEM");
    if (str::IsEmpty(d)) {
        return false;
    }

    ByteReader r(d);
    DWORD n = 0;
    // Note: skipping DWORD version at offset 0. It's supposed to be 2 or 3.
    for (size_t off = 4; off + 4 < (size_t)d.len; off += n + (size_t)4) {
        // Note: at some point we seem to get off-sync i.e. I'm seeing
        // many entries with type == 0 and length == 0. Seems harmless.
        n = r.WordLE(off + 2);
        if (n == 0) {
            continue;
        }
        WORD type = r.WordLE(off);
        switch (type) {
            case 0:
                if (str::IsNull(tocPath)) {
                    tocPath = GetCharZ(d, off + 4);
                }
                break;
            case 1:
                if (str::IsNull(indexPath)) {
                    indexPath = GetCharZ(d, off + 4);
                }
                break;
            case 2:
                if (str::IsNull(homePath)) {
                    homePath = GetCharZ(d, off + 4);
                }
                break;
            case 3:
                if (str::IsNull(title)) {
                    title = GetCharZ(d, off + 4);
                }
                break;
            case 4:
                if (!codepage && n >= 4) {
                    codepage = LcidToCodepage(r.DWordLE(off + 4));
                }
                break;
            case 6:
                // compiled file - ignore
                break;
            case 9:
                if (str::IsNull(creator)) {
                    creator = GetCharZ(d, off + 4);
                }
                break;
            case 16:
                // default font - ignore
                break;
        }
    }

    return true;
}

TempStr ChmFile::ResolveTopicID(unsigned int id) const {
    TempStr ivbData = GetDataTemp("/#IVB");
    size_t ivbLen = (size_t)ivbData.len;
    ByteReader br(ivbData);
    if ((ivbLen % 8) != 4 || ivbLen - 4 != br.DWordLE(0)) {
        return {};
    }

    for (size_t off = 4; off < ivbLen; off += 8) {
        if (br.DWordLE(off) == id) {
            TempStr stringsData = GetDataTemp("/#STRINGS");
            Str res = GetCharZ(stringsData, br.DWordLE(off + 4));
            if (!res) {
                return {};
            }
            return str::DupTemp(res);
        }
    }
    return {};
}

void ChmFile::FixPathCodepage(Str& path, uint& fileCP) {
    if (str::IsEmpty(path) || HasData(path)) {
        return;
    }

    TempStr utf8Path = SmartToUtf8Temp(path, codepage);
    if (HasData(utf8Path)) {
        str::ReplaceWithCopy(&path, utf8Path);
        fileCP = codepage;
        return;
    }

    if (fileCP == codepage) {
        return;
    }

    utf8Path = SmartToUtf8Temp(path, fileCP);
    if (HasData(utf8Path)) {
        str::ReplaceWithCopy(&path, utf8Path);
        codepage = fileCP;
        return;
    }
}

bool ChmFile::Load(Str path) {
    data = file::ReadFile(path);
    chmHandle = chm_open(data.s, (size_t)data.len);
    if (!chmHandle) {
        return false;
    }

    ParseWindowsData();
    if (!ParseSystemData()) {
        return false;
    }

    uint fileCodepage = codepage;
    char header[24]{};
    int n = file::ReadN(path, (u8*)header, sizeof(header));
    if (n < (int)sizeof(header)) {
        ByteReader r(Str(header, (int)sizeof(header)));
        DWORD lcid = r.DWordLE(20);
        fileCodepage = LcidToCodepage(lcid);
    }
    if (!codepage) {
        codepage = fileCodepage;
    }
    // if file and #SYSTEM codepage disagree, prefer #SYSTEM's (unless it leads to wrong paths)
    FixPathCodepage(homePath, fileCodepage);
    FixPathCodepage(tocPath, fileCodepage);
    FixPathCodepage(indexPath, fileCodepage);
    if (GetACP() == codepage) {
        codepage = CP_ACP;
    }

    if (!HasData(homePath)) {
        Str pathsToTest[] = {"/index.htm", "/index.html", "/default.htm", "/default.html"};
        for (int i = 0; i < dimof(pathsToTest); i++) {
            if (HasData(pathsToTest[i])) {
                str::ReplaceWithCopy(&homePath, pathsToTest[i]);
            }
        }
        if (!HasData(homePath)) {
            return false;
        }
    }

    return true;
}

TempStr ChmFile::GetPropertyTemp(Str name) const {
    TempStr result;
    if (str::Eq(kPropTitle, name) && !str::IsEmpty(title)) {
        result = SmartToUtf8Temp(title, codepage);
    } else if (str::Eq(kPropCreatorApp, name) && !str::IsEmpty(creator)) {
        result = SmartToUtf8Temp(creator, codepage);
    }
    if (!result) {
        return {};
    }
    str::NormalizeWSInPlace(result);
    return result;
}

TempStr ChmFile::GetHomePath() const {
    return homePath;
}

static int ChmEnumerateEntry(struct chmFile* chmHandle, struct chmUnitInfo* info, void* data) {
    if (str::IsEmpty(info->path)) {
        return CHM_ENUMERATOR_CONTINUE;
    }
    StrVec* paths = (StrVec*)data;
    paths->Append(info->path);
    return CHM_ENUMERATOR_CONTINUE;
}

void ChmFile::GetAllPaths(StrVec* v) const {
    chm_enumerate(chmHandle, CHM_ENUMERATE_FILES | CHM_ENUMERATE_NORMAL, ChmEnumerateEntry, v);
}

/* The html looks like:
<li>
  <object type="text/sitemap">
    <param name="Name" value="Main Page">
    <param name="Local" value="0789729717_main.html">
    <param name="ImageNumber" value="12">
  </object>
  <ul> ... children ... </ul>
<li>
  ... siblings ...
*/
// Strip the "ITS protocol" prefix from a CHM URL, e.g.
// "mk:@MSITStore:foo.chm::/index.html" -> "index.html".
static Str StripItsProtocol(Str url) {
    Str p;
    str::Cut(url, StrL("::/"), nullptr, &p);
    return p ? p : url;
}

static bool VisitChmTocItem(EbookTocVisitor* visitor, const GumboNode* objNode, int level) {
    ReportIf(!GumboTagNameIs(objNode, "object"));

    TempStr name, local;
    const GumboVector* children = &objNode->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (!GumboTagNameIs(child, "param")) {
            continue;
        }
        const GumboAttribute* attrName = gumbo_get_attribute(&child->v.element.attributes, "name");
        const GumboAttribute* attrVal = gumbo_get_attribute(&child->v.element.attributes, "value");
        if (!attrName || !attrVal) {
            continue;
        }
        if (str::EqI(attrName->value, "Name")) {
            name = str::DupTemp(attrVal->value);
        } else if (str::EqI(attrName->value, "Local")) {
            local = str::DupTemp(StripItsProtocol(Str(attrVal->value)));
        }
    }
    if (!name) {
        return false;
    }
    visitor->Visit(name, local, level);
    return true;
}

/* The html looks like:
<li>
  <object type="text/sitemap">
    <param name="Keyword" value="- operator">
    <param name="Name" value="Subtraction Operator (-)">
    <param name="Local" value="html/vsoprsubtract.htm">
    <param name="Name" value="Subtraction Operator (-)">
    <param name="Local" value="html/js56jsoprsubtract.htm">
  </object>
  <ul> ... optional children ... </ul>
<li>
  ... siblings ...
*/
static bool VisitChmIndexItem(EbookTocVisitor* visitor, const GumboNode* objNode, int level) {
    ReportIf(!GumboTagNameIs(objNode, "object"));

    StrVec references;
    Str keyword;
    Str name;
    const GumboVector* children = &objNode->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (!GumboTagNameIs(child, "param")) {
            continue;
        }
        const GumboAttribute* attrName = gumbo_get_attribute(&child->v.element.attributes, "name");
        const GumboAttribute* attrVal = gumbo_get_attribute(&child->v.element.attributes, "value");
        if (!attrName || !attrVal) {
            continue;
        }
        if (str::EqI(attrName->value, "Keyword")) {
            keyword = Str(attrVal->value);
        } else if (str::EqI(attrName->value, "Name")) {
            name = Str(attrVal->value);
            // some CHM documents seem to use a lonely Name instead of Keyword
            if (!keyword) {
                keyword = name;
            }
        } else if (str::EqI(attrName->value, "Local") && name) {
            references.Append(name);
            references.Append(StripItsProtocol(Str(attrVal->value)));
        }
    }
    if (!keyword) {
        return false;
    }

    if (len(references) == 2) {
        visitor->Visit(keyword, references.At(1), level);
        return true;
    }
    visitor->Visit(keyword, {}, level);
    int n = len(references);
    for (int i = 0; i < n; i += 2) {
        visitor->Visit(references[i], references[i + 1], level + 1);
    }
    return true;
}

// Process a single <ul>'s <li> children as TOC entries at `level`.
// A nested <ul> holds the children of the preceding <li>, so it's always walked
// at level + 1. It can appear either inside the <li> (well-formed ToCs) or as a
// <ul> sibling among the <li>s -- gumbo's HTML5 repair leaves the latter as a
// bare child of the parent <ul> when the <li> was explicitly closed, which some
// broken CHM ToCs do. Walking every such <ul> at level + 1 reproduces the
// nesting the previous, pre-gumbo parser produced (and walks each <ul> once, so
// no entries are duplicated).
// One suspended <ul> walk: `i` is the next child of `ul` to process at `level`.
struct ChmUlFrame {
    const GumboNode* ul;
    int level;
    unsigned int i;
};

static void WalkChmUl(EbookTocVisitor* visitor, const GumboNode* ulNode, bool isIndex, int level) {
    if (!ulNode) {
        return;
    }
    // Iterative version of the recursive <ul>/<li> walk: each stack frame holds
    // a <ul> and how far we've scanned its children, so a pathologically deep
    // ToC nesting can't overflow the call stack. Order and levels match the
    // recursive walk exactly (parent frame resumes after its child completes).
    Vec<ChmUlFrame> stack;
    stack.Append({ulNode, level, 0});
    while (len(stack) > 0) {
        ChmUlFrame& top = stack.Last();
        const GumboVector* lis = &top.ul->v.element.children;
        if (top.i >= lis->length) {
            stack.RemoveLast();
            continue;
        }
        const GumboNode* child = (const GumboNode*)lis->data[top.i];
        int lvl = top.level;
        top.i++;
        // any stack.Append() below may reallocate -> don't touch `top` after this

        if (GumboTagNameIs(child, "ul")) {
            // a bare <ul> among the <li>s holds the children of the preceding <li>
            stack.Append({child, lvl + 1, 0});
            continue;
        }
        const GumboNode* li = child;
        if (!GumboTagNameIs(li, "li")) {
            continue; // skip whitespace / text / unexpected nodes
        }
        const GumboNode* objNode = GumboFindChildByTag(li, "object");
        if (!objNode) {
            continue;
        }
        bool valid = isIndex ? VisitChmIndexItem(visitor, objNode, lvl) : VisitChmTocItem(visitor, objNode, lvl);
        if (!valid) {
            continue;
        }
        const GumboNode* nested = GumboFindChildByTag(li, "ul");
        if (nested) {
            stack.Append({nested, lvl + 1, 0});
        }
    }
}

// Process `firstUl` and any consecutive <ul> siblings (some broken ToCs wrap
// each <li> in its own <ul>, producing a run of sibling <ul>s).
static void WalkChmTocOrIndex(EbookTocVisitor* visitor, const GumboNode* firstUl, bool isIndex) {
    if (!firstUl || !firstUl->parent) {
        WalkChmUl(visitor, firstUl, isIndex, 1);
        return;
    }
    const GumboNode* parent = firstUl->parent;
    const GumboVector* siblings =
        (parent->type == GUMBO_NODE_ELEMENT) ? &parent->v.element.children : &parent->v.document.children;
    for (size_t s = firstUl->index_within_parent; s < siblings->length; s++) {
        const GumboNode* sib = (const GumboNode*)siblings->data[s];
        if (sib->type != GUMBO_NODE_ELEMENT || !GumboTagNameIs(sib, "ul")) {
            break;
        }
        WalkChmUl(visitor, sib, isIndex, 1);
    }
}

// Ignore any <ul><li> structure and visit every <object type="text/sitemap">
// in document order. Used for ToCs where the list scaffolding is broken.
static bool WalkBrokenChmTocOrIndex(EbookTocVisitor* visitor, const GumboNode* root, bool isIndex, bool* hadOneInOut) {
    // iterative pre-order DFS so a deeply nested document can't overflow the stack
    Vec<const GumboNode*> toVisit;
    toVisit.Append(root);
    while (len(toVisit) > 0) {
        const GumboNode* node = toVisit.Pop();
        if (!node) {
            continue;
        }
        if (node->type == GUMBO_NODE_ELEMENT && GumboTagNameIs(node, "object")) {
            const GumboAttribute* type = gumbo_get_attribute(&node->v.element.attributes, "type");
            if (type && str::EqI(type->value, "text/sitemap")) {
                *hadOneInOut |= isIndex ? VisitChmIndexItem(visitor, node, 1) : VisitChmTocItem(visitor, node, 1);
                continue; // don't recurse into the object's <param> children
            }
        }
        const GumboVector* children = nullptr;
        if (node->type == GUMBO_NODE_ELEMENT) {
            children = &node->v.element.children;
        } else if (node->type == GUMBO_NODE_DOCUMENT) {
            children = &node->v.document.children;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                toVisit.Append((const GumboNode*)children->data[i - 1]);
            }
        }
    }
    return *hadOneInOut;
}

// True for the non-Latin1 single-byte codepages where a ToC label made up
// entirely of Latin-1 characters is almost certainly mis-encoded (see
// FixChmTocEntitiesTemp). Excludes 1252 (Latin-1 is correct there) and the
// multi-byte CJK codepages (single Latin-1 bytes don't reconstruct a DBCS
// stream).
static bool ChmTocNeedsEntityRemap(uint cp) {
    switch (cp) {
        case 874:  // Thai
        case 1250: // Central European
        case 1251: // Cyrillic
        case 1253: // Greek
        case 1254: // Turkish
        case 1255: // Hebrew
        case 1256: // Arabic
        case 1257: // Baltic
        case 1258: // Vietnamese
            return true;
    }
    return false;
}

// Map a codepoint (decoded by gumbo from a mis-authored Latin entity) back to
// the single source-codepage byte it stands for. Returns -1 if it can't be a
// single byte (then the label is left untouched).
static int ChmEntityByte(WCHAR c) {
    if (c <= 0xFF) {
        return (int)c; // Latin-1: codepoint == byte value
    }
    // Đ/đ: some HTML Help Workshop versions emit &Dstrok;/&dstrok; (U+0110/0111)
    // for the Latin-1 bytes 0xD0/0xF0 (Ð/ð)
    if (c == 0x0110) {
        return 0xD0;
    }
    if (c == 0x0111) {
        return 0xF0;
    }
    // CP-1252 places a few chars (€ ‚ ƒ … Š Œ Ž ' ' " " – — Ÿ ...) above U+00FF
    // at bytes 0x80-0x9F; recover those too
    char b = 0;
    BOOL defUsed = FALSE;
    int n = WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, &c, 1, &b, 1, nullptr, &defUsed);
    if (n == 1 && !defUsed) {
        return (u8)b;
    }
    return -1;
}

// `s` is utf8 with HTML entities already decoded by gumbo. Some .hhc ToCs
// authored by HTML Help Workshop store non-Latin labels as Latin-1 named
// entities -- e.g. the Cyrillic CP-1251 byte 0xCF ("П") written as &Iuml; (Ï,
// U+00CF). After decoding we get Latin-1 codepoints whose low byte is the
// original codepage byte. When the whole label is in the Latin-1 range and the
// document's codepage isn't Latin (1252), reinterpret those bytes in the
// codepage. Labels that decoded to real Unicode (codepoints > 0xFF, i.e. raw
// non-Latin bytes already converted by SmartToUtf8Temp) are left untouched, as
// are pure-ASCII labels (issue #842).
static const TempStr FixChmTocEntitiesTemp(Str s, uint codepage) {
    uint cp = (codepage == CP_ACP) ? GetACP() : codepage;
    if (!s || !ChmTocNeedsEntityRemap(cp)) {
        return s;
    }
    TempWStr ws = ToWStrTemp(s);
    str::Builder bytes;
    bool hasHigh = false;
    for (int i = 0; i < ws.len; i++) {
        int b = ChmEntityByte(ws.s[i]);
        if (b < 0) {
            return s; // real Unicode we can't trace to a byte -> leave as-is
        }
        if (b > 0x7F) {
            hasHigh = true;
        }
        bytes.AppendChar((char)(u8)b);
    }
    if (!hasHigh) {
        return s; // pure ASCII -> nothing to remap
    }
    return SmartToUtf8Temp(ToStr(bytes), cp);
}

// Wraps the caller's visitor to repair Latin-1-entity-encoded ToC labels (see
// FixChmTocEntitiesTemp) before forwarding them; urls/levels pass through.
struct ChmTocEntityFixer : EbookTocVisitor {
    EbookTocVisitor* inner;
    uint codepage;
    ChmTocEntityFixer(EbookTocVisitor* v, uint cp) : inner(v), codepage(cp) {}
    void Visit(Str name, Str url, int level) override {
        inner->Visit(FixChmTocEntitiesTemp(name, codepage), url, level);
    }
};

bool ChmFile::ParseTocOrIndex(EbookTocVisitor* visitor, Str path, bool isIndex) const {
    if (!path) {
        return false;
    }
    TempStr htmlData = GetDataTemp(path);
    if (str::IsEmpty(htmlData)) {
        return false;
    }
    // Convert to UTF-8 (handling UTF-8 BOM and the file's codepage) so gumbo's
    // attribute values come out in a known encoding -- no per-attribute
    // conversion needed in the visit functions.
    TempStr utf8 = SmartToUtf8Temp(htmlData, codepage);
    if (!utf8) {
        return false;
    }
    int n = ::len(utf8);

    GumboOptions opts = GumboMakeOptions();
    GumboOutput* output = gumbo_parse_with_options(&opts, utf8.s, n);
    if (!output) {
        return false;
    }

    // repair Latin-1-entity-encoded labels (e.g. Cyrillic written as &Iuml;...)
    // for non-Latin codepages, before they reach the caller's visitor (issue #842)
    ChmTocEntityFixer fixer(visitor, codepage);

    // Find <body>, then the first <ul> under it (DFS). <body> is optional.
    const GumboNode* body = GumboFindDescendantByTag(output->document, "body");
    const GumboNode* firstUl = GumboFindDescendantByTag(body ? body : output->document, "ul");
    bool result;
    if (firstUl) {
        WalkChmTocOrIndex(&fixer, firstUl, isIndex);
        result = true;
    } else {
        bool hadOne = false;
        WalkBrokenChmTocOrIndex(&fixer, output->document, isIndex, &hadOne);
        result = hadOne;
    }

    gumbo_destroy_output_iter(&opts, output);
    return result;
}

bool ChmFile::HasToc() const {
    return !str::IsEmpty(tocPath);
}

bool ChmFile::ParseToc(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, tocPath, false);
}

bool ChmFile::HasIndex() const {
    return !str::IsEmpty(indexPath);
}

bool ChmFile::ParseIndex(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, indexPath, true);
}

bool ChmFile::IsSupportedFileType(Kind kind) {
    return kind == kindFileChm;
}

ChmFile* ChmFile::CreateFromFile(Str path) {
    ChmFile* chmFile = new ChmFile();
    if (!chmFile || !chmFile->Load(path)) {
        delete chmFile;
        return nullptr;
    }
    return chmFile;
}
