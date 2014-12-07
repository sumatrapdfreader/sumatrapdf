/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#define PPC_BSTR
#include <chm_lib.h>
#include "ByteReader.h"
#include "FileUtil.h"
#include "HtmlParserLookup.h"
#include "TrivialHtmlParser.h"
// rendering engines
#include "BaseEngine.h"
#include "EbookBase.h"
#include "ChmDoc.h"

ChmDoc::~ChmDoc()
{
    chm_close(chmHandle);
}

bool ChmDoc::HasData(const char *fileName)
{
    if (!fileName)
        return NULL;

    ScopedMem<char> tmpName;
    if (!str::StartsWith(fileName, "/")) {
        tmpName.Set(str::Join("/", fileName));
        fileName = tmpName;
    }
    else if (str::StartsWith(fileName, "///"))
        fileName += 2;

    struct chmUnitInfo info;
    return chm_resolve_object(chmHandle, fileName, &info) == CHM_RESOLVE_SUCCESS;
}

unsigned char *ChmDoc::GetData(const char *fileName, size_t *lenOut)
{
    ScopedMem<char> fileNameTmp;
    if (!str::StartsWith(fileName, "/")) {
        fileNameTmp.Set(str::Join("/", fileName));
        fileName = fileNameTmp;
    } else if (str::StartsWith(fileName, "///")) {
        fileName += 2;
    }

    struct chmUnitInfo info;
    int res = chm_resolve_object(chmHandle, fileName, &info);
    if (CHM_RESOLVE_SUCCESS != res)
        return NULL;
    size_t len = (size_t)info.length;
    if (len > 128 * 1024 * 1024) {
        // don't allow anything above 128 MB
        return NULL;
    }

    // +1 for 0 terminator for C string compatibility
    ScopedMem<unsigned char> data((unsigned char *)malloc(len + 1));
    if (!data)
        return NULL;
    if (!chm_retrieve_object(chmHandle, &info, data.Get(), 0, len))
        return NULL;
    data[len] = '\0';

    if (lenOut)
        *lenOut = len;
    return data.StealData();
}

char *ChmDoc::ToUtf8(const unsigned char *text, UINT overrideCP)
{
    const char *s = (char *)text;
    if (str::StartsWith(s, UTF8_BOM))
        return str::Dup(s + 3);
    if (overrideCP)
        return str::ToMultiByte(s, overrideCP, CP_UTF8);
    if (CP_UTF8 == codepage)
        return str::Dup(s);
    return str::ToMultiByte(s, codepage, CP_UTF8);
}

WCHAR *ChmDoc::ToStr(const char *text)
{
    return str::conv::FromCodePage(text, codepage);
}

static char *GetCharZ(const unsigned char *data, size_t len, size_t off)
{
    if (off >= len)
        return NULL;
    CrashIf(!memchr(data + off, '\0', len - off + 1)); // data is zero-terminated
    const char *str = (char *)data + off;
    if (str::IsEmpty(str))
        return NULL;
    return str::Dup(str);
}

// http://www.nongnu.org/chmspec/latest/Internal.html#WINDOWS
void ChmDoc::ParseWindowsData()
{
    size_t windowsLen, stringsLen;
    ScopedMem<unsigned char> windowsData(GetData("/#WINDOWS", &windowsLen));
    ScopedMem<unsigned char> stringsData(GetData("/#STRINGS", &stringsLen));
    if (!windowsData || !stringsData)
        return;
    if (windowsLen <= 8)
        return;

    ByteReader rw(windowsData, windowsLen);
    DWORD entries = rw.DWordLE(0);
    DWORD entrySize = rw.DWordLE(4);
    if (entrySize < 188)
        return;

    for (DWORD i = 0; i < entries && (i + 1) * entrySize <= windowsLen; i++) {
        DWORD off = 8 + i * entrySize;
        if (!title) {
            DWORD strOff = rw.DWordLE(off + 0x14);
            title.Set(GetCharZ(stringsData, stringsLen, strOff));
        }
        if (!tocPath) {
            DWORD strOff = rw.DWordLE(off + 0x60);
            tocPath.Set(GetCharZ(stringsData, stringsLen, strOff));
        }
        if (!indexPath) {
            DWORD strOff = rw.DWordLE(off + 0x64);
            indexPath.Set(GetCharZ(stringsData, stringsLen, strOff));
        }
        if (!homePath) {
            DWORD strOff = rw.DWordLE(off + 0x68);
            homePath.Set(GetCharZ(stringsData, stringsLen, strOff));
        }
    }
}

#define CP_CHM_DEFAULT 1252

static UINT LcidToCodepage(DWORD lcid)
{
    // cf. http://msdn.microsoft.com/en-us/library/bb165625(v=VS.90).aspx
    static struct {
        DWORD lcid;
        UINT codepage;
    } lcidToCodepage[] = {
        { 1025, 1256 }, { 2052,  936 }, { 1028,  950 }, { 1029, 1250 },
        { 1032, 1253 }, { 1037, 1255 }, { 1038, 1250 }, { 1041,  932 },
        { 1042,  949 }, { 1045, 1250 }, { 1049, 1251 }, { 1051, 1250 },
        { 1060, 1250 }, { 1055, 1254 }, { 1026, 1251 },
    };

    for (int i = 0; i < dimof(lcidToCodepage); i++) {
        if (lcid == lcidToCodepage[i].lcid)
            return lcidToCodepage[i].codepage;
    }

    return CP_CHM_DEFAULT;
}

// http://www.nongnu.org/chmspec/latest/Internal.html#SYSTEM
bool ChmDoc::ParseSystemData()
{
    size_t dataLen;
    ScopedMem<unsigned char> data(GetData("/#SYSTEM", &dataLen));
    if (!data)
        return false;

    ByteReader r(data, dataLen);
    DWORD len = 0;
    // Note: skipping DWORD version at offset 0. It's supposed to be 2 or 3.
    for (size_t off = 4; off + 4 < dataLen; off += len + 4) {
        // Note: at some point we seem to get off-sync i.e. I'm seeing
        // many entries with type == 0 and len == 0. Seems harmless.
        len = r.WordLE(off + 2);
        if (len == 0)
            continue;
        WORD type = r.WordLE(off);
        switch (type) {
        case 0:
            if (!tocPath)
                tocPath.Set(GetCharZ(data, dataLen, off + 4));
            break;
        case 1:
            if (!indexPath)
                indexPath.Set(GetCharZ(data, dataLen, off + 4));
            break;
        case 2:
            if (!homePath)
                homePath.Set(GetCharZ(data, dataLen, off + 4));
            break;
        case 3:
            if (!title)
                title.Set(GetCharZ(data, dataLen, off + 4));
            break;
        case 4:
            if (!codepage && len >= 4)
                codepage = LcidToCodepage(r.DWordLE(off + 4));
            break;
        case 6:
            // compiled file - ignore
            break;
        case 9:
            if (!creator)
                creator.Set(GetCharZ(data, dataLen, off + 4));
            break;
        case 16:
            // default font - ignore
            break;
        }
    }

    return true;
}

bool ChmDoc::Load(const WCHAR *fileName)
{
    chmHandle = chm_open((WCHAR *)fileName);
    if (!chmHandle)
        return false;

    ParseWindowsData();
    if (!ParseSystemData())
        return false;

    if (!HasData(homePath)) {
        const char *pathsToTest[] = {
            "/index.htm", "/index.html", "/default.htm", "/default.html"
        };
        for (int i = 0; i < dimof(pathsToTest); i++) {
            if (HasData(pathsToTest[i]))
                homePath.Set(str::Dup(pathsToTest[i]));
        }
    }
    if (!HasData(homePath))
        return false;

    if (!codepage) {
        char header[24] = { 0 };
        if (file::ReadN(fileName, header, sizeof(header))) {
            DWORD lcid = ByteReader(header, sizeof(header)).DWordLE(20);
            codepage = LcidToCodepage(lcid);
        }
        else
            codepage = CP_CHM_DEFAULT;
    }
    if (GetACP() == codepage)
        codepage = CP_ACP;

    return true;
}

WCHAR *ChmDoc::GetProperty(DocumentProperty prop)
{
    ScopedMem<WCHAR> result;
    if (Prop_Title == prop && title)
        result.Set(ToStr(title));
    else if (Prop_CreatorApp == prop && creator)
        result.Set(ToStr(creator));
    // TODO: shouldn't it be up to the front-end to normalize whitespace?
    if (result) {
        // TODO: original code called str::RemoveChars(result, "\n\r\t")
        str::NormalizeWS(result);
    }
    return result.StealData();
}

const char *ChmDoc::GetHomePath()
{
    return homePath;
}

static int ChmEnumerateEntry(struct chmFile *chmHandle, struct chmUnitInfo *info, void *data)
{
    if (info->path) {
        Vec<char *> *paths = (Vec<char *> *)data;
        paths->Append(str::Dup(info->path));
    }
    return CHM_ENUMERATOR_CONTINUE;
}

Vec<char *> *ChmDoc::GetAllPaths()
{
    Vec<char *> *paths = new Vec<char *>();
    chm_enumerate(chmHandle, CHM_ENUMERATE_FILES | CHM_ENUMERATE_NORMAL, ChmEnumerateEntry, paths);
    return paths;
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
static bool VisitChmTocItem(EbookTocVisitor *visitor, HtmlElement *el, UINT cp, int level)
{
    CrashIf(el->tag != Tag_Object || level > 1 && (!el->up || el->up->tag != Tag_Li));

    ScopedMem<WCHAR> name, local;
    for (el = el->GetChildByTag(Tag_Param); el; el = el->next) {
        if (Tag_Param != el->tag)
            continue;
        ScopedMem<WCHAR> attrName(el->GetAttribute("name"));
        ScopedMem<WCHAR> attrVal(el->GetAttribute("value"));
        if (attrName && attrVal && cp != CP_CHM_DEFAULT) {
            ScopedMem<char> bytes(str::conv::ToCodePage(attrVal, CP_CHM_DEFAULT));
            attrVal.Set(str::conv::FromCodePage(bytes, cp));
        }
        if (!attrName || !attrVal)
            /* ignore incomplete/unneeded <param> */;
        else if (str::EqI(attrName, L"Name"))
            name.Set(attrVal.StealData());
        else if (str::EqI(attrName, L"Local")) {
            // remove the ITS protocol and any filename references from the URLs
            if (str::Find(attrVal, L"::/"))
                attrVal.Set(str::Dup(str::Find(attrVal, L"::/") + 3));
            local.Set(attrVal.StealData());
        }
    }
    if (!name)
        return false;

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
static bool VisitChmIndexItem(EbookTocVisitor *visitor, HtmlElement *el, UINT cp, int level)
{
    CrashIf(el->tag != Tag_Object || level > 1 && (!el->up || el->up->tag != Tag_Li));

    WStrVec references;
    ScopedMem<WCHAR> keyword, name;
    for (el = el->GetChildByTag(Tag_Param); el; el = el->next) {
        if (Tag_Param != el->tag)
            continue;
        ScopedMem<WCHAR> attrName(el->GetAttribute("name"));
        ScopedMem<WCHAR> attrVal(el->GetAttribute("value"));
        if (attrName && attrVal && cp != CP_CHM_DEFAULT) {
            ScopedMem<char> bytes(str::conv::ToCodePage(attrVal, CP_CHM_DEFAULT));
            attrVal.Set(str::conv::FromCodePage(bytes, cp));
        }
        if (!attrName || !attrVal)
            /* ignore incomplete/unneeded <param> */;
        else if (str::EqI(attrName, L"Keyword"))
            keyword.Set(attrVal.StealData());
        else if (str::EqI(attrName, L"Name")) {
            name.Set(attrVal.StealData());
            // some CHM documents seem to use a lonely Name instead of Keyword
            if (!keyword)
                keyword.Set(str::Dup(name));
        }
        else if (str::EqI(attrName, L"Local") && name) {
            // remove the ITS protocol and any filename references from the URLs
            if (str::Find(attrVal, L"::/"))
                attrVal.Set(str::Dup(str::Find(attrVal, L"::/") + 3));
            references.Append(name.StealData());
            references.Append(attrVal.StealData());
        }
    }
    if (!keyword)
        return false;

    if (references.Count() == 2) {
        visitor->Visit(keyword, references.At(1), level);
        return true;
    }
    visitor->Visit(keyword, NULL, level);
    for (size_t i = 0; i < references.Count(); i += 2) {
        visitor->Visit(references.At(i), references.At(i + 1), level + 1);
    }
    return true;
}

static void WalkChmTocOrIndex(EbookTocVisitor *visitor, HtmlElement *list, UINT cp, bool isIndex, int level=1)
{
    CrashIf(Tag_Ul != list->tag);

    // some broken ToCs wrap every <li> into its own <ul>
    for (; list && Tag_Ul == list->tag; list = list->next) {
        for (HtmlElement *el = list->down; el; el = el->next) {
            if (Tag_Li != el->tag)
                continue; // ignore unexpected elements

            bool valid;
            HtmlElement *elObj = el->GetChildByTag(Tag_Object);
            if (!elObj)
                valid = false;
            else if (isIndex)
                valid = VisitChmIndexItem(visitor, elObj, cp, level);
            else
                valid = VisitChmTocItem(visitor, elObj, cp, level);
            if (!valid)
                continue; // skip incomplete elements and all their children

            HtmlElement *nested = el->GetChildByTag(Tag_Ul);
            // some broken ToCs have the <ul> follow right *after* a <li>
            if (!nested && el->next && Tag_Ul == el->next->tag)
                nested = el->next;
            if (nested)
                WalkChmTocOrIndex(visitor, nested, cp, isIndex, level + 1);
        }
    }
}

// ignores any <ul><li> list structure and just extracts a linear list of <object type="text/sitemap">...</object>
static bool WalkBrokenChmTocOrIndex(EbookTocVisitor *visitor, HtmlParser& p, UINT cp, bool isIndex)
{
    bool hadOne = false;

    HtmlElement *el = p.FindElementByName("body");
    while ((el = p.FindElementByName("object", el)) != NULL) {
        ScopedMem<WCHAR> type(el->GetAttribute("type"));
        if (!str::EqI(type, L"text/sitemap"))
            continue;
        if (isIndex)
            hadOne |= VisitChmIndexItem(visitor, el, cp, 1);
        else
            hadOne |= VisitChmTocItem(visitor, el, cp, 1);
    }

    return hadOne;
}

bool ChmDoc::ParseTocOrIndex(EbookTocVisitor *visitor, const char *path, bool isIndex)
{
    if (!path)
        return false;
    // TODO: is path already UTF-8 encoded - or do we need str::conv::ToUtf8(ToStr(path)) ?
    ScopedMem<unsigned char> htmlData(GetData(path, NULL));
    const char *html = (char *)htmlData.Get();
    if (!html)
        return false;

    HtmlParser p;
    UINT cp = codepage;
    // detect UTF-8 content by BOM
    if (str::StartsWith(html, UTF8_BOM)) {
        html += 3;
        cp = CP_UTF8;
    }
    // enforce the default codepage, so that pre-encoded text and
    // entities are in the same codepage and VisitChmTocItem yields
    // consistent results
    HtmlElement *el = p.Parse(html, CP_CHM_DEFAULT);
    if (!el)
        return false;
    el = p.FindElementByName("body");
    // since <body> is optional, also continue without one
    el = p.FindElementByName("ul", el);
    if (!el)
        return WalkBrokenChmTocOrIndex(visitor, p, cp, isIndex);
    WalkChmTocOrIndex(visitor, el, cp, isIndex);
    return true;
}

bool ChmDoc::HasToc() const
{
    return tocPath != NULL;
}

bool ChmDoc::ParseToc(EbookTocVisitor *visitor)
{
    return ParseTocOrIndex(visitor, tocPath, false);
}

bool ChmDoc::HasIndex() const
{
    return indexPath != NULL;
}

bool ChmDoc::ParseIndex(EbookTocVisitor *visitor)
{
    return ParseTocOrIndex(visitor, indexPath, true);
}

bool ChmDoc::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff)
        return file::StartsWith(fileName, "ITSF");

    return str::EndsWithI(fileName, L".chm");
}

ChmDoc *ChmDoc::CreateFromFile(const WCHAR *fileName)
{
    ChmDoc *doc = new ChmDoc();
    if (!doc || !doc->Load(fileName)) {
        delete doc;
        return NULL;
    }
    return doc;
}
