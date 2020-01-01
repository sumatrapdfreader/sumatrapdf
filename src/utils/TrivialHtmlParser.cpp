/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

#include "HtmlParserLookup.h"
#include "TrivialHtmlParser.h"
#include "HtmlPullParser.h"

/*
Html parser that is good enough for parsing html files
inside CHM archives (and XML files in EPUB documents).
Not really meant for general use.

name/val pointers inside Element/Attr structs refer to
memory inside HtmlParser::s, so they don't need to be freed.
*/

bool HtmlElement::NameIs(const char* name) const {
    if (!this->name) {
        CrashIf(Tag_NotFound == this->tag);
        HtmlTag tag = FindHtmlTag(name, str::Len(name));
        return tag == this->tag;
    }
    return str::EqI(this->name, name);
}

// for now just ignores any namespace qualifier
// (i.e. succeeds for "opf:content" with name="content" and any value of ns)
// TODO: add proper namespace support
bool HtmlElement::NameIsNS(const char* name, const char* ns) const {
    CrashIf(!ns);
    const char* nameStart = nullptr;
    if (this->name) {
        nameStart = str::FindChar(this->name, ':');
    }
    if (!nameStart) {
        return NameIs(name);
    }
    nameStart = nameStart ? nameStart + 1 : this->name;
    return str::EqI(nameStart, name);
}

HtmlElement* HtmlElement::GetChildByTag(HtmlTag tag, int idx) const {
    for (HtmlElement* el = down; el; el = el->next) {
        if (tag == el->tag) {
            if (0 == idx)
                return el;
            idx--;
        }
    }
    return nullptr;
}

static WCHAR IntToChar(int codepoint) {
    if (codepoint <= 0 || codepoint >= (1 << (8 * sizeof(WCHAR))))
        return '?';
    return (WCHAR)codepoint;
}

// caller needs to free() the result
WCHAR* DecodeHtmlEntitites(const char* string, UINT codepage) {
    WCHAR* fixed = strconv::FromCodePage(string, codepage);
    WCHAR* dst = fixed;
    const WCHAR* src = fixed;

    while (*src) {
        if (*src != '&') {
            *dst++ = *src++;
            continue;
        }
        src++;
        // numeric entities
        int unicode;
        if (str::Parse(src, L"#%d;", &unicode) || str::Parse(src, L"#x%x;", &unicode)) {
            *dst++ = IntToChar(unicode);
            src = str::FindChar(src, ';') + 1;
            continue;
        }

        // named entities
        int rune = -1;
        const WCHAR* entityEnd = src;
        while (iswalnum(*entityEnd)) {
            entityEnd++;
        }

        if (entityEnd != src) {
            size_t entityLen = entityEnd - src;
            rune = HtmlEntityNameToRune(src, entityLen);
        }
        if (-1 != rune) {
            *dst++ = IntToChar(rune);
            src = entityEnd;
            if (*src == ';')
                ++src;
        } else {
            *dst++ = '&';
        }
    }
    *dst = '\0';

    return fixed;
}

HtmlParser::HtmlParser()
    : html(nullptr),
      freeHtml(false),
      rootElement(nullptr),
      currElement(nullptr),
      elementsCount(0),
      attributesCount(0),
      codepage(CP_ACP),
      error(ErrParsingNoError),
      errorContext(nullptr) {
}

HtmlParser::~HtmlParser() {
    if (freeHtml)
        free(html);
}

void HtmlParser::Reset() {
    if (freeHtml)
        free(html);
    html = nullptr;
    freeHtml = false;
    rootElement = currElement = nullptr;
    elementsCount = attributesCount = 0;
    error = ErrParsingNoError;
    errorContext = nullptr;
    allocator.FreeAll();
}

HtmlAttr* HtmlParser::AllocAttr(char* name, HtmlAttr* next) {
    HtmlAttr* attr = allocator.AllocStruct<HtmlAttr>();
    attr->name = name;
    attr->next = next;
    ++attributesCount;
    return attr;
}

// caller needs to free() the result
WCHAR* HtmlElement::GetAttribute(const char* name) const {
    for (HtmlAttr* attr = firstAttr; attr; attr = attr->next) {
        if (str::EqI(attr->name, name))
            return DecodeHtmlEntitites(attr->val, codepage);
    }
    return nullptr;
}

HtmlElement* HtmlParser::AllocElement(HtmlTag tag, char* name, HtmlElement* parent) {
    HtmlElement* el = allocator.AllocStruct<HtmlElement>();
    el->tag = tag;
    el->name = name;
    el->up = parent;
    el->codepage = codepage;
    ++elementsCount;
    return el;
}

HtmlElement* HtmlParser::FindParent(HtmlToken* tok) {
    if (Tag_Li == tok->tag) {
        // make a list item the child of the closest list
        for (HtmlElement* el = currElement; el; el = el->up) {
            if (Tag_Ul == el->tag || Tag_Ol == el->tag)
                return el;
        }
    }

    return currElement;
}

void HtmlParser::StartTag(HtmlToken* tok) {
    char* tagName = nullptr;
    if (Tag_NotFound == tok->tag) {
        tagName = (char*)tok->s;
        char* tagEnd = tagName + tok->nLen;
        *tagEnd = '\0';
    }

    HtmlElement* parent = FindParent(tok);
    currElement = AllocElement(tok->tag, tagName, parent);
    if (nullptr == rootElement)
        rootElement = currElement;

    if (!parent) {
        // if this isn't the root tag, this tag
        // and all its children will be ignored
    } else if (nullptr == parent->down) {
        // parent has no children => set as a first child
        parent->down = currElement;
    } else {
        // parent has children => set as a sibling
        HtmlElement* tmp = parent->down;
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = currElement;
    }
}

void HtmlParser::CloseTag(HtmlToken* tok) {
    char* tagName = nullptr;
    if (Tag_NotFound == tok->tag) {
        tagName = (char*)tok->s;
        char* tagEnd = tagName + tok->nLen;
        *tagEnd = '\0';
    }

    // to allow for lack of closing tags, e.g. in case like
    // <a><b><c></a>, we look for the first parent with matching name
    for (HtmlElement* el = currElement; el; el = el->up) {
        if (tagName ? el->NameIs(tagName) : tok->tag == el->tag) {
            currElement = el->up;
            return;
        }
    }
    // ignore the unexpected closing tag
}

void HtmlParser::AppendAttr(char* name, char* value) {
    currElement->firstAttr = AllocAttr(name, currElement->firstAttr);
    currElement->firstAttr->val = value;
}

// Parse s in place i.e. we assume we can modify it. Must be 0-terminated.
// The caller owns the memory for s.
HtmlElement* HtmlParser::ParseInPlace(char* s, UINT codepage) {
    if (this->html)
        Reset();
    this->html = s;
    this->codepage = codepage;

    HtmlPullParser parser(s, strlen(s));
    HtmlToken* tok;

    while ((tok = parser.Next()) != nullptr) {
        if (tok->IsError()) {
            errorContext = tok->s;
            switch (tok->error) {
                case HtmlToken::UnclosedTag:
                    return ParseError(ErrParsingElementName);
                case HtmlToken::InvalidTag:
                    return ParseError(ErrParsingClosingElement);
                default:
                    return ParseError(ErrParsingElement);
            }
        }
        if (!tok->IsTag()) {
            // ignore text content
            AssertCrash(tok->IsText());
            continue;
        }
        if (!tok->IsEndTag()) {
            // note: call tok->NextAttr() before zero-terminating names and values
            AttrInfo* attr = tok->NextAttr();
            StartTag(tok);

            while (attr) {
                char* name = (char*)attr->name;
                char* nameEnd = name + attr->nameLen;
                char* value = (char*)attr->val;
                char* valueEnd = value + attr->valLen;
                attr = tok->NextAttr();

                *nameEnd = *valueEnd = '\0';
                AppendAttr(name, value);
            }
        }
        if (!tok->IsStartTag() || IsTagSelfClosing(tok->tag)) {
            CloseTag(tok);
        }
    }

    return rootElement;
}

HtmlElement* HtmlParser::Parse(const char* s, UINT codepage) {
    HtmlElement* root = ParseInPlace(str::Dup(s), codepage);
    freeHtml = true;
    return root;
}

// Does a depth-first search of element tree, looking for an element with
// a given name. If from is nullptr, it starts from rootElement otherwise
// it starts from *next* element in traversal order, which allows for
// easy iteration over elements.
// Note: name must be lower-case
HtmlElement* HtmlParser::FindElementByName(const char* name, HtmlElement* from) {
    return FindElementByNameNS(name, nullptr, from);
}

HtmlElement* HtmlParser::FindElementByNameNS(const char* name, const char* ns, HtmlElement* from) {
    HtmlElement* el = from ? from : rootElement;
    if (from)
        goto FindNext;
    if (!el)
        return nullptr;
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
    HtmlElement* parent = el->up;
    while (parent) {
        if (parent->next) {
            el = parent->next;
            goto CheckNext;
        }
        parent = parent->up;
    }
    return nullptr;
}
