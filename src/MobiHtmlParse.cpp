/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiHtmlParse.h"
#include "Vec.h"

struct HtmlToken {
    enum TokenType {
        StartTag,           // <foo>
        EndTag,             // </foo>
        EmptyElementTag,    // <foo/>
        Text,               // <foo>text</foo> => "text"
        Error
    };

    enum ParsingError {
        ExpectedElement,
        NonTagAtEnd,
        UnclosedTag,
        EmptyTag,
        InvalidTag
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsEmptyElementEndTag() const { return type == EmptyElementTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsEmptyElementEndTag(); }
    bool IsText() const { return type == Text; }
    bool IsError() const { return type == Error; }

    void SetError(ParsingError err, uint8_t *errContext) {
        type = Error;
        error = err;
        s = errContext;
    }

    TokenType       type;
    ParsingError    error;
    uint8_t *       s;
    size_t          sLen;
};

/* A very simple pull html parser. Simply call Next() to get the next part of
html, which can be one one of 3 tag types or error. If a tag has attributes,
the caller has to parse them out. */
class HtmlPullParser {
    uint8_t *   s;
    uint8_t *   currPos;

    HtmlToken   currToken;

    HtmlToken * MakeError(HtmlToken::ParsingError err, uint8_t *errContext) {
        currToken.SetError(err, errContext);
        return &currToken;
    }

public:
    HtmlPullParser(uint8_t *s) : s(s), currPos(s) {
    }

    HtmlToken *Next();
};

// TODO: share this and other such functions with TrivialHtmlParser.cpp
static bool SkipUntil(char **sPtr, char c)
{
    char *s = *sPtr;
    while (*s && (*s != c)) {
        ++s;
    }
    *sPtr = s;
    return *s == c;
}

static bool SkipUntil(uint8_t **sPtr, char c)
{
    return SkipUntil((char**)sPtr, c);
}

static bool IsSpaceOnly(uint8_t *s, size_t len)
{
    uint8_t *end = s + len;
    while (s < end) {
        if (*s++ != ' ')
            return false;
    }
    return true;
}

// Returns next part of html or NULL if finished
HtmlToken *HtmlPullParser::Next()
{
    uint8_t *start;
 
    if (!*currPos)
        return NULL;

    if (s == currPos) {
        // at the beginning, we expect a tag
        if (*currPos != '<')
            return MakeError(HtmlToken::ExpectedElement, currPos);
    }

Next:
    start = currPos;
    if (*currPos != '<') {
        // this must text between tags
        if (!SkipUntil(&currPos, '<')) {
            // text cannot be at the end
            return MakeError(HtmlToken::NonTagAtEnd, start);
        }
        size_t len = currPos - start;
        if (IsSpaceOnly(start, len))
            goto Next;
        currToken.type = HtmlToken::Text;
        currToken.s = start;
        currToken.sLen = len;
        return &currToken;
    }

    // '<' - tag begins
    ++start;
    if (!SkipUntil(&currPos, '>'))
        return MakeError(HtmlToken::UnclosedTag, start);

    if (currPos == start) {
        // <>
        return MakeError(HtmlToken::EmptyTag, start);
    }
    HtmlToken::TokenType type = HtmlToken::StartTag;
    if (('/' == *start) && ('/' == currPos[-1])) {
        // </foo/>
        return MakeError(HtmlToken::InvalidTag, start);
    }
    size_t len = currPos - start;
    if ('/' == *start) {
        // </foo>
        type = HtmlToken::EndTag;
        ++start;
        len -= 1;
    } else if ('/' == currPos[-1]) {
        // <foo/>
        type = HtmlToken::EmptyElementTag;
        len -= 1;
    }
    assert('>' == *currPos);
    ++currPos;
    currToken.type = type;
    currToken.s = start;
    currToken.sLen = len;
    return &currToken;
}

// convert mobi html to a format optimized for layout/display
// returns NULL in case of an error
// caller has to free() the result
uint8_t *MobiHtmlToDisplay(uint8_t *s, size_t sLen, size_t& lenOut)
{
    Vec<uint8_t> res(sLen); // set estimated size to avoid re-allocations
    HtmlPullParser parser(s);

    for (;;)
    {
        HtmlToken *t = parser.Next();
        if (!t)
            break;
        if (t->IsError())
            return NULL;
        // TODO: interpret the tag
    }
    lenOut = res.Size();
    uint8_t *resData = res.StealData();
    return resData;
}
