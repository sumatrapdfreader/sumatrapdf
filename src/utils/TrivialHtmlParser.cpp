/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"

#include "TrivialHtmlParser.h"
#include "Vec.h"
#include "FileUtil.h"
#include "AppTools.h"

/*
Html parser that is good enough for parsing html files
inside CHM archives. Not meant for general use.

Element and Attr nodes are allocated from a corresponding
Vec so we refer to other Element and Attr nodes by their
index within Vec and not by pointer (we can't use a pointer
because reallocating underlying Vec storage might change
where data lives in memory).

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

#define NO_LEN (size_t)-1

typedef size_t HtmlElementId;
typedef size_t HtmlAttrId;

#define NO_ID (size_t)-1

struct HtmlElement {
    char *name;
    char *val;
    HtmlAttrId firstAttrId;
    HtmlElementId up, down, next;
};

static void InitHtmlElement(HtmlElement *el)
{
    el->name = NULL;
    el->val = NULL;
    el->firstAttrId = NO_ID;
    el->up = el->down = el->next = NO_ID;
}

struct HtmlAttr {
    char *name;
    char *val;
    HtmlAttrId nextAttrId;
};

static void InitHtmlAttr(HtmlAttr *attr)
{
    attr->name = NULL;
    attr->val = NULL;
    attr->nextAttrId = NO_ID;
}

class HtmlParser {
    Vec<HtmlElement> elAllocator;
    Vec<HtmlAttr> attrAllocator;

    // text to parse. It can be changed.
    char *s;
    size_t slen;

    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeText;

    HtmlElementId AllocElement(HtmlElement **elOut);
    HtmlAttrId AllocAttr(HtmlAttr **attrOut);

    HtmlElement *GetElement(HtmlElementId id);
    HtmlAttr *GetAttr(HtmlAttrId id);
public:
    HtmlParser();
    ~HtmlParser();

    bool Parse(char *s, size_t len=NO_LEN);
    bool ParseInPlace(char *s, size_t len=NO_LEN);
};

HtmlParser::HtmlParser() : s(NULL), slen(0), freeText(false)
{
}

HtmlParser::~HtmlParser()
{
    if (freeText)
        free(s);
}

// elOut is only valid until next AllocElement()
HtmlElementId HtmlParser::AllocElement(HtmlElement **elOut)
{
    HtmlElement *el = elAllocator.MakeSpaceAtEnd();
    InitHtmlElement(el);
    if (elOut)
        *elOut = el;
    return elAllocator.Count() - 1;
}

// attrOut is only valid until next AllocAttr()
HtmlAttrId HtmlParser::AllocAttr(HtmlAttr **attrOut)
{
    HtmlAttr *attr = attrAllocator.MakeSpaceAtEnd();
    InitHtmlAttr(attr);
    if (attrOut)
        *attrOut = attr;
    return attrAllocator.Count() - 1;
}

// only valid until next AllocElement()
HtmlElement *HtmlParser::GetElement(HtmlElementId id)
{
    assert(NO_ID != id);
    return elAllocator.AtPtr(id);
}

// only valid until next AllocAttr()
HtmlAttr *HtmlParser::GetAttr(HtmlAttrId id)
{
    assert(NO_ID != id);
    return attrAllocator.AtPtr(id);
}

bool HtmlParser::Parse(char *txt, size_t len)
{
    if (NO_LEN == len)
        len = strlen(txt);
    freeText = true;
    return ParseInPlace(str::DupN(txt, len), len);
}

// Parse txt in place i.e. we assume we can modify txt
// The caller owns the memory for txt.
bool HtmlParser::ParseInPlace(char *txt, size_t len)
{
    if (NO_LEN == len)
        len = strlen(txt);
    s = txt;
    slen = len;

    return false;
}

#ifdef DEBUG
void TrivialHtmlParser_UnitTests()
{
    TCHAR *fileName = _T("HtmlParseTest00.html");
    // We assume we're being run from obj-[dbg|rel], so the test
    // files are in ..\src\utils directory relative to exe's dir
    ScopedMem<TCHAR> exePath(GetExePath());
    const TCHAR *exeDir = path::GetBaseName(exePath);
    ScopedMem<TCHAR> p1(path::Join(exeDir, _T("..\\src\\utils")));
    ScopedMem<TCHAR> p2(path::Join(p1, fileName));
    size_t fileSize;
    char *d = file::ReadAll(p2, &fileSize);
    // it's ok if we fail - we assume we were not run from the
    // right location
    if (!d)
        return;
    HtmlParser p;
    bool ok = p.ParseInPlace(d, fileSize);
    // assert(ok);
    free(d);
}
#endif
