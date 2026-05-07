/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <chm_lib.h>
#include "utils/ByteReader.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "GumboHelpers.h"

#include "DocController.h"
#include "DocProperties.h"
#include "EbookBase.h"
#include "ChmFile.h"

ChmFile::~ChmFile() {
    chm_close(chmHandle);
}

bool ChmFile::HasData(const char* fileName) const {
    if (!fileName) {
        return false;
    }

    if (!str::StartsWith(fileName, "/")) {
        fileName = str::JoinTemp("/", fileName);
    } else if (str::StartsWith(fileName, "///")) {
        fileName += 2;
    }

    struct chmUnitInfo info{};
    return chm_resolve_object(chmHandle, fileName, &info) == CHM_RESOLVE_SUCCESS;
}

ByteSlice ChmFile::GetData(const char* fileName) const {
    if (!str::StartsWith(fileName, "/")) {
        fileName = str::JoinTemp("/", fileName);
    } else if (str::StartsWith(fileName, "///")) {
        fileName = fileName + 2;
    }

    struct chmUnitInfo info;
    int res = chm_resolve_object(chmHandle, fileName, &info);
    if (CHM_RESOLVE_SUCCESS != res && str::FindChar(fileName, '\\')) {
        // Microsoft's HTML Help CHM viewer tolerates backslashes in URLs
        auto fileNameTemp = str::DupTemp(fileName);
        str::TransCharsInPlace(fileNameTemp, "\\", "/");
        res = chm_resolve_object(chmHandle, fileNameTemp, &info);
    }

    if (CHM_RESOLVE_SUCCESS != res) {
        return {};
    }
    size_t len = (size_t)info.length;
    if (len > 128 * 1024 * 1024) {
        // limit to 128 MB
        return {};
    }

    // +1 for 0 terminator for C string compatibility
    u8* d = AllocArray<u8>(len + 1);
    if (!d) {
        return {};
    }
    if (!chm_retrieve_object(chmHandle, &info, d, 0, len)) {
        return {};
    }

    return {d, len};
}

TempStr SmartToUtf8Temp(const char* s, uint codepage) {
    if (str::StartsWith(s, UTF8_BOM)) {
        return str::DupTemp(s + 3);
    }
    if (CP_UTF8 == codepage) {
        return str::DupTemp(s);
    }
    return strconv::ToMultiByteTemp(s, codepage, CP_UTF8);
}

static char* GetCharZ(const ByteSlice& d, size_t off) {
    u8* data = d.data();
    size_t len = d.size();
    if (off >= len) {
        return nullptr;
    }
    ReportIf(!memchr(data + off, '\0', len - off + 1)); // data is zero-terminated
    u8* str = data + off;
    if (str::IsEmpty((const char*)str)) {
        return nullptr;
    }
    return str::Dup((const char*)str);
}

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
void ChmFile::ParseWindowsData() {
    ByteSlice windowsData = GetData("/#WINDOWS");
    ByteSlice stringsData = GetData("/#STRINGS");

    AutoFree stringsDataFree(stringsData);
    AutoFree windowsDataFree(windowsData);

    if (windowsData.empty() || stringsData.empty()) {
        return;
    }
    size_t windowsLen = windowsData.size();
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
        if (!title) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x14);
            title.Set(GetCharZ(stringsData, strOff));
        }
        if (!tocPath) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x60);
            tocPath.Set(GetCharZ(stringsData, strOff));
        }
        if (!indexPath) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x64);
            indexPath.Set(GetCharZ(stringsData, strOff));
        }
        if (!homePath) {
            DWORD strOff = rw.DWordLE(off + (size_t)0x68);
            homePath.Set(GetCharZ(stringsData, strOff));
        }
    }
}

#define CP_CHM_DEFAULT 1252

static uint LcidToCodepage(DWORD lcid) {
    // cf. http://msdn.microsoft.com/en-us/library/bb165625(v=VS.90).aspx
    static struct {
        DWORD lcid;
        uint codepage;
    } lcidToCodepage[] = {
        {1025, 1256}, {2052, 936},  {1028, 950},  {1029, 1250}, {1032, 1253}, {1037, 1255}, {1038, 1250}, {1041, 932},
        {1042, 949},  {1045, 1250}, {1049, 1251}, {1051, 1250}, {1060, 1250}, {1055, 1254}, {1026, 1251}, {4, 936},
    };

    for (int i = 0; i < dimofi(lcidToCodepage); i++) {
        if (lcid == lcidToCodepage[i].lcid) {
            return lcidToCodepage[i].codepage;
        }
    }

    return CP_CHM_DEFAULT;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#SYSTEM
bool ChmFile::ParseSystemData() {
    ByteSlice d = GetData("/#SYSTEM");
    if (d.empty()) {
        return false;
    }
    AutoFree dataFree = d.Get();

    ByteReader r(d);
    DWORD len = 0;
    // Note: skipping DWORD version at offset 0. It's supposed to be 2 or 3.
    for (size_t off = 4; off + 4 < d.size(); off += len + (size_t)4) {
        // Note: at some point we seem to get off-sync i.e. I'm seeing
        // many entries with type == 0 and len == 0. Seems harmless.
        len = r.WordLE(off + 2);
        if (len == 0) {
            continue;
        }
        WORD type = r.WordLE(off);
        switch (type) {
            case 0:
                if (!tocPath) {
                    tocPath.Set(GetCharZ(d, off + 4));
                }
                break;
            case 1:
                if (!indexPath) {
                    indexPath.Set(GetCharZ(d, off + 4));
                }
                break;
            case 2:
                if (!homePath) {
                    homePath.Set(GetCharZ(d, off + 4));
                }
                break;
            case 3:
                if (!title) {
                    title.Set(GetCharZ(d, off + 4));
                }
                break;
            case 4:
                if (!codepage && len >= 4) {
                    codepage = LcidToCodepage(r.DWordLE(off + 4));
                }
                break;
            case 6:
                // compiled file - ignore
                break;
            case 9:
                if (!creator) {
                    creator.Set(GetCharZ(d, off + 4));
                }
                break;
            case 16:
                // default font - ignore
                break;
        }
    }

    return true;
}

char* ChmFile::ResolveTopicID(unsigned int id) const {
    ByteSlice ivbData = GetData("/#IVB");
    AutoFree f = ivbData.Get();
    size_t ivbLen = ivbData.size();
    ByteReader br(ivbData);
    if ((ivbLen % 8) != 4 || ivbLen - 4 != br.DWordLE(0)) {
        return nullptr;
    }

    for (size_t off = 4; off < ivbLen; off += 8) {
        if (br.DWordLE(off) == id) {
            ByteSlice stringsData = GetData("/#STRINGS");
            char* res = GetCharZ(stringsData, br.DWordLE(off + 4));
            stringsData.Free();
            return res;
        }
    }
    return nullptr;
}

void ChmFile::FixPathCodepage(AutoFreeStr& path, uint& fileCP) {
    if (!path || HasData(path)) {
        return;
    }

    TempStr utf8Path = SmartToUtf8Temp(path.Get(), codepage);
    if (HasData(utf8Path)) {
        path.SetCopy(utf8Path);
        fileCP = codepage;
        return;
    }

    if (fileCP == codepage) {
        return;
    }

    utf8Path = SmartToUtf8Temp(path.Get(), fileCP);
    if (HasData(utf8Path)) {
        path.SetCopy(utf8Path);
        codepage = fileCP;
        return;
    }
}

bool ChmFile::Load(const char* path) {
    ByteSlice fileContent = file::ReadFile(path);
    data = fileContent.Get();
    chmHandle = chm_open(fileContent, fileContent.size());
    if (!chmHandle) {
        return false;
    }

    ParseWindowsData();
    if (!ParseSystemData()) {
        return false;
    }

    uint fileCodepage = codepage;
    char header[24]{};
    int n = file::ReadN(path, header, sizeof(header));
    if (n < (int)sizeof(header)) {
        ByteReader r(header, sizeof(header));
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
        const char* pathsToTest[] = {"/index.htm", "/index.html", "/default.htm", "/default.html"};
        for (int i = 0; i < dimof(pathsToTest); i++) {
            if (HasData(pathsToTest[i])) {
                homePath.SetCopy(pathsToTest[i]);
            }
        }
        if (!HasData(homePath)) {
            return false;
        }
    }

    return true;
}

TempStr ChmFile::GetPropertyTemp(const char* name) const {
    char* result = nullptr;
    if (str::Eq(kPropTitle, name) && title.CStr()) {
        result = SmartToUtf8Temp(title.CStr(), codepage);
    } else if (str::Eq(kPropCreatorApp, name) && creator.CStr()) {
        result = SmartToUtf8Temp(creator.CStr(), codepage);
    }
    // TODO: shouldn't it be up to the front-end to normalize whitespace?
    if (!result) {
        return nullptr;
    }
    // TODO: original code called str::RemoveCharsInPlace(result, "\n\r\t")
    str::NormalizeWSInPlace(result);
    return result;
}

const char* ChmFile::GetHomePath() const {
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
static const char* StripItsProtocol(const char* url) {
    const char* p = str::Find(url, "::/");
    return p ? p + 3 : url;
}

static bool VisitChmTocItem(EbookTocVisitor* visitor, const GumboNode* objNode, int level) {
    ReportIf(!GumboTagNameIs(objNode, "object"));

    AutoFreeStr name, local;
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
            name.Set(str::Dup(attrVal->value));
        } else if (str::EqI(attrName->value, "Local")) {
            local.Set(str::Dup(StripItsProtocol(attrVal->value)));
        }
    }
    if (!name) {
        return false;
    }
    visitor->Visit(name.Get(), local.Get(), level);
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
    const char* keyword = nullptr;
    const char* name = nullptr;
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
            keyword = attrVal->value;
        } else if (str::EqI(attrName->value, "Name")) {
            name = attrVal->value;
            // some CHM documents seem to use a lonely Name instead of Keyword
            if (!keyword) {
                keyword = name;
            }
        } else if (str::EqI(attrName->value, "Local") && name) {
            references.Append((char*)name);
            references.Append((char*)StripItsProtocol(attrVal->value));
        }
    }
    if (!keyword) {
        return false;
    }

    if (references.Size() == 2) {
        visitor->Visit((char*)keyword, references[1], level);
        return true;
    }
    visitor->Visit((char*)keyword, nullptr, level);
    int n = references.Size();
    for (int i = 0; i < n; i += 2) {
        visitor->Visit(references[i], references[i + 1], level + 1);
    }
    return true;
}

// Process a single <ul>'s <li> children as TOC entries at `level`.
// Recurses into nested <ul>s (either as <li>'s child, or as a <ul> sibling
// directly after the <li> -- some broken ToCs do that).
static void WalkChmUl(EbookTocVisitor* visitor, const GumboNode* ulNode, bool isIndex, int level) {
    const GumboVector* lis = &ulNode->v.element.children;
    for (unsigned int j = 0; j < lis->length; j++) {
        const GumboNode* li = (const GumboNode*)lis->data[j];
        if (!GumboTagNameIs(li, "li")) {
            continue;
        }
        const GumboNode* objNode = GumboFindChildByTag(li, "object");
        if (!objNode) {
            continue;
        }
        bool valid = isIndex ? VisitChmIndexItem(visitor, objNode, level) : VisitChmTocItem(visitor, objNode, level);
        if (!valid) {
            continue;
        }
        const GumboNode* nested = GumboFindChildByTag(li, "ul");
        if (!nested && j + 1 < lis->length) {
            const GumboNode* afterLi = (const GumboNode*)lis->data[j + 1];
            if (afterLi->type == GUMBO_NODE_ELEMENT && GumboTagNameIs(afterLi, "ul")) {
                nested = afterLi;
            }
        }
        if (nested) {
            WalkChmUl(visitor, nested, isIndex, level + 1);
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
static bool WalkBrokenChmTocOrIndex(EbookTocVisitor* visitor, const GumboNode* node, bool isIndex, bool* hadOneInOut) {
    if (!node) {
        return *hadOneInOut;
    }
    if (node->type == GUMBO_NODE_ELEMENT && GumboTagNameIs(node, "object")) {
        const GumboAttribute* type = gumbo_get_attribute(&node->v.element.attributes, "type");
        if (type && str::EqI(type->value, "text/sitemap")) {
            *hadOneInOut |= isIndex ? VisitChmIndexItem(visitor, node, 1) : VisitChmTocItem(visitor, node, 1);
            return *hadOneInOut; // don't recurse into the object's <param> children
        }
    }
    const GumboVector* children = nullptr;
    if (node->type == GUMBO_NODE_ELEMENT) {
        children = &node->v.element.children;
    } else if (node->type == GUMBO_NODE_DOCUMENT) {
        children = &node->v.document.children;
    }
    if (children) {
        for (unsigned int i = 0; i < children->length; i++) {
            WalkBrokenChmTocOrIndex(visitor, (const GumboNode*)children->data[i], isIndex, hadOneInOut);
        }
    }
    return *hadOneInOut;
}

bool ChmFile::ParseTocOrIndex(EbookTocVisitor* visitor, const char* path, bool isIndex) const {
    if (!path) {
        return false;
    }
    ByteSlice htmlData = GetData(path);
    if (htmlData.empty()) {
        return false;
    }
    AutoFreeStr htmlFree = htmlData.Get();
    // Convert to UTF-8 (handling UTF-8 BOM and the file's codepage) so gumbo's
    // attribute values come out in a known encoding -- no per-attribute
    // conversion needed in the visit functions.
    TempStr utf8 = SmartToUtf8Temp((const char*)htmlData.data(), codepage);
    if (!utf8) {
        return false;
    }
    size_t len = str::Len(utf8);

    GumboOptions opts = GumboMakeOptions();
    GumboOutput* output = gumbo_parse_with_options(&opts, utf8, len);
    if (!output) {
        return false;
    }

    // Find <body>, then the first <ul> under it (DFS). <body> is optional.
    const GumboNode* body = GumboFindDescendantByTag(output->document, "body");
    const GumboNode* firstUl = GumboFindDescendantByTag(body ? body : output->document, "ul");
    bool result;
    if (firstUl) {
        WalkChmTocOrIndex(visitor, firstUl, isIndex);
        result = true;
    } else {
        bool hadOne = false;
        WalkBrokenChmTocOrIndex(visitor, output->document, isIndex, &hadOne);
        result = hadOne;
    }

    gumbo_destroy_output(&opts, output);
    return result;
}

bool ChmFile::HasToc() const {
    return tocPath != nullptr;
}

bool ChmFile::ParseToc(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, tocPath, false);
}

bool ChmFile::HasIndex() const {
    return indexPath != nullptr;
}

bool ChmFile::ParseIndex(EbookTocVisitor* visitor) const {
    return ParseTocOrIndex(visitor, indexPath, true);
}

bool ChmFile::IsSupportedFileType(Kind kind) {
    return kind == kindFileChm;
}

ChmFile* ChmFile::CreateFromFile(const char* path) {
    ChmFile* chmFile = new ChmFile();
    if (!chmFile || !chmFile->Load(path)) {
        delete chmFile;
        return nullptr;
    }
    return chmFile;
}
