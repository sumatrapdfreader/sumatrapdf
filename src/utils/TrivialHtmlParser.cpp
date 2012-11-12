/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "TrivialHtmlParser.h"

#include "HtmlPullParser.h"

/*
Html parser that is good enough for parsing html files
inside CHM archives (and XML files in EPUB documents).
Not really meant for general use.

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

struct HtmlAttr {
    char *name;
    char *val;
    HtmlAttr *next;
};

bool HtmlElement::NameIs(const char *name) const
{
    return str::Eq(this->name, name);
}

// for now just ignores any namespace qualifier
// (i.e. succeeds for "opf:content" with name="content" and any value of ns)
// TODO: add proper namespace support
bool HtmlElement::NameIsNS(const char *name, const char *ns) const
{
    CrashIf(!ns);
    const char *nameStart = str::FindChar(this->name, ':');
    nameStart = nameStart ? nameStart + 1 : this->name;
    return str::Eq(nameStart, name);
}

HtmlElement *HtmlElement::GetChildByName(const char *name, int idx) const
{
    for (HtmlElement *el = down; el; el = el->next) {
        if (el->NameIs(name)) {
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
    WCHAR wc = codepoint;
    char c = 0;
    WideCharToMultiByte(codepage, 0, &wc, 1, &c, 1, NULL, NULL);
    codepoint = c;
#endif
    if (codepoint <= 0 || codepoint >= (1 << (8 * sizeof(TCHAR))))
        return '?';
    return (TCHAR)codepoint;
}

// caller needs to free() the result
TCHAR *DecodeHtmlEntitites(const char *string, UINT codepage)
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
        if (str::Parse(src, _T("#%d;"), &unicode) ||
            str::Parse(src, _T("#x%x;"), &unicode)) {
            *dst++ = IntToChar(unicode, codepage);
            src = str::FindChar(src, ';') + 1;
            continue;
        }

        // named entities
        int rune = -1;
        const TCHAR *entityEnd = src;
        while (_istalnum(*entityEnd))
            entityEnd++;
        if (entityEnd != src) {
            size_t entityLen = entityEnd - src;
            rune = HtmlEntityNameToRune(src, entityLen);
        }
        if (-1 != rune) {
            *dst++ = IntToChar(rune, codepage);
            src = entityEnd;
            if (*src == _T(';'))
                ++src;
        } else {
            *dst++ = '&';
        }
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
            if (el->NameIs("ul") || el->NameIs("ol"))
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
        if (el->NameIs(tagName)) {
            currElement = el->up;
            return;
        }
    }
    // ignore the unexpected closing tag
}

void HtmlParser::AppendAttr(char *name, char *value)
{
    str::ToLower(name);
    currElement->firstAttr = AllocAttr(name, currElement->firstAttr);
    currElement->firstAttr->val = value;
}

// Parse s in place i.e. we assume we can modify it. Must be 0-terminated.
// The caller owns the memory for s.
HtmlElement *HtmlParser::ParseInPlace(char *s, UINT codepage)
{
    if (this->html)
        Reset();
    this->html = s;
    this->codepage = codepage;

    HtmlPullParser parser(s, strlen(s));
    HtmlToken *tok;

    while ((tok = parser.Next())) {
        char *tag = (char *)tok->s;
        if (tok->IsError()) {
            errorContext = tag;
            switch (tok->error) {
                case HtmlToken::UnclosedTag: return ParseError(ErrParsingElementName);
                case HtmlToken::InvalidTag:  return ParseError(ErrParsingClosingElement);
                default:                     return ParseError(ErrParsingElement);
            }
        }
        if (!tok->IsTag()) {
            // ignore text content
            assert(tok->IsText());
            continue;
        }
        char *tagEnd = tag + tok->nLen;
        if (!tok->IsEndTag()) {
            // note: call tok->NextAttr() before zero-terminating names and values
            AttrInfo *attr = tok->NextAttr();
            *tagEnd = '\0';
            StartTag(tag);

            while (attr) {
                char *name = (char *)attr->name;
                char *nameEnd = name + attr->nameLen;
                char *value = (char *)attr->val;
                char *valueEnd = value + attr->valLen;
                attr = tok->NextAttr();

                *nameEnd = *valueEnd = '\0';
                AppendAttr(name, value);
            }
        }
        if (!tok->IsStartTag() || IsTagSelfClosing(tok->tag)) {
            *tagEnd = '\0';
            CloseTag(tag);
        }
    }

    return rootElement;
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
    return FindElementByNameNS(name, NULL, from);
}

HtmlElement *HtmlParser::FindElementByNameNS(const char *name, const char *ns, HtmlElement *from)
{
    HtmlElement *el = from ? from : rootElement;
    if (from)
        goto FindNext;
    if (!el)
        return NULL;
CheckNext:
    if (el->NameIs(name) || ns && el->NameIsNS(name, ns))
        return el;
FindNext:
    if (el->down) {
        el = el->down;
        goto CheckNext;
    }
    if (el->next) {
        el = el->next;
        goto CheckNext;
    }
    // backup in the tree
    HtmlElement *parent = el->up;
    while (parent) {
        if (parent->next) {
            el = parent->next;
            goto CheckNext;
        }
        parent = parent->up;
    }
    return NULL;
}

#ifdef DEBUG
#include "TrivialHtmlParser_ut.cpp"
#endif
