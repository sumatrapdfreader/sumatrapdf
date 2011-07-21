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

Element and Attr nodes are allocated from a corresponding
Vec so we refer to other Element and Attr nodes by their
index within Vec and not by pointer (we can't use a pointer
because reallocating underlying Vec storage might change
where data lives in memory).

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

#define NO_ID (size_t)-1

static void InitHtmlElement(HtmlElement *el)
{
    el->name = NULL;
    el->val = NULL;
    el->firstAttrId = NO_ID;
    el->up = el->down = el->next = NO_ID;
}

static void InitHtmlAttr(HtmlAttr *attr)
{
    attr->name = NULL;
    attr->val = NULL;
    attr->nextAttrId = NO_ID;
}

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

// only valid until next AllocElement()
HtmlElement *HtmlParser::GetElement(HtmlElementId id) const
{
    if ((NO_ID == id) || (elAllocator.Count() <= id))
        return NULL;
    return elAllocator.AtPtr(id);
}

HtmlAttr *HtmlParser::GetAttrByName(HtmlElement *el, const char *name) const
{
    HtmlAttr *a = GetAttr(el->firstAttrId);
    while (NULL != a) {
        if (str::EqI(name, a->name))
            return a;
        a = GetAttr(a->nextAttrId);
    }
    return NULL;
}

// elOut is only valid until next AllocElement()
// TODO: move the code from AllocElement() to StartTag()
HtmlElementId HtmlParser::AllocElement(HtmlElementId parentId, char *name, HtmlElement **elOut)
{
    HtmlElementId id = elAllocator.Count();
    HtmlElement *el = elAllocator.MakeSpaceAtEnd();
    InitHtmlElement(el);
    el->name = name;
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
    // to allow for lack of closing tags, e.g. in case like
    // <a><b><c></a>, we look for the first parent with matching name
    HtmlElementId elId = currElementId;
    HtmlElement *el = GetElement(elId);
    while (el) {
        if (str::Eq(el->name, tagName)) {
            currElementId = el->up;
            return;
        }
        elId = el->up;
        el = GetElement(elId);
    }
    // TODO: should we do sth. here?
}

void HtmlParser::StartAttr(char *attrName)
{
    str::ToLower(attrName);
    HtmlAttr *attr;
    HtmlAttrId id = AllocAttr(&attr);
    InitHtmlAttr(attr);
    attr->name = attrName;
    HtmlElement *currEl = GetElement(currElementId);
    if (NO_ID == currEl->firstAttrId) {
        currEl->firstAttrId = id;
        return;
    }
    attr = GetAttr(currEl->firstAttrId);
    while (NO_ID != attr->nextAttrId) {
        attr = GetAttr(attr->nextAttrId);
    }
    attr->nextAttrId = id;
}

void HtmlParser::SetAttrVal(char *attrVal)
{
    HtmlElement *currEl = GetElement(currElementId);
    CollapseEntitiesInPlace(attrVal);
    HtmlAttr *a = GetAttr(currEl->firstAttrId);
    while (NO_ID != a->nextAttrId) {
        a = GetAttr(a->nextAttrId);
    }
    a->val = attrVal;
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
FoundElementEnd:
    if (tagEndLen > 0) {
        if (tagEndLen == 2)
            CloseTag(tagName);
        s += tagEndLen;
        goto ParseText;
    }
    return ParseError(ErrParsingAttributes);

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
    assert(str::Eq("a", p->GetElementName(HtmlParser::RootElementId)));
    delete p;
}

static void HtmlParser01()
{
    HtmlParser *p = ParseString("<A><bAh></a>");
    assert(p->ElementsCount() == 2);
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    assert(NO_ID == el->up);
    assert(NO_ID == el->next);
    el = p->GetElement(el->down);
    assert(NO_ID == el->firstAttrId);
    assert(str::Eq("bah", el->name));
    assert(el->up == HtmlParser::RootElementId);
    assert(NO_ID == el->down);
    assert(NO_ID == el->next);
    delete p;
}

static void HtmlParser05()
{
    HtmlParser *p = ParseString("<!doctype><html><HEAD><meta name=foo></head><body><object t=la><param name=foo val=bar></object><ul><li></ul></object></body></Html>");
    assert(8 == p->ElementsCount());
    assert(4 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("html", el->name));
    assert(NO_ID == el->up);
    assert(NO_ID == el->next);
    el = p->GetElement(el->down);
    assert(str::Eq("head", el->name));
    HtmlElement *el2 = p->GetElement(el->down);
    assert(str::Eq("meta", el2->name));
    assert(NO_ID == el2->next);
    assert(NO_ID == el2->down);
    el2 = p->GetElement(el->next);
    assert(str::Eq("body", el2->name));
    assert(NO_ID == el2->next);
    el2 = p->GetElement(el2->down);
    assert(str::Eq("object", el2->name));
}

static void HtmlParser04()
{
    HtmlParser *p = ParseString("<el att=  va&apos;l></ el >");
    assert(1 == p->ElementsCount());
    assert(1 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("el", el->name));
    assert(NO_ID == el->next);
    assert(NO_ID == el->up);
    assert(NO_ID == el->down);
    HtmlAttr *a = p->GetAttr(el->firstAttrId);
    assert(str::Eq("att", a->name));
    assert(str::Eq("va'l", a->val));
    assert(NO_ID == a->nextAttrId);
}

static void HtmlParser03()
{
    HtmlParser *p = ParseString("<el   att  =v&quot;al/>");
    assert(1 == p->ElementsCount());
    assert(1 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("el", el->name));
    assert(NO_ID == el->next);
    assert(NO_ID == el->up);
    assert(NO_ID == el->down);
    HtmlAttr *a = p->GetAttr(el->firstAttrId);
    assert(str::Eq("att", a->name));
    assert(str::Eq("v\"al", a->val));
    assert(NO_ID == a->nextAttrId);
}

static void HtmlParser02()
{
    HtmlParser *p = ParseString("<a><b/><  c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=end/></a>");
    assert(4 == p->ElementsCount());
    assert(4 == p->TotalAttrCount());
    HtmlElement *el = p->GetRootElement();
    assert(str::Eq("a", el->name));
    assert(NO_ID == el->next);
    el = p->GetElement(el->down);
    assert(str::Eq("b", el->name));
    assert(HtmlParser::RootElementId == el->up);
    el = p->GetElement(el->next);
    assert(str::Eq("c", el->name));
    assert(HtmlParser::RootElementId == el->up);
    el = p->GetElement(el->next);
    assert(str::Eq("d", el->name));
    assert(NO_ID == el->next);
    assert(HtmlParser::RootElementId == el->up);
    HtmlAttr *a = p->GetAttr(el->firstAttrId);
    assert(str::Eq("at1", a->name));
    assert(str::Eq("<quo&ted>", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("at2", a->name));
    assert(str::Eq("also quoted", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("att3", a->name));
    assert(str::Eq("notquoted", a->val));
    a = p->GetAttr(a->nextAttrId);
    assert(str::Eq("att4", a->name));
    assert(str::Eq("end", a->val));
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
    assert(ok);
    assert(709 == p.ElementsCount());
    assert(955 == p.TotalAttrCount());
    HtmlElement *el = p.GetRootElement();
    assert(str::Eq(el->name, "html"));
    el = p.GetElement(el->down);
    assert(str::Eq(el->name, "head"));
    el = p.GetElement(el->next);
    assert(str::Eq(el->name, "body"));
    el = p.GetElement(el->down);
    assert(str::Eq(el->name, "object"));
    el = p.GetElement(el->next);
    assert(str::Eq(el->name, "ul"));
    el = p.GetElement(el->down);
    assert(str::Eq(el->name, "li"));
    el = p.GetElement(el->down);
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
