/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "TrivialHtmlParser.h"

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

struct HtmlEntity {
    const TCHAR *name;
    wchar_t unicode;
};
static HtmlEntity gHtmlEntities[] = { { _T("AElig"), 0xc6 }, { _T("Aacute"), 0xc1 }, { _T("Acirc"), 0xc2 }, { _T("Agrave"), 0xc0 }, { _T("Alpha"), 0x391 }, { _T("Aring"), 0xc5 }, { _T("Atilde"), 0xc3 }, { _T("Auml"), 0xc4 }, { _T("Beta"), 0x392 }, { _T("Ccedil"), 0xc7 }, { _T("Chi"), 0x3a7 }, { _T("Dagger"), 0x2021 }, { _T("Delta"), 0x394 }, { _T("ETH"), 0xd0 }, { _T("Eacute"), 0xc9 }, { _T("Ecirc"), 0xca }, { _T("Egrave"), 0xc8 }, { _T("Epsilon"), 0x395 }, { _T("Eta"), 0x397 }, { _T("Euml"), 0xcb }, { _T("Gamma"), 0x393 }, { _T("Iacute"), 0xcd }, { _T("Icirc"), 0xce }, { _T("Igrave"), 0xcc }, { _T("Iota"), 0x399 }, { _T("Iuml"), 0xcf }, { _T("Kappa"), 0x39a }, { _T("Lambda"), 0x39b }, { _T("Mu"), 0x39c }, { _T("Ntilde"), 0xd1 }, { _T("Nu"), 0x39d }, { _T("OElig"), 0x152 }, { _T("Oacute"), 0xd3 }, { _T("Ocirc"), 0xd4 }, { _T("Ograve"), 0xd2 }, { _T("Omega"), 0x3a9 }, { _T("Omicron"), 0x39f }, { _T("Oslash"), 0xd8 }, { _T("Otilde"), 0xd5 }, { _T("Ouml"), 0xd6 }, { _T("Phi"), 0x3a6 }, { _T("Pi"), 0x3a0 }, { _T("Prime"), 0x2033 }, { _T("Psi"), 0x3a8 }, { _T("Rho"), 0x3a1 }, { _T("Scaron"), 0x160 }, { _T("Sigma"), 0x3a3 }, { _T("THORN"), 0xde }, { _T("Tau"), 0x3a4 }, { _T("Theta"), 0x398 }, { _T("Uacute"), 0xda }, { _T("Ucirc"), 0xdb }, { _T("Ugrave"), 0xd9 }, { _T("Upsilon"), 0x3a5 }, { _T("Uuml"), 0xdc }, { _T("Xi"), 0x39e }, { _T("Yacute"), 0xdd }, { _T("Yuml"), 0x178 }, { _T("Zeta"), 0x396 }, { _T("aacute"), 0xe1 }, { _T("acirc"), 0xe2 }, { _T("acute"), 0xb4 }, { _T("aelig"), 0xe6 }, { _T("agrave"), 0xe0 }, { _T("alefsym"), 0x2135 }, { _T("alpha"), 0x3b1 }, { _T("amp"), 0x26 }, { _T("and"), 0x2227 }, { _T("ang"), 0x2220 }, { _T("apos"), 0x27 }, { _T("aring"), 0xe5 }, { _T("asymp"), 0x2248 }, { _T("atilde"), 0xe3 }, { _T("auml"), 0xe4 }, { _T("bdquo"), 0x201e }, { _T("beta"), 0x3b2 }, { _T("brvbar"), 0xa6 }, { _T("bull"), 0x2022 }, { _T("cap"), 0x2229 }, { _T("ccedil"), 0xe7 }, { _T("cedil"), 0xb8 }, { _T("cent"), 0xa2 }, { _T("chi"), 0x3c7 }, { _T("circ"), 0x2c6 }, { _T("clubs"), 0x2663 }, { _T("cong"), 0x2245 }, { _T("copy"), 0xa9 }, { _T("crarr"), 0x21b5 }, { _T("cup"), 0x222a }, { _T("curren"), 0xa4 }, { _T("dArr"), 0x21d3 }, { _T("dagger"), 0x2020 }, { _T("darr"), 0x2193 }, { _T("deg"), 0xb0 }, { _T("delta"), 0x3b4 }, { _T("diams"), 0x2666 }, { _T("divide"), 0xf7 }, { _T("eacute"), 0xe9 }, { _T("ecirc"), 0xea }, { _T("egrave"), 0xe8 }, { _T("empty"), 0x2205 }, { _T("emsp"), 0x2003 }, { _T("ensp"), 0x2002 }, { _T("epsilon"), 0x3b5 }, { _T("equiv"), 0x2261 }, { _T("eta"), 0x3b7 }, { _T("eth"), 0xf0 }, { _T("euml"), 0xeb }, { _T("euro"), 0x20ac }, { _T("exist"), 0x2203 }, { _T("fnof"), 0x192 }, { _T("forall"), 0x2200 }, { _T("frac12"), 0xbd }, { _T("frac14"), 0xbc }, { _T("frac34"), 0xbe }, { _T("frasl"), 0x2044 }, { _T("gamma"), 0x3b3 }, { _T("ge"), 0x2265 }, { _T("gt"), 0x3e }, { _T("hArr"), 0x21d4 }, { _T("harr"), 0x2194 }, { _T("hearts"), 0x2665 }, { _T("hellip"), 0x2026 }, { _T("iacute"), 0xed }, { _T("icirc"), 0xee }, { _T("iexcl"), 0xa1 }, { _T("igrave"), 0xec }, { _T("image"), 0x2111 }, { _T("infin"), 0x221e }, { _T("int"), 0x222b }, { _T("iota"), 0x3b9 }, { _T("iquest"), 0xbf }, { _T("isin"), 0x2208 }, { _T("iuml"), 0xef }, { _T("kappa"), 0x3ba }, { _T("lArr"), 0x21d0 }, { _T("lambda"), 0x3bb }, { _T("lang"), 0x2329 }, { _T("laquo"), 0xab }, { _T("larr"), 0x2190 }, { _T("lceil"), 0x2308 }, { _T("ldquo"), 0x201c }, { _T("le"), 0x2264 }, { _T("lfloor"), 0x230a }, { _T("lowast"), 0x2217 }, { _T("loz"), 0x25ca }, { _T("lrm"), 0x200e }, { _T("lsaquo"), 0x2039 }, { _T("lsquo"), 0x2018 }, { _T("lt"), 0x3c }, { _T("macr"), 0xaf }, { _T("mdash"), 0x2014 }, { _T("micro"), 0xb5 }, { _T("middot"), 0xb7 }, { _T("minus"), 0x2212 }, { _T("mu"), 0x3bc }, { _T("nabla"), 0x2207 }, { _T("nbsp"), 0xa0 }, { _T("ndash"), 0x2013 }, { _T("ne"), 0x2260 }, { _T("ni"), 0x220b }, { _T("not"), 0xac }, { _T("notin"), 0x2209 }, { _T("nsub"), 0x2284 }, { _T("ntilde"), 0xf1 }, { _T("nu"), 0x3bd }, { _T("oacute"), 0xf3 }, { _T("ocirc"), 0xf4 }, { _T("oelig"), 0x153 }, { _T("ograve"), 0xf2 }, { _T("oline"), 0x203e }, { _T("omega"), 0x3c9 }, { _T("omicron"), 0x3bf }, { _T("oplus"), 0x2295 }, { _T("or"), 0x2228 }, { _T("ordf"), 0xaa }, { _T("ordm"), 0xba }, { _T("oslash"), 0xf8 }, { _T("otilde"), 0xf5 }, { _T("otimes"), 0x2297 }, { _T("ouml"), 0xf6 }, { _T("para"), 0xb6 }, { _T("part"), 0x2202 }, { _T("permil"), 0x2030 }, { _T("perp"), 0x22a5 }, { _T("phi"), 0x3c6 }, { _T("pi"), 0x3c0 }, { _T("piv"), 0x3d6 }, { _T("plusmn"), 0xb1 }, { _T("pound"), 0xa3 }, { _T("prime"), 0x2032 }, { _T("prod"), 0x220f }, { _T("prop"), 0x221d }, { _T("psi"), 0x3c8 }, { _T("quot"), 0x22 }, { _T("rArr"), 0x21d2 }, { _T("radic"), 0x221a }, { _T("rang"), 0x232a }, { _T("raquo"), 0xbb }, { _T("rarr"), 0x2192 }, { _T("rceil"), 0x2309 }, { _T("rdquo"), 0x201d }, { _T("real"), 0x211c }, { _T("reg"), 0xae }, { _T("rfloor"), 0x230b }, { _T("rho"), 0x3c1 }, { _T("rlm"), 0x200f }, { _T("rsaquo"), 0x203a }, { _T("rsquo"), 0x2019 }, { _T("sbquo"), 0x201a }, { _T("scaron"), 0x161 }, { _T("sdot"), 0x22c5 }, { _T("sect"), 0xa7 }, { _T("shy"), 0xad }, { _T("sigma"), 0x3c3 }, { _T("sigmaf"), 0x3c2 }, { _T("sim"), 0x223c }, { _T("spades"), 0x2660 }, { _T("sub"), 0x2282 }, { _T("sube"), 0x2286 }, { _T("sum"), 0x2211 }, { _T("sup"), 0x2283 }, { _T("sup1"), 0xb9 }, { _T("sup2"), 0xb2 }, { _T("sup3"), 0xb3 }, { _T("supe"), 0x2287 }, { _T("szlig"), 0xdf }, { _T("tau"), 0x3c4 }, { _T("there4"), 0x2234 }, { _T("theta"), 0x3b8 }, { _T("thetasym"), 0x3d1 }, { _T("thinsp"), 0x2009 }, { _T("thorn"), 0xfe }, { _T("tilde"), 0x2dc }, { _T("times"), 0xd7 }, { _T("trade"), 0x2122 }, { _T("uArr"), 0x21d1 }, { _T("uacute"), 0xfa }, { _T("uarr"), 0x2191 }, { _T("ucirc"), 0xfb }, { _T("ugrave"), 0xf9 }, { _T("uml"), 0xa8 }, { _T("upsih"), 0x3d2 }, { _T("upsilon"), 0x3c5 }, { _T("uuml"), 0xfc }, { _T("weierp"), 0x2118 }, { _T("xi"), 0x3be }, { _T("yacute"), 0xfd }, { _T("yen"), 0xa5 }, { _T("yuml"), 0xff }, { _T("zeta"), 0x3b6 }, { _T("zwj"), 0x200d }, { _T("zwnj"), 0x200c } };

static int entityComparator(const void *a, const void *b)
{
    const TCHAR *nameA = ((HtmlEntity *)a)->name;
    const TCHAR *nameB = ((HtmlEntity *)b)->name;
    // skip identical characters (case sensitively)
    for (; *nameA && *nameA == *nameB; nameA++, nameB++);
    // if both names are identical in all alpha-numeric characters, the entitites are the same
    if (!_istalnum(*nameA) && !_istalnum(*nameB))
        return 0;
    return *nameA - *nameB;
}

// caller needs to free() the result
static TCHAR *DecodeHtmlEntitites(const char *string)
{
    TCHAR *fixed = str::conv::FromAnsi(string), *dst = fixed;
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
            if (unicode < (1 << (8 * sizeof(TCHAR))))
                *dst++ = (TCHAR)unicode;
            else
                *dst++ = '?';
            src = str::FindChar(src, ';') + 1;
            continue;
        }
        // named entities
        HtmlEntity cmp = { src };
        HtmlEntity *entity = (HtmlEntity *)bsearch(&cmp, gHtmlEntities, dimof(gHtmlEntities), sizeof(HtmlEntity), entityComparator);
        if (entity) {
            if (entity->unicode < (1 << (8 * sizeof(TCHAR))))
                *dst++ = (TCHAR)entity->unicode;
            else
                *dst++ = '?';
            src += str::Len(entity->name);
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
    for (HtmlAttr *attr = this->firstAttr; attr; attr = attr->next) {
        if (str::EqI(attr->name, name))
            return DecodeHtmlEntitites(attr->val);
    }
    return NULL;
}

HtmlElement *HtmlParser::AllocElement(char *name, HtmlElement *parent)
{
    HtmlElement *el = allocator.AllocStruct<HtmlElement>();
    el->name = name;
    el->up = parent;
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
HtmlElement *HtmlParser::ParseInPlace(char *s)
{
    char *tagName, *attrName, *attrVal, *tagEnd;
    int tagEndLen;

    html = s;
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

HtmlElement *HtmlParser::Parse(const char *s)
{
    freeHtml = true;
    return ParseInPlace(str::Dup(s));
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
    unittests::HtmlParser06();
    unittests::HtmlParser05();
    unittests::HtmlParser04();
    unittests::HtmlParser03();
    unittests::HtmlParser02();
    unittests::HtmlParser00();
    unittests::HtmlParser01();
}
#endif
