/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"

#include "TrivialHtmlParser.h"
#include "Vec.h"
#include "FileUtil.h"
#include "WinUtil.h"

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

static const HtmlElementId RootElementId = 0;

enum HtmlParseError {
    ErrParsingNoError,
    ErrParsingElement, // syntax error parsing element
    ErrParsingExclOrPI,
    ErrParsingClosingElement, // syntax error in closing element
    ErrParsingElementName, // syntax error after element name
    ErrParsingAttributes, // syntax error in attributes
    ErrParsingAttributeName, // syntax error after attribute name
    ErrParsingAttributeValue1,
    ErrParsingAttributeValue2,
};

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

    HtmlElementId currElementId;

    HtmlElementId AllocElement(HtmlElementId parentId, char *name, HtmlElement **elOut);
    HtmlAttrId AllocAttr(HtmlAttr **attrOut);

    void CloseTag(char *tagName);
    void StartTag(char *tagName);
    void StartAttr(char *attrName);
    void SetAttrVal(char *attrVal);
    bool ParseError(HtmlParseError err) {
        error = err;
        return false;
    }

public:
    HtmlParseError error;  // parsing error, a static string
    char *errorContext; // pointer within html showing which part we failed to parse

    HtmlParser();
    ~HtmlParser();

    bool Parse(const char *s);
    bool ParseInPlace(char *s);

    HtmlElement *GetRootElement() const {
        return GetElement(RootElementId);
    }
    HtmlElement *GetElement(HtmlElementId id) const;
    size_t ElementsCount() const {
        return elAllocator.Count();
    }
    HtmlAttr *GetAttr(HtmlAttrId id) const;

    size_t TotalAttrCount() const {
        return attrAllocator.Count();
    }
    char *GetElementName(HtmlElementId id) const;
    char *GetAttrName(HtmlAttrId id) const;
    HtmlElementId GetParent(HtmlElementId id) const;
    size_t GetSiblingCount(HtmlElementId id) const;
    HtmlElementId GetSibling(HtmlElementId id, size_t siblingNo) const;
};

char *HtmlParser::GetElementName(HtmlElementId id) const
{
    HtmlElement *el = elAllocator.AtPtr(id);
    return el->name;
}

char *HtmlParser::GetAttrName(HtmlAttrId id) const
{
    HtmlAttr *attr = attrAllocator.AtPtr(id);
    return attr->name;
}

HtmlElementId HtmlParser::GetParent(HtmlElementId id) const
{
    HtmlElement *el = elAllocator.AtPtr(id);
    return el->up;
}

size_t HtmlParser::GetSiblingCount(HtmlElementId id) const
{
    assert(0); // TODO: write me
    return 0;
}

HtmlElementId HtmlParser::GetSibling(HtmlElementId id, size_t siblingNo) const
{
    assert(0); // TODO: write me
    return NO_ID;
}


HtmlParser::HtmlParser() : 
    html(NULL), freeHtml(false), currElementId(NO_ID),
    error(ErrParsingNoError), errorContext(NULL)
{
}

HtmlParser::~HtmlParser()
{
    if (freeHtml)
        free(html);
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

// only valid until next AllocAttr()
HtmlAttr *HtmlParser::GetAttr(HtmlAttrId id) const
{
    if ((NO_ID == id) || (attrAllocator.Count() <= id))
        return NULL;
    return attrAllocator.AtPtr(id);
}

bool HtmlParser::Parse(const char *s)
{
    freeHtml = true;
    return ParseInPlace(str::Dup(s));
}

static bool IsWs(int c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static int IsName(int c)
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

static bool SkipUntil(char **sPtr, char c)
{
    char *s = *sPtr;
    while (*s && (*s != c)) {
        ++s;
    }
    *sPtr = s;
    return *s == c;
}

// only valid until next AllocElement()
HtmlElement *HtmlParser::GetElement(HtmlElementId id) const
{
    if ((NO_ID == id) || (elAllocator.Count() <= id))
        return NULL;
    return elAllocator.AtPtr(id);
}

// elOut is only valid until next AllocElement()
// TODO: move the code from AllocElement() to StartTag()
HtmlElementId HtmlParser::AllocElement(HtmlElementId parentId, char *name, HtmlElement **elOut)
{
    HtmlElementId id = elAllocator.Count();
    HtmlElement *el = elAllocator.MakeSpaceAtEnd();
    InitHtmlElement(el);
    el->up = parentId;
    if (parentId != NO_ID) {
        HtmlElement *parentEl = GetElement(parentId);
        if (parentEl->down == NO_ID) {
            // parent has no children => set as a first child
            parentEl->down = id;
        } else {
            // parent has children => set as a sibling
            el = GetElement(parentEl->down);
            while (el->next != NO_ID) {
                el = GetElement(el->next);
            }
            el->next = id;
        }
    }
    el->name = name;
    if (elOut)
        *elOut = el;
    return id;
}

void HtmlParser::StartTag(char *tagName)
{
    str::ToLower(tagName);
    currElementId = AllocElement(currElementId, tagName, NULL);
}

void HtmlParser::CloseTag(char *tagName)
{
    str::ToLower(tagName);
    HtmlElementId id = currElementId;
    // to allow for lack of closing tags, e.g. in case like
    // <a><b><c></a>, we look for the first parent with matching name
    while (id != NO_ID) {
        HtmlElement *el = GetElement(id);
        if (str::Eq(el->name, tagName)) {
            // TODO: what if el->up is NO_ID?
            currElementId = el->up;
            return;
        }
        id = el->up;
    }
    // TODO: should we do sth. here?
}

void HtmlParser::StartAttr(char *attrName)
{
    str::ToLower(attrName);
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
    char *tagName, *attrName, *attrVal, *tmp;
    char quoteChar;
    bool ok;

    html = s;
ParseText:
    ok = SkipUntil(&s, '<');
    if (!ok) {
        // TODO: I think we can be in an inconsistent state here (unclosed tags)
        // but not sure if we should care
        return true;
    }
    ++s;

    // parsing element
    if (*s == '/') {
        ++s;
        goto ParseClosingElement;
    }
    if (*s == '!' || *s == '?') {
        ++s;
        goto ParseExclOrPi;
    }
    errorContext = s;
    SkipWs(&s);
    if (IsName(*s))
        goto ParseElementName;
    return ParseError(ErrParsingElement);

ParseExclOrPi:
    // Parse anything that starts with <!, or <?
    // this might be a <!DOCTYPE ..>, a <!-- comment ->, a <? processing instruction >
    // or really anything. We're very lenient and consider it a success if we
    // find a terminating '>'
    errorContext = s;
    ok = SkipUntil(&s, '>');
    if (!ok)
        return ParseError(ErrParsingExclOrPI);
    ++s;
    goto ParseText;

ParseClosingElement:
    errorContext = s;
    SkipWs(&s);
    tagName = s;
    SkipName(&s);
    tmp = s;
    SkipWs(&s);
    if (*s != '>')
        return ParseError(ErrParsingClosingElement);
    *tmp = 0; // terminate tag name
    CloseTag(tagName);
    ++s;
    goto ParseText;

ParseElementName:
    tagName = s;
    errorContext = s;
    SkipName(&s);
    if (*s == '>') {
        *s = 0; // terminate tag name
        StartTag(tagName);
        ++s;
        goto ParseText;
    }
    if (*s == '/' && s[1] == '>') {
        *s = 0;
        StartTag(tagName);
        CloseTag(tagName);
        s += 2;
        goto ParseText;
    }
    if (IsWs(*s)) {
        *s = 0; // terminate tag name
        s++;
        goto ParseAttributes;
    }
    return ParseError(ErrParsingElementName);

ParseAttributes:
    errorContext = s;
    SkipWs(&s);
    if (IsName(*s))
        goto ParseAttributeName;
    if (*s == '>') {
        ++s;
        goto ParseText;
    }
    if (*s == '/' && s[1] == '>') {
        CloseTag(tagName);
        s += 2;
        goto ParseText;
    }
    return ParseError(ErrParsingAttributes);

ParseAttributeName:
    attrName = s;
    errorContext = s;
    SkipName(&s);
    tmp = s;
    SkipWs(&s);
    if (*s == '=') {
        *tmp = 0; // terminate attribute name
        StartAttr(attrName);
        ++s;
        goto ParseAttributeValue;
    }
    return ParseError(ErrParsingAttributeName);

ParseAttributeValue:
    errorContext = s;
    SkipWs(&s);
    quoteChar = *s++;
    // TODO: relax quoting rules
    if (quoteChar != '"' && quoteChar != '\'')
        return ParseError(ErrParsingAttributeValue1);
    attrVal = s;
    while (*s && *s != quoteChar) {
        ++s;
    }
    if (*s == quoteChar) {
        *s++ = 0; // terminate attribute value
        SetAttrVal(attrVal);
        goto ParseAttributes;
    }
    return ParseError(ErrParsingAttributeValue2);
}

#ifdef DEBUG
#include <assert.h>

namespace unittests {
static HtmlParser *ParseString(const char *s)
{
    HtmlParser *p = new HtmlParser();
    bool ok = p->Parse(s);
    if (!ok) {
        delete p;
        return NULL;
    }
    return p;
}

static void HtmlParser00()
{
    HtmlParser *p = ParseString("<a></A>");
    assert(p);
    assert(p->ElementsCount() == 1);
    assert(str::Eq("a", p->GetElementName(RootElementId)));
    delete p;
}

static void HtmlParser01()
{
    HtmlParser *p = ParseString("<A><bAh></a>");
    assert(p->ElementsCount() == 2);
    HtmlElement *el = p->GetElement(RootElementId);
    assert(str::Eq("a", el->name));
    assert(NO_ID == el->up);
    assert(NO_ID == el->next);
    el = p->GetElement(el->down);
    assert(NO_ID == el->firstAttrId);
    assert(str::Eq("bah", el->name));
    assert(el->up == RootElementId);
    assert(NO_ID == el->down);
    assert(NO_ID == el->next);
    delete p;
}

static void HtmlParser02()
{
    HtmlParser *p = ParseString("<a><b/><  c></c  ><d at1=\"quoted\" at2='also quoted'   att3=notquoted att4=\"partially quoted/></a>");
    assert(4 == p->ElementsCount());
    assert(4 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    assert(NO_ID == el->next);
    el = p->GetElement(el->down);
    assert(str::Eq("b", el->name));
    assert(RootElementId == el->up);
    el = p->GetElement(el->next);
    assert(str::Eq("c", el->name));
    assert(RootElementId == el->up);
    el = p->GetElement(el->next);
    assert(str::Eq("d", el->name));
    assert(NO_ID == el->next);
    assert(RootElementId == el->up);
    HtmlAttr *a = p->GetAttr(el->firstAttrId);
    assert(str::Eq("at1", a->name));
    assert(str::Eq("quoted", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("at2", a->name));
    assert(str::Eq("also quoted", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("att3", a->name));
    assert(str::Eq("notquoted", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("att4", a->name));
    assert(str::Eq("partially quoted", a->val));
    assert(NO_ID == a->nextAttrId);
    delete p;
}

static void HtmlParserFile()
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
}

void TrivialHtmlParser_UnitTests()
{
    //unittests::HtmlParser02();
    unittests::HtmlParser00();
    unittests::HtmlParser01();
    //unittests::HtmlParserFile();
}
#endif
