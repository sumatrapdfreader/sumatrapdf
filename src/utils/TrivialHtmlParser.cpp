/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "TrivialHtmlParser.h"

#include "HtmlPullParser.h"
#include "Scoped.h"
#include "StrUtil.h"

/*
Html parser that is good enough for parsing html files
inside CHM archives. Not meant for general use.

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

struct HtmlAttr {
    char *name;
    char *val;
    HtmlAttr *next;
};

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

static int cmpCharPtrs(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static bool IsTagSelfclosing(const char *name)
{
    static const char *tagList[] = {
        "area", "base", "basefont", "br", "col", "frame",
        "hr", "img", "input", "link", "meta", "param"
    };
    return bsearch(&name, tagList, dimof(tagList), sizeof(const char *), cmpCharPtrs) != NULL;
}

HtmlElement *HtmlElement::GetChildByName(const char *name, int idx) const
{
    for (HtmlElement *el = down; el; el = el->next) {
        if (str::Eq(name, el->name)) {
            if (0 == idx)
                return el;
            idx--;
        }
    }
    return NULL;
}

static TCHAR IntToChar(int codepoint, UINT codepage)
{
#ifndef UNICODE
    char c = 0;
    WideCharToMultiByte(codepage, 0, &codepoint, 1, &c, 1, NULL, NULL);
    codepoint = c;
#endif
    if (codepoint <= 0 || codepoint >= (1 << (8 * sizeof(TCHAR))))
        return '?';
    return (TCHAR)codepoint;
}

// caller needs to free() the result
static TCHAR *DecodeHtmlEntitites(const char *string, UINT codepage=CP_ACP)
{
    TCHAR *fixed = str::conv::FromCodePage(string, codepage), *dst = fixed;
    const TCHAR *src = fixed;

    while (*src) {
        if (*src != '&') {
            *dst++ = *src++;
            continue;
        }
        src++;
        // numeric entities
        int unicode;
        if (str::Parse(src, _T("%d;"), &unicode) ||
            str::Parse(src, _T("#%x;"), &unicode)) {
            *dst++ = IntToChar(unicode, codepage);
            src = str::FindChar(src, ';') + 1;
            continue;
        }

        // named entities. We rely on the fact that entity names
        // do not contain non-ascii (> 127) characters so we can
        // ignore codepage without a change to the result
        size_t stringLen = str::Len(string);
        int rune = HtmlEntityNameToRune(string, stringLen);
        if (-1 != rune) {
            *dst++ = IntToChar(rune, codepage);
            src += stringLen;
            if (*src == ';')
                src++;
        }
        else
            *dst++ = '&';
    }
    *dst = '\0';

    return fixed;
}

HtmlParser::HtmlParser() : html(NULL), freeHtml(false), rootElement(NULL),
    currElement(NULL), elementsCount(0), attributesCount(0),
    error(ErrParsingNoError), errorContext(NULL)
{
}

HtmlParser::~HtmlParser()
{
    if (freeHtml)
        free(html);
}

void HtmlParser::Reset()
{
    if (freeHtml)
        free(html);
    html = NULL;
    freeHtml = false;
    rootElement = currElement = NULL;
    elementsCount = attributesCount = 0;
    error = ErrParsingNoError;
    errorContext = NULL;
    allocator.FreeAll();
}

HtmlAttr *HtmlParser::AllocAttr(char *name, HtmlAttr *next)
{
    HtmlAttr *attr = allocator.AllocStruct<HtmlAttr>();
    attr->name = name;
    attr->next = next;
    ++attributesCount;
    return attr;
}

// caller needs to free() the result
TCHAR *HtmlElement::GetAttribute(const char *name) const
{
    for (HtmlAttr *attr = firstAttr; attr; attr = attr->next) {
        if (str::EqI(attr->name, name))
            return DecodeHtmlEntitites(attr->val, codepage);
    }
    return NULL;
}

HtmlElement *HtmlParser::AllocElement(char *name, HtmlElement *parent)
{
    HtmlElement *el = allocator.AllocStruct<HtmlElement>();
    el->name = name;
    el->up = parent;
    el->codepage = codepage;
    ++elementsCount;
    return el;
}

HtmlElement *HtmlParser::FindParent(char *tagName)
{
    if (str::Eq(tagName, "li")) {
        // make a list item the child of the closest list
        for (HtmlElement *el = currElement; el; el = el->up) {
            if (str::Eq(el->name, "ul") || str::Eq(el->name, "ol"))
                return el;
        }
    }

    return currElement;
}

void HtmlParser::StartTag(char *tagName)
{
    str::ToLower(tagName);
    HtmlElement *parent = FindParent(tagName);
    currElement = AllocElement(tagName, parent);
    if (NULL == rootElement)
        rootElement = currElement;

    if (!parent) {
        // if this isn't the root tag, this tag
        // and all its children will be ignored
    } else if (NULL == parent->down) {
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
    for (HtmlElement *el = currElement; el; el = el->up) {
        if (str::Eq(el->name, tagName)) {
            currElement = el->up;
            return;
        }
    }
    // ignore the unexpected closing tag
}

void HtmlParser::StartAttr(char *name)
{
    str::ToLower(name);
    currElement->firstAttr = AllocAttr(name, currElement->firstAttr);
}

void HtmlParser::SetAttrVal(char *val)
{
    currElement->firstAttr->val = val;
}

static char *ParseAttrValue(char **sPtr)
{
    char *attrVal = NULL;
    char *s = *sPtr;
    SkipWs(&s);
    char quoteChar = *s;
    if (quoteChar == '"' || quoteChar == '\'') {
        ++s;
        attrVal = s;
        if (!SkipUntil(&s, quoteChar))
            return NULL;
        *s++ = 0;
    } else {
        attrVal = s;
        while (!IsUnquotedAttrValEnd(*s)) {
            ++s;
        }
        if (!IsWs(*s) && TagEndLen(s) == 0)
            return NULL;
        if (IsWs(*s))
            *s = 0;
    }
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
HtmlElement *HtmlParser::ParseInPlace(char *s, UINT codepage)
{
    char *tagName, *attrName, *attrVal, *tagEnd;
    int tagEndLen;

    if (this->html)
        Reset();

    this->html = s;
    this->codepage = codepage;

ParseText:
    if (!SkipUntil(&s, '<')) {
        // Note: I think we can be in an inconsistent state here
        // (unclosed tags) but not sure if we should care
        return rootElement;
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
        if (tagEndLen == 2 || IsTagSelfclosing(tagName))
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
    if (tagEndLen == 2 || IsTagSelfclosing(tagName))
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
    if (!SkipUntil(&s, '>'))
        return ParseError(ErrParsingExclOrPI);
    ++s;
    goto ParseText;
}

HtmlElement *HtmlParser::Parse(const char *s, UINT codepage)
{
    HtmlElement *root = ParseInPlace(str::Dup(s), codepage);
    freeHtml = true;
    return root;
}

// Does a depth-first search of element tree, looking for an element with
// a given name. If from is NULL, it starts from rootElement otherwise
// it starts from *next* element in traversal order, which allows for
// easy iteration over elements.
// Note: name must be lower-case
HtmlElement *HtmlParser::FindElementByName(const char *name, HtmlElement *from)
{
    HtmlElement *el = from;
    if (!from) {
        if (!rootElement)
            return NULL;
        if (str::Eq(name, rootElement->name))
            return rootElement;
        el = rootElement;
    }
Next:
    if (el->down) {
        el = el->down;
        goto FoundNext;
    }
    if (el->next) {
        el = el->next;
        goto FoundNext;
    }
    // backup in the tree
    HtmlElement *parent = el->up;
    while (parent) {
        if (parent->next) {
            el = parent->next;
            goto FoundNext;
        }
        parent = parent->up;
    }
    return NULL;
FoundNext:
    if (str::Eq(el->name, name))
        return el;
    goto Next;
}

#ifdef DEBUG
#include <assert.h>
#include "FileUtil.h"
#include "WinUtil.h"

namespace unittests {

static void HtmlParser00()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<a></A>");
    assert(p.ElementsCount() == 1);
    assert(root);
    assert(str::Eq("a", root->name));

    root = p.Parse("<b></B>");
    assert(p.ElementsCount() == 1);
    assert(root);
    assert(str::Eq("b", root->name));
}

static void HtmlParser01()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<A><bAh></a>");
    assert(p.ElementsCount() == 2);
    assert(str::Eq("a", root->name));
    assert(NULL == root->up);
    assert(NULL == root->next);
    HtmlElement *el = root->down;
    assert(NULL == el->firstAttr);
    assert(str::Eq("bah", el->name));
    assert(el->up == root);
    assert(NULL == el->down);
    assert(NULL == el->next);
}

static void HtmlParser05()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<!doctype><html><HEAD><meta name=foo></head><body><object t=la><param name=foo val=bar></object><ul><li></ul></object></body></Html>");
    assert(8 == p.ElementsCount());
    assert(4 == p.TotalAttrCount());
    assert(str::Eq("html", root->name));
    assert(NULL == root->up);
    assert(NULL == root->next);
    HtmlElement *el = root->down;
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
    el = p.FindElementByName("html");
    assert(el);
    el = p.FindElementByName("head", el);
    assert(el);
    assert(str::Eq("head", el->name));
    el = p.FindElementByName("ul", el);
    assert(el);
}

static void HtmlParser04()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<el att=  va&apos;l></ el >");
    assert(1 == p.ElementsCount());
    assert(1 == p.TotalAttrCount());
    assert(str::Eq("el", root->name));
    assert(NULL == root->next);
    assert(NULL == root->up);
    assert(NULL == root->down);
    ScopedMem<TCHAR> val(root->GetAttribute("att"));
    assert(str::Eq(val, _T("va'l")));
    assert(!root->firstAttr->next);
}

static void HtmlParser03()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<el   att  =v&quot;al/>");
    assert(1 == p.ElementsCount());
    assert(1 == p.TotalAttrCount());
    assert(str::Eq("el", root->name));
    assert(NULL == root->next);
    assert(NULL == root->up);
    assert(NULL == root->down);
    ScopedMem<TCHAR> val(root->GetAttribute("att"));
    assert(str::Eq(val, _T("v\"al")));
    assert(!root->firstAttr->next);
}

static void HtmlParser02()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<a><b/><  c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=&101;&#6e;d/></a>");
    assert(4 == p.ElementsCount());
    assert(4 == p.TotalAttrCount());
    assert(str::Eq("a", root->name));
    assert(NULL == root->next);
    HtmlElement *el = root->down;
    assert(str::Eq("b", el->name));
    assert(root == el->up);
    el = el->next;
    assert(str::Eq("c", el->name));
    assert(root == el->up);
    el = el->next;
    assert(str::Eq("d", el->name));
    assert(NULL == el->next);
    assert(root == el->up);
    ScopedMem<TCHAR> val(el->GetAttribute("at1"));
    assert(str::Eq(val, _T("<quo&ted>")));
    val.Set(el->GetAttribute("at2"));
    assert(str::Eq(val, _T("also quoted")));
    val.Set(el->GetAttribute("att3"));
    assert(str::Eq(val, _T("notquoted")));
    val.Set(el->GetAttribute("att4"));
    assert(str::Eq(val, _T("end")));
}

static void HtmlParser06()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<ul><p>ignore<li><br><meta><li><ol><li></ul><dropme>");
    assert(9 == p.ElementsCount());
    assert(0 == p.TotalAttrCount());
    assert(str::Eq("ul", root->name));
    assert(!root->next);
    HtmlElement *el = root->GetChildByName("li");
    assert(el);
    assert(str::Eq(el->down->name, "br"));
    assert(str::Eq(el->down->next->name, "meta"));
    assert(!el->down->next->next);
    el = root->GetChildByName("li", 1);
    assert(el);
    assert(!el->next);
    el = el->GetChildByName("ol");
    assert(!el->next);
    assert(str::Eq(el->down->name, "li"));
    assert(!el->down->down);
}

static void HtmlParser07()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<test umls=&auml;\xC3\xB6&#FC; zero=&1;&0;&-1;>", CP_UTF8);
    assert(1 == p.ElementsCount());
    ScopedMem<TCHAR> val(root->GetAttribute("umls"));
#ifdef UNICODE
    assert(str::Eq(val, L"\xE4\xF6\xFC"));
#else
    assert(str::EndsWith(val, "\xFC"));
#endif
    val.Set(root->GetAttribute("zero"));
    assert(str::Eq(val, _T("\x01??")));
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
    HtmlElement *root = p.ParseInPlace(d);
    assert(root);
    assert(709 == p.ElementsCount());
    assert(955 == p.TotalAttrCount());
    assert(str::Eq(root->name, "html"));
    HtmlElement *el = root->down;
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
    ScopedMem<TCHAR> val(el->GetAttribute("type"));
    assert(str::Eq(val, _T("text/sitemap")));
    el = el->down;
    assert(str::Eq(el->name, "param"));
    assert(!el->down);
    assert(str::Eq(el->next->name, "param"));
    el = p.FindElementByName("body");
    assert(el);
    el = p.FindElementByName("ul", el);
    assert(el);
    int count = 0;
    while (el) {
        ++count;
        el = p.FindElementByName("ul", el);
    }
    assert(18 == count);
    free(d);
}

}

void TrivialHtmlParser_UnitTests()
{
    unittests::HtmlParserFile();
    unittests::HtmlParser07();
    unittests::HtmlParser06();
    unittests::HtmlParser05();
    unittests::HtmlParser04();
    unittests::HtmlParser03();
    unittests::HtmlParser02();
    unittests::HtmlParser00();
    unittests::HtmlParser01();
}
#endif
