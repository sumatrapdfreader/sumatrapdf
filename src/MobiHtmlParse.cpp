/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiHtmlParse.h"
#include "Vec.h"

struct HtmlToken {
    enum TokenType {
        StartTag,       // <foo>
        EndTag,         // </foo>
        StartEndTag,    // <foo/>
        Text,           // <foo>text</foo> => "text"
        Finished,
        Error
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsStartEndTag() const { return type == StartEndTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsStartEndTag(); }
    bool IsText() const { return type == Text; }
    bool IsFinished() const { return type == Finished; }
    bool IsError() const { return type == Error; }

    TokenType       type;
    uint8_t *       s;
    size_t          sLen;
};

class HtmlPullParser {
    uint8_t *   s;
    size_t      sLen;
    uint8_t *   currPos;
    uint8_t *   end;

    HtmlToken   currToken;
public:
    HtmlPullParser(uint8_t *s, size_t sLen) : s(s), sLen(sLen)
    {
        currPos = s;
        end = s + sLen;
    }

    HtmlToken *Next();
};

HtmlToken *HtmlPullParser::Next()
{
    currToken.type = HtmlToken::Finished;
    return &currToken;
}

// convert mobi html to a format optimized for layout/display
// caller has to free() the result
uint8_t *MobiHtmlToDisplay(uint8_t *s, size_t sLen, size_t& lenOut)
{
    Vec<uint8_t> res(sLen);
    HtmlPullParser parser(s, sLen);

    for (;;)
    {
        HtmlToken *t = parser.Next();
        if (t->IsFinished())
            break;
        if (t->IsError())
            return NULL;
    }
    lenOut = res.Size();
    uint8_t *resData = res.StealData();
    return resData;
}
