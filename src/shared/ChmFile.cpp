/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm.h>
#include "base/ByteReaderWriter.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "base/HtmlTags.h"
#include "GumboHtmlParser.h"

#include "DocProperties.h"
#include "EbookBase.h"
#include "ChmFile.h"

static const struct {
    Str ChmFile::* field;
    u16 systemId;
    u16 windowsOff;
} chmStrings[] = {
    {&ChmFile::title, 3, 0x14},    {&ChmFile::tocPath, 0, 0x60}, {&ChmFile::indexPath, 1, 0x64},
    {&ChmFile::homePath, 2, 0x68}, {&ChmFile::creator, 9, 0},
};

ChmFile::~ChmFile() {
    // chm_ctx_free also closes the archive and frees the entries + their paths
    chm_ctx_free(chmCtx);
    for (const auto& field : chmStrings) {
        str::Free(this->*field.field);
    }
    str::Free(data);
}

// find an entry by path. CHM path resolution is case-insensitive (the old
// chm_resolve_object was too), which some files rely on - e.g. bug-842 has
// home=HTML/PCAbout.htm in #SYSTEM but the entry is /Html/PCAbout.htm.
static chm_entry* ChmLookupPath(const ChmFile* chm, Str path) {
    for (int i = 0; i < chm->nEntries; i++) {
        chm_entry* e = chm->entries[i];
        if (e->path && str::EqI(Str(e->path), path)) {
            return e;
        }
    }
    return nullptr;
}

// Resolve a CHM object by its path, normalizing the leading slash and
// tolerating backslashes in URLs the way Microsoft's HTML Help viewer does.
// Returns the entry (owned by chmCtx) or nullptr.
static chm_entry* ChmResolveObject(const ChmFile* chm, Str fileName) {
    if (len(fileName) == 0) {
        return nullptr;
    }
    if (!str::StartsWith(fileName, StrL("/"))) {
        fileName = str::JoinTemp(StrL("/"), fileName);
    } else if (str::StartsWith(fileName, StrL("///"))) {
        str::TrimPrefix(fileName, StrL("//"));
    }

    chm_entry* e = ChmLookupPath(chm, fileName);
    if (!e && str::ContainsChar(fileName, '\\')) {
        TempStr fileNameTemp = str::DupTemp(fileName);
        str::TransCharsInPlace(fileNameTemp, StrL("\\"), StrL("/"));
        e = ChmLookupPath(chm, fileNameTemp);
    }
    return e;
}

bool ChmFile::HasData(Str fileName) const {
    return ChmResolveObject(this, fileName) != nullptr;
}

TempStr ChmFile::GetDataTemp(Str fileName) const {
    chm_entry* e = ChmResolveObject(this, fileName);
    if (!e) {
        return {};
    }
    if (e->length > 128ULL * 1024 * 1024) {
        // limit to 128 MB
        return {};
    }
    int n = (int)e->length;

    // +1 for 0 terminator for C string compatibility
    u8* d = AllocArrayTemp<u8>(n + 1);
    if (!d) {
        return {};
    }
    if (chm_read_entry(chmCtx, e, d) != (int64_t)e->length) {
        return {};
    }

    return Str((char*)d, n);
}

// Strip a UTF-8 BOM if present; otherwise convert from `codepage` to UTF-8
// (unless already UTF-8). Returns a TempStr owned by the temp allocator.
TempStr SmartToUtf8Temp(Str s, uint codepage) {
    if (str::TrimPrefix(s, StrL(kUtf8Bom))) {
        return str::DupTemp(s);
    }
    if (CP_UTF8 == codepage) {
        return str::DupTemp(s);
    }
    return strconv::ToMultiByteTemp(s, codepage, CP_UTF8);
}

// Borrow a bounded string; callers copy retained metadata.
static Str ReadCharZ(Str d, int off) {
    // File offsets may wrap when narrowed to int.
    if (off < 0 || off >= len(d)) {
        return {};
    }
    Str value;
    Str rest(d.s + off, len(d) - off);
    if (!str::CutChar(rest, '\0', &value, nullptr) || len(value) == 0) {
        return {};
    }
    return value;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
void ChmFile::ParseWindowsData() {
    TempStr windowsData = GetDataTemp(StrL("/#WINDOWS"));
    TempStr stringsData = GetDataTemp(StrL("/#STRINGS"));

    if (len(windowsData) == 0 || len(stringsData) == 0) {
        return;
    }
    int windowsLen = windowsData.len;
    if (windowsLen <= 8) {
        return;
    }

    ByteReader rw(windowsData);
    int entries = (int)rw.UInt32LE(0);
    int entrySize = (int)rw.UInt32LE(4);
    if (entrySize < 188) {
        return;
    }

    for (int i = 0; i < entries && (i + 1) * entrySize <= windowsLen; i++) {
        int off = 8 + (i * entrySize);
        for (const auto& field : chmStrings) {
            Str& value = this->*field.field;
            if (field.windowsOff && str::IsNull(value)) {
                value = str::Dup(ReadCharZ(stringsData, (int)rw.UInt32LE(off + field.windowsOff)));
            }
        }
    }
}

constexpr int kCpChmDefault = 1252;

static uint LcidToCodepage(DWORD lcid) {
    // cf. http://msdn.microsoft.com/en-us/library/bb165625(v=VS.90).aspx
    static const struct {
        u16 lcid;
        u16 codepage;
    } lcidToCodepage[] = {
        {1025, 1256}, {2052, 936},  {1028, 950},  {1029, 1250}, {1032, 1253}, {1037, 1255}, {1038, 1250},
        {1041, 932},  {1042, 949},  {1045, 1250}, {1049, 1251}, {1051, 1250}, {1060, 1250}, {1055, 1254},
        {1026, 1251}, {4, 936},     {1058, 1251}, {1059, 1251}, {3098, 1251}, {2074, 1251}, {1071, 1251},
        {1087, 1251}, {1088, 1251}, {1092, 1251}, {1104, 1251}, {2092, 1251},
    };

    for (const auto& entry : lcidToCodepage) {
        if (lcid == entry.lcid) {
            return entry.codepage;
        }
    }

    return kCpChmDefault;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#SYSTEM
bool ChmFile::ParseSystemData() {
    TempStr d = GetDataTemp(StrL("/#SYSTEM"));
    if (len(d) == 0) {
        return false;
    }

    ByteReader r(d);
    DWORD n = 0;
    // Note: skipping DWORD version at offset 0. It's supposed to be 2 or 3.
    for (int off = 4; off + 4 < d.len; off += (int)n + 4) {
        // Note: at some point we seem to get off-sync i.e. I'm seeing
        // many entries with type == 0 and length == 0. Seems harmless.
        n = r.UInt16LE(off + 2);
        if (n == 0) {
            continue;
        }
        WORD type = r.UInt16LE(off);
        if (type == 4 && !codepage && n >= 4) {
            codepage = LcidToCodepage(r.UInt32LE(off + 4));
        }
        for (const auto& field : chmStrings) {
            Str& value = this->*field.field;
            if (type == field.systemId && str::IsNull(value)) {
                value = str::Dup(ReadCharZ(d, off + 4));
                break;
            }
        }
    }

    return true;
}

TempStr ChmFile::ResolveTopicID(unsigned int id) const {
    TempStr ivbData = GetDataTemp(StrL("/#IVB"));
    int ivbLen = ivbData.len;
    ByteReader br(ivbData);
    if ((ivbLen % 8) != 4 || ivbLen - 4 != (int)br.UInt32LE(0)) {
        return {};
    }

    for (int off = 4; off < ivbLen; off += 8) {
        if (br.UInt32LE(off) == id) {
            TempStr stringsData = GetDataTemp(StrL("/#STRINGS"));
            return ReadCharZ(stringsData, (int)br.UInt32LE(off + 4));
        }
    }
    return {};
}

void ChmFile::FixPathCodepage(Str& path, uint& fileCP) {
    if (len(path) == 0 || HasData(path)) {
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
    chmCtx = chm_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!chmCtx || !chm_open(chmCtx, (const uint8_t*)data.s, (size_t)data.len)) {
        return false;
    }
    // the data buffer must outlive chmCtx (chm_open doesn't copy it); it does,
    // it's freed in ~ChmFile after chm_ctx_free
    nEntries = chm_get_entries(chmCtx, &entries);

    ParseWindowsData();
    if (!ParseSystemData()) {
        return false;
    }

    uint fileCodepage = codepage;
    char header[24]{};
    int n = file::ReadN(path, (u8*)header, sizeof(header));
    if (n < sizeofi(header)) {
        ByteReader r(Str(header, sizeof(header)));
        DWORD lcid = r.UInt32LE(20);
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
        Str pathsToTest[] = {StrL("/index.htm"), StrL("/index.html"), StrL("/default.htm"), StrL("/default.html")};
        for (Str testPath : pathsToTest) {
            if (HasData(testPath)) {
                str::ReplaceWithCopy(&homePath, testPath);
            }
        }
        if (!HasData(homePath)) {
            return false;
        }
    }

    return true;
}

TempStr ChmFile::GetPropertyTemp(DocProp prop) const {
    TempStr result;
    if (prop == DocProp::Title && len(title) > 0) {
        result = SmartToUtf8Temp(title, codepage);
    } else if (prop == DocProp::CreatorApp && len(creator) > 0) {
        result = SmartToUtf8Temp(creator, codepage);
    }
    if (len(result) == 0) {
        return {};
    }
    str::NormalizeWSInPlace(result);
    return result;
}

TempStr ChmFile::GetHomePath() const {
    return homePath;
}

void ChmFile::GetAllPaths(StrVec* v) const {
    // equivalent of the old CHM_ENUMERATE_FILES | CHM_ENUMERATE_NORMAL
    for (int i = 0; i < nEntries; i++) {
        chm_entry* e = entries[i];
        if (e->is_file && e->is_normal && e->path && e->path[0]) {
            v->Append(Str(e->path));
        }
    }
}

// Strip the "ITS protocol" prefix from a CHM URL, e.g.
// "mk:@MSITStore:foo.chm::/index.html" -> "index.html".
static Str StripItsProtocol(Str url) {
    Str p;
    str::Cut(url, StrL("::/"), nullptr, &p);
    return p ? p : url;
}

enum class ChmItemKind {
    Toc,
    Index
};

static bool VisitChmItem(EbookTocVisitor* visitor, const GumboNode* objNode, ChmItemKind kind, int level) {
    ReportIf(!GumboTagNameIs(objNode, StrL("object")));

    StrVec references;
    Str keyword;
    Str name, local;
    const GumboVector* children = &objNode->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (!GumboTagNameIs(child, StrL("param"))) {
            continue;
        }
        const GumboAttribute* attrName = gumbo_get_attribute(&child->v.element.attributes, "name");
        const GumboAttribute* attrVal = gumbo_get_attribute(&child->v.element.attributes, "value");
        if (!attrName || !attrVal) {
            continue;
        }
        if (kind == ChmItemKind::Index && str::EqI(Str(attrName->value), StrL("Keyword"))) {
            keyword = Str(attrVal->value);
        } else if (str::EqI(Str(attrName->value), StrL("Name"))) {
            name = Str(attrVal->value);
            // Some indexes use Name without Keyword.
            if (len(keyword) == 0) {
                keyword = name;
            }
        } else if (str::EqI(Str(attrName->value), StrL("Local"))) {
            local = StripItsProtocol(Str(attrVal->value));
            if (kind == ChmItemKind::Index && name) {
                references.Append(name);
                references.Append(local);
            }
        }
    }
    if (kind == ChmItemKind::Toc) {
        if (len(name) == 0) {
            return false;
        }
        visitor->Visit(name, local, level);
        return true;
    }
    if (len(keyword) == 0) {
        return false;
    }

    if (len(references) == 2) {
        visitor->Visit(keyword, references[1], level);
        return true;
    }
    visitor->Visit(keyword, {}, level);
    for (int i = 0; i < len(references); i += 2) {
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
    VecAppend(stack, {ulNode, level, 0});
    while (len(stack) > 0) {
        ChmUlFrame& top = VecLast(stack);
        const GumboVector* lis = &top.ul->v.element.children;
        if (top.i >= lis->length) {
            VecRemoveLast(stack);
            continue;
        }
        const GumboNode* child = (const GumboNode*)lis->data[top.i];
        int lvl = top.level;
        top.i++;
        // any stack.Append() below may reallocate -> don't touch `top` after this

        if (GumboTagNameIs(child, StrL("ul"))) {
            // a bare <ul> among the <li>s holds the children of the preceding <li>
            VecAppend(stack, {child, lvl + 1, 0});
            continue;
        }
        const GumboNode* li = child;
        if (!GumboTagNameIs(li, StrL("li"))) {
            continue; // skip whitespace / text / unexpected nodes
        }
        const GumboNode* objNode = GumboFindChildByTag(li, StrL("object"));
        if (!objNode) {
            continue;
        }
        bool valid = VisitChmItem(visitor, objNode, isIndex ? ChmItemKind::Index : ChmItemKind::Toc, lvl);
        if (!valid) {
            continue;
        }
        const GumboNode* nested = GumboFindChildByTag(li, StrL("ul"));
        if (nested) {
            VecAppend(stack, {nested, lvl + 1, 0});
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
        if (sib->type != GUMBO_NODE_ELEMENT || !GumboTagNameIs(sib, StrL("ul"))) {
            break;
        }
        WalkChmUl(visitor, sib, isIndex, 1);
    }
}

// Ignore any <ul><li> structure and visit every <object type="text/sitemap">
// in document order. Used for ToCs where the list scaffolding is broken.
static bool WalkBrokenChmTocOrIndex(EbookTocVisitor* visitor, const GumboNode* root, bool isIndex) {
    bool hadOne = false;
    // iterative pre-order DFS so a deeply nested document can't overflow the stack
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, root);
    while (len(toVisit) > 0) {
        const GumboNode* node = VecPop(toVisit);
        if (!node) {
            continue;
        }
        if (node->type == GUMBO_NODE_ELEMENT && GumboTagNameIs(node, StrL("object"))) {
            const GumboAttribute* type = gumbo_get_attribute(&node->v.element.attributes, "type");
            if (type && str::EqI(Str(type->value), StrL("text/sitemap"))) {
                hadOne |= VisitChmItem(visitor, node, isIndex ? ChmItemKind::Index : ChmItemKind::Toc, 1);
                continue; // don't recurse into the object's <param> children
            }
        }
        GumboPushChildren(toVisit, node);
    }
    return hadOne;
}

// True for the non-Latin1 single-byte codepages where a ToC label made up
// entirely of Latin-1 characters is almost certainly mis-encoded (see
// FixChmTocEntitiesTemp). Excludes 1252 (Latin-1 is correct there) and the
// multi-byte CJK codepages (single Latin-1 bytes don't reconstruct a DBCS
// stream).
static bool ChmTocNeedsEntityRemap(uint cp) {
    return cp == 874 || (cp >= 1250 && cp <= 1258 && cp != 1252);
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
    static const u16 cp1252High[32] = {0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                       0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
                                       0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                       0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};
    for (int i = 0; i < 32; i++) {
        if (cp1252High[i] != 0 && cp1252High[i] == (u16)c) {
            return 0x80 + i;
        }
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
static TempStr FixChmTocEntitiesTemp(Str s, uint codepage) {
    uint cp = (codepage == CP_ACP) ? GetACP() : codepage;
    if (len(s) == 0 || !ChmTocNeedsEntityRemap(cp)) {
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
    if (len(path) == 0) {
        return false;
    }
    TempStr htmlData = GetDataTemp(path);
    if (len(htmlData) == 0) {
        return false;
    }
    // Convert to UTF-8 (handling UTF-8 BOM and the file's codepage) so gumbo's
    // attribute values come out in a known encoding -- no per-attribute
    // conversion needed in the visit functions.
    TempStr utf8 = SmartToUtf8Temp(htmlData, codepage);
    if (len(utf8) == 0) {
        return false;
    }
    GumboDoc doc(utf8, GumboMode::Html);
    if (!doc.Document()) {
        return false;
    }

    // repair Latin-1-entity-encoded labels (e.g. Cyrillic written as &Iuml;...)
    // for non-Latin codepages, before they reach the caller's visitor (issue #842)
    ChmTocEntityFixer fixer(visitor, codepage);

    // Find <body>, then the first <ul> under it (DFS). <body> is optional.
    const GumboNode* body = GumboFindDescendantByTag(doc.Document(), StrL("body"));
    const GumboNode* firstUl = GumboFindDescendantByTag(body ? body : doc.Document(), StrL("ul"));
    if (firstUl) {
        WalkChmTocOrIndex(&fixer, firstUl, isIndex);
        return true;
    }
    return WalkBrokenChmTocOrIndex(&fixer, doc.Document(), isIndex);
}

bool ChmFile::HasToc() const {
    return len(tocPath) > 0;
}

bool ChmFile::ParseToc(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, tocPath, false);
}

bool ChmFile::HasIndex() const {
    return len(indexPath) > 0;
}

bool ChmFile::ParseIndex(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, indexPath, true);
}

bool ChmFile::IsSupportedFileType(FileType kind) {
    return kind == FileType::Chm;
}

ChmFile* ChmFile::CreateFromFile(Str path) {
    ChmFile* chmFile = new ChmFile();
    if (!chmFile || !chmFile->Load(path)) {
        delete chmFile;
        return nullptr;
    }
    return chmFile;
}
