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
    char *html;

    // true if s was allocated by ourselves, false if managed
    // by the caller
    bool freeHtml;

    HtmlElementId AllocElement(HtmlElement **elOut);
    HtmlAttrId AllocAttr(HtmlAttr **attrOut);

    HtmlElement *GetElement(HtmlElementId id);
    HtmlAttr *GetAttr(HtmlAttrId id);

    void CloseTag(char *tagName);
    void StartTag(char *tagName);
    void StartAttr(char *attrName);
    void SetAttrVal(char *attrVal);

public:
    char *error;  // parsing error, a static string
    char *errorPos;

    HtmlParser();
    ~HtmlParser();

    bool Parse(char *s);
    bool ParseInPlace(char *s);
    bool ParseError(char *err, char *pos) {
        error = err;
        errorPos = pos;
        return false;
    }
};

HtmlParser::HtmlParser() : html(NULL), freeHtml(false), error(NULL)
{
}

HtmlParser::~HtmlParser()
{
    if (freeHtml)
        free(html);
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

bool HtmlParser::Parse(char *s)
{
    freeHtml = true;
    return ParseInPlace(str::Dup(s));
}

static inline bool IsWs(int c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static inline int IsName(int c)
{
    return c == '.' || c == '-' || c == '_' || c == ':' ||
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

static void SkipWs(char **sPtr)
{
    char *s = *sPtr;
    while (IsWs(*s)) {
        s++;
    }
    *sPtr = s;
}

static void SkipName(char **sPtr)
{
    char *s = *sPtr;
    while (IsName(*s)) {
        s++;
    }
    *sPtr = s;
}

void HtmlParser::CloseTag(char *tagName)
{
    // TODO: write me
}

void HtmlParser::StartTag(char *tagName)
{
    // TODO: write me
}

void HtmlParser::StartAttr(char *attrName)
{
    // TODO: write me
}

void HtmlParser::SetAttrVal(char *attrVal)
{
    // TODO: write me
}

// Parse s in place i.e. we assume we can modify it. Must be 0-terminated.
// The caller owns the memory for s.
bool HtmlParser::ParseInPlace(char *s)
{
    char *mark, *tmp;
    char quoteChar;
    html = s;

ParseText:
    mark = s;
    while (*s != '<') {
        ++s;
    }
    if (*s == '<') {
        ++s;
        goto ParseElement;
    }
    return true;

ParseElement:
    if (*s == '/') {
        ++s;
        goto ParseClosingElement;
    }
    if (*s == '!') {
        ++s;
        goto ParseComment;
    }
    if (*s == '?') {
        ++s;
        goto ParsePi;
    }
    SkipWs(&s);
    if (IsName(*s))
        goto ParseElementName;
    return ParseError("syntax error in element", s);

ParseComment:
    if (*s == '[')
        goto ParseCdata;
    if (*s++ != '-')
        return ParseError("syntax error in comment (<! not followed by --)", s);
    if (*s++ != '-')
        return ParseError("syntax error in comment (<!- not followed by -),", s);
    mark = s;
    while (*s) {
        if (str::StartsWith(s, "-->")) {
            s += 3;
            goto ParseText;
        }
        ++s;
    }
    return ParseError("end of data in comment", s);

ParseCdata:
    if (!str::StartsWith(s, "CDATA["))
        return ParseError("syntax error in CDATA section", s);
    s += 7;
    mark = s;
    while (*s) {
        if (*s == ']' && s[1] == ']' && s[2] == '>') {
            s += 3;
            goto ParseText;
        }
        ++s;
    }
    return ParseError("end of data in CDATA section", s);

ParsePi:
    while (*s) {
        if (str::StartsWith(s, "?>")) {
            s += 2;
            goto ParseText;
        }
        ++s;
    }
    return ParseError("end of data in processing instruction", s);

ParseClosingElement:
    SkipWs(&s);
    mark = s;
    SkipName(&s);
    tmp = s;
    SkipWs(&s);
    if (*s != '>')
        return ParseError("syntax error in closing element", s);
    *tmp = 0; // terminate tag name
    CloseTag(mark);
    ++s;
    goto ParseText;

ParseElementName:
    mark = s;
    SkipName(&s);
    if (*s == '>') {
        *s = 0; // terminate tag name
        StartTag(mark);
        ++s;
        goto ParseText;
    }
    if (*s == '/' && s[1] == '>') {
        *s = 0;
        StartTag(mark);
        CloseTag(mark);
        s += 2;
        goto ParseText;
    }
    if (IsWs(*s)) {
        *s = 0; // terminate tag name
        s++;
        goto ParseAttributes;
    }
    return ParseError("syntax error after element name", s);

ParseAttributes:
    SkipWs(&s);
    if (IsName(*s))
        goto ParseAttributeName;
    if (*s == '>') {
        ++s;
        goto ParseText;
    }
    if (*s == '/' && s[1] == '>') {
        CloseTag(mark);
        s += 2;
        goto ParseText;
    }
    return ParseError("syntax error in attributes", s);

ParseAttributeName:
    mark = s;
    SkipName(&s);
    tmp = s;
    SkipWs(&s);
    if (*s == '=') {
        *tmp = 0; // terminate attribute name
        StartAttr(mark);
        ++s;
        goto ParseAttributeValue;
    }
    return ParseError("syntax error after attribute name", s);

ParseAttributeValue:
    SkipWs(&s);
    quoteChar = *s++;
    // TODO: relax quoting rules
    if (quoteChar != '"' && quoteChar != '\'')
        return ParseError("missing quote character", s);
    mark = s;
    while (*s && *s != quoteChar) {
        ++s;
    }
    if (*s == quoteChar) {
        *s++ = 0; // terminate attribute value
        SetAttrVal(mark);
        goto ParseAttributes;
    }
    return ParseError("end of data in attribute value", s);
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
    char *d = file::ReadAll(p2, NULL);
    // it's ok if we fail - we assume we were not run from the
    // right location
    if (!d)
        return;
    HtmlParser p;
    bool ok = p.ParseInPlace(d);
    // assert(ok);
    free(d);
}
#endif
