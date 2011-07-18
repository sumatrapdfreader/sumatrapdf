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

#define INVALID_SIZE_T (size_t)-1

struct Element {
    char *name;
    char *val;
    size_t firstAttr;
    size_t up, down, next;
};

struct Attr {
    char *name;
    char *val;
    size_t nextAttr;
};

class HtmlParser {
    Vec<Element> elAllocator;
    Vec<Attr> attrAllocator;

    // text to parse. It can be changed.
    char *s;
    size_t slen;

    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeText;

public:
    HtmlParser();
    ~HtmlParser();

    bool Parse(char *s, size_t len=INVALID_SIZE_T);
    bool ParseInPlace(char *s, size_t len=INVALID_SIZE_T);
};

HtmlParser::HtmlParser() : s(NULL), slen(0), freeText(false)
{
}

HtmlParser::~HtmlParser()
{
    if (freeText)
        free(s);
}

bool HtmlParser::Parse(char *txt, size_t len)
{
    if (INVALID_SIZE_T == len)
        len = strlen(txt);
    freeText = true;
    return ParseInPlace(str::DupN(txt, len), len);
}

// Parse txt in place i.e. we assume we can modify txt
// The caller owns the memory for txt.
bool HtmlParser::ParseInPlace(char *txt, size_t len)
{
    if (INVALID_SIZE_T == len)
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
