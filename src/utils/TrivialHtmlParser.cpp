/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"

#include "TrivialHtmlParser.h"
#include "FileUtil.h"
#include "WinUtil.h"

/*
Html parser that is good enough for parsing html files
inside CHM archives. Not meant for general use.

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

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

// 0 if not tag end, 1 if ends with '>' and 2 if ends with "/>"
static int TagEndLen(char *s) {
    if ('>' == *s)
        return 1;
    if ('/' == s[0] && '>' == s[1])
        return 2;
    return 0;
}

static bool IsUnquotedAttrValEnd(char c) {
    return !c || IsWs(c) || c == '/' || c == '>';
}

// TODO: support #x...; hex notation
// TODO: support other HTML entities (such as &uuml; or &ocirc;)?
static void CollapseEntitiesInPlace(char *s)
{
    char *out = s;
    while (*s) {
        if (*s =='&') {
            ++s;
            if (str::StartsWithI(s, "lt;")) {
                s += 3; *out++ = '<';
            } else if (str::StartsWithI(s, "gt;")) {
                s += 3; *out++ = '>';
            } else if (str::StartsWithI(s, "amp;")) {
                s += 4; *out++ = '&';
            } else if (str::StartsWithI(s, "apos;")) {
                s += 5; *out++ = '\'';
            } else  if (str::StartsWithI(s, "quot;")) {
                s += 5; *out++ = '"';
            } else {
                *out++ = s[-1];
            }
        } else {
            *out++ = *s++;
        }
    }
    *out = 0;
}

HtmlParser::HtmlParser()
{
    html = NULL;
    freeHtml = false;
    rootElement = NULL;
    currElement = NULL;
    elementsCount = 0;
    attributesCount = 0;
    error = ErrParsingNoError;
    errorContext= NULL;
}

HtmlParser::~HtmlParser()
{
    if (freeHtml)
        free(html);
}

HtmlAttr *HtmlParser::AllocAttr(char *name)
{
    HtmlAttr *attr = AllocStruct<HtmlAttr>(allocator);
    attr->name = name;
    attr->val = NULL;
    attr->next = NULL;
    ++attributesCount;
    return attr;
}

bool HtmlParser::Parse(const char *s)
{
    freeHtml = true;
    return ParseInPlace(str::Dup(s));
}

HtmlAttr *HtmlParser::GetAttrByName(HtmlElement *el, const char *name) const
{
    HtmlAttr *a = el->firstAttr;
    while (NULL != a) {
        if (str::EqI(name, a->name))
            return a;
        a = a->next;
    }
    return NULL;
}

HtmlElement *HtmlParser::AllocElement(HtmlElement *parent, char *name)
{
    HtmlElement *el = AllocStruct<HtmlElement>(allocator);
    el->name = name;
    el->up = parent;
    el->val = NULL;
    el->firstAttr = NULL;
    el->down = el->next = NULL;
    ++elementsCount;
    return el;
}

void HtmlParser::StartTag(char *tagName)
{
    str::ToLower(tagName);
    HtmlElement *parent = currElement;
    currElement = AllocElement(currElement, tagName);
    if (NULL == rootElement)
        rootElement = currElement;

    if (!parent)
        return;
    if (NULL == parent->down) {
        // parent has no children => set as a first child
        parent->down = currElement;
    } else {
        // parent has children => set as a sibling
        HtmlElement *tmp = parent->down;
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = currElement;
    }
}

void HtmlParser::CloseTag(char *tagName)
{
    str::ToLower(tagName);
    // to allow for lack of closing tags, e.g. in case like
    // <a><b><c></a>, we look for the first parent with matching name
    HtmlElement *el = currElement;
    while (el) {
        if (str::Eq(el->name, tagName)) {
            currElement = el->up;
            return;
        }
        el = el->up;
    }
    // TODO: should we do sth. here?
}

void HtmlParser::StartAttr(char *name)
{
    str::ToLower(name);
    HtmlAttr *a = AllocAttr(name);
    if (NULL == currElement->firstAttr) {
        currElement->firstAttr = a;
        return;
    }
    HtmlAttr *tmp = currElement->firstAttr;
    while (NULL != tmp->next) {
        tmp = tmp->next;
    }
    tmp->next = a;
}

void HtmlParser::SetAttrVal(char *val)
{
    CollapseEntitiesInPlace(val);
    HtmlAttr *a = currElement->firstAttr;
    while (NULL != a->next) {
        a = a->next;
    }
    a->val = val;
}

static char *ParseAttrValue(char **sPtr)
{
    char *attrVal = NULL;
    char *s = *sPtr;
    SkipWs(&s);
    char quoteChar = *s;
    if (quoteChar == '"' || quoteChar == '\'') {
        ++s; attrVal = s;
        SkipUntil(&s, quoteChar);
        if (*s != quoteChar)
            return NULL;
        *s++ = 0;
        goto Exit;
    } else {
        attrVal = s;
        while (!IsUnquotedAttrValEnd(*s)) {
            ++s;
        }
        if (IsWs(*s) || TagEndLen(s) > 0) {
            if (IsWs(*s))
                *s = 0;
            goto Exit;
        }
        return NULL;
    }
Exit:
    *sPtr = s;
    return attrVal;
}

static char *ParseAttrName(char **sPtr)
{
    char *s = *sPtr;
    char *attrName = s;
    SkipName(&s);
    char *attrNameEnd = s;
    SkipWs(&s);
    if (*s != '=')
        return NULL;
    *attrNameEnd = 0; // terminate attribute name
    *sPtr = ++s;
    return attrName;
}

// Parse s in place i.e. we assume we can modify it. Must be 0-terminated.
// The caller owns the memory for s.
bool HtmlParser::ParseInPlace(char *s)
{
    char *tagName, *attrName, *attrVal, *tagEnd;
    int tagEndLen;
    bool ok;

    html = s;
ParseText:
    ok = SkipUntil(&s, '<');
    if (!ok) {
        // Note: I think we can be in an inconsistent state here 
        // (unclosed tags) but not sure if we should care
        return true;
    }
    // TODO: if within a tag, set this as tag value
    // Note: even then it won't handle cases where value
    // spans multiple parts as in:
    // "<a>foo<b/>bar</a>", where value of tag a should be "foobar"
    ++s;

    if (*s == '!' || *s == '?') {
        ++s;
        goto ParseExclOrPi;
    }

    if (*s == '/') {
        ++s;
        goto ParseClosingElement;
    }

    // parse element name
    errorContext = s;
    SkipWs(&s);
    if (!IsName(*s))
        return ParseError(ErrParsingElement);
    tagName = s;
    SkipName(&s);
    tagEnd = s;
    SkipWs(&s);
    tagEndLen = TagEndLen(s);
    if (tagEndLen > 0) {
        *tagEnd = 0;
        StartTag(tagName);
        if (tagEndLen == 2)
            CloseTag(tagName);
        s += tagEndLen;
        goto ParseText;
    }
    if (IsWs(*tagEnd)) {
        *tagEnd = 0;
        StartTag(tagName);
        goto ParseAttributes;
    }
    return ParseError(ErrParsingElementName);

ParseClosingElement: // "</"
    errorContext = s;
    SkipWs(&s);
    if (!IsName(*s))
        return ParseError(ErrParsingClosingElement);
    tagName = s;
    SkipName(&s);
    tagEnd = s;
    SkipWs(&s);
    if (*s != '>')
        return ParseError(ErrParsingClosingElement);
    *tagEnd = 0;
    CloseTag(tagName);
    ++s;
    goto ParseText;

ParseAttributes:
    errorContext = s;
    SkipWs(&s);
    if (IsName(*s))
        goto ParseAttributeName;
    tagEndLen = TagEndLen(s);
    if (0 == tagEndLen)
        return ParseError(ErrParsingAttributes);

FoundElementEnd:
    if (tagEndLen == 2)
        CloseTag(tagName);
    s += tagEndLen;
    goto ParseText;

ParseAttributeName:
    errorContext = s;
    attrName = ParseAttrName(&s);
    if (!attrName)
        return ParseError(ErrParsingAttributeName);
    StartAttr(attrName);

    // parse attribute value
    errorContext = s;
    attrVal = ParseAttrValue(&s);
    if (!attrVal)
        return ParseError(ErrParsingAttributeValue);
    tagEndLen = TagEndLen(s);
    if (tagEndLen > 0) {
        *s = 0;
        SetAttrVal(attrVal);
        goto FoundElementEnd;
    }
    SetAttrVal(attrVal);
    s++;
    goto ParseAttributes;

ParseExclOrPi: // "<!" or "<?"
    // might be a <!DOCTYPE ..>, a <!-- comment ->, a <? processing instruction >
    // or really anything. We're very lenient and consider it a success 
    // if we find a terminating '>'
    errorContext = s;
    ok = SkipUntil(&s, '>');
    if (!ok)
        return ParseError(ErrParsingExclOrPI);
    ++s;
    goto ParseText;
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
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    delete p;
}

static void HtmlParser01()
{
    HtmlParser *p = ParseString("<A><bAh></a>");
    assert(p->ElementsCount() == 2);
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    assert(NULL == el->up);
    assert(NULL == el->next);
    el = el->down;
    assert(NULL == el->firstAttr);
    assert(str::Eq("bah", el->name));
    assert(el->up == p->GetRootElement());
    assert(NULL == el->down);
    assert(NULL == el->next);
    delete p;
}

static void HtmlParser05()
{
    HtmlParser *p = ParseString("<!doctype><html><HEAD><meta name=foo></head><body><object t=la><param name=foo val=bar></object><ul><li></ul></object></body></Html>");
    assert(8 == p->ElementsCount());
    assert(4 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("html", el->name));
    assert(NULL == el->up);
    assert(NULL == el->next);
    el = el->down;
    assert(str::Eq("head", el->name));
    HtmlElement *el2 = el->down;
    assert(str::Eq("meta", el2->name));
    assert(NULL == el2->next);
    assert(NULL == el2->down);
    el2 = el->next;
    assert(str::Eq("body", el2->name));
    assert(NULL == el2->next);
    el2 = el2->down;
    assert(str::Eq("object", el2->name));
    delete p;
}

static void HtmlParser04()
{
    HtmlParser *p = ParseString("<el att=  va&apos;l></ el >");
    assert(1 == p->ElementsCount());
    assert(1 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("el", el->name));
    assert(NULL == el->next);
    assert(NULL == el->up);
    assert(NULL == el->down);
    HtmlAttr *a = el->firstAttr;
    assert(str::Eq("att", a->name));
    assert(str::Eq("va'l", a->val));
    assert(NULL == a->next);
    delete p;
}

static void HtmlParser03()
{
    HtmlParser *p = ParseString("<el   att  =v&quot;al/>");
    assert(1 == p->ElementsCount());
    assert(1 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("el", el->name));
    assert(NULL == el->next);
    assert(NULL == el->up);
    assert(NULL == el->down);
    HtmlAttr *a = el->firstAttr;
    assert(str::Eq("att", a->name));
    assert(str::Eq("v\"al", a->val));
    assert(NULL == a->next);
    delete p;
}

static void HtmlParser02()
{
    HtmlParser *p = ParseString("<a><b/><  c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=end/></a>");
    assert(4 == p->ElementsCount());
    assert(4 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    assert(NULL == el->next);
    el = el->down;
    assert(str::Eq("b", el->name));
    assert(p->GetRootElement() == el->up);
    el = el->next;
    assert(str::Eq("c", el->name));
    assert(p->GetRootElement() == el->up);
    el = el->next;
    assert(str::Eq("d", el->name));
    assert(NULL == el->next);
    assert(p->GetRootElement() == el->up);
    HtmlAttr *a = el->firstAttr;
    assert(str::Eq("at1", a->name));
    assert(str::Eq("<quo&ted>", a->val));
    a = a->next;
    assert(str::Eq("at2", a->name));
    assert(str::Eq("also quoted", a->val));
    a = a->next;
    assert(str::Eq("att3", a->name));
    assert(str::Eq("notquoted", a->val));
    a = a->next;
    assert(str::Eq("att4", a->name));
    assert(str::Eq("end", a->val));
    assert(NULL == a->next);
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
    assert(ok);
    assert(709 == p.ElementsCount());
    assert(955 == p.TotalAttrCount());
    HtmlElement *el = p.GetRootElement();
    assert(str::Eq(el->name, "html"));
    el = el->down;
    assert(str::Eq(el->name, "head"));
    el = el->next;
    assert(str::Eq(el->name, "body"));
    el = el->down;
    assert(str::Eq(el->name, "object"));
    el = el->next;
    assert(str::Eq(el->name, "ul"));
    el = el->down;
    assert(str::Eq(el->name, "li"));
    el = el->down;
    assert(str::Eq(el->name, "object"));
    HtmlAttr *a = p.GetAttrByName(el, "type");
    assert(str::Eq(a->name, "type"));
    assert(str::Eq(a->val, "text/sitemap"));
    free(d);
}
}

void TrivialHtmlParser_UnitTests()
{
    unittests::HtmlParserFile();
    unittests::HtmlParser05();
    unittests::HtmlParser04();
    unittests::HtmlParser03();
    unittests::HtmlParser02();
    unittests::HtmlParser00();
    unittests::HtmlParser01();
}
#endif
