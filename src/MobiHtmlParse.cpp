/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiHtmlParse.h"
#include "Vec.h"

/* Converts mobi html to our internal format optimized for further layout/display.
Our format can be thought of as a stream of Virtual Machine instructions.
A VM instruction is either a string to be displayed or a formatting code (possibly
with some additional data for this formatting code).
We tokenize strings into words during this process because words are basic units
of layout.
Our format is a sequence of instructions encoded as sequence of bytes.
If an instruction starts with a byte in the range <FC_Last - 255>, it's a formatting
code followed by data specific for a given code.
Otherwise it's a string and the first byte is the length of the string followed by
string data.
If string length is >= FC_Last, we emit FC_Last followed by a 2-byte length (i.e. a
string cannot be longer than 65 kB).
Strings are utf8.
*/

// format codes correspond to html formatting tags
// TODO: do I need to represent Tag_Guide, Tag_Reference ?
enum FormatCode : uint8_t {
    // <b>
    FC_BoldStart = 255,
    FC_BoldEnd = 254,
    // <blockquote>
    FC_BlockQuoteStart = 253,
    FC_BlockQuoteEnd = 252,
    // <i>
    FC_ItalicStart = 251,
    FC_ItalicEnd = 250,
    // <p>
    FC_ParagraphStart = 249,
    FC_ParagraphEnd = 248,
    // <mbp:pagebrake>
    FC_MobiPageBreak = 247,
    // <table>
    FC_TableStart = 246,
    FC_TableEnd = 245,
    // <td>
    FC_TdStart = 244,
    FC_TdEnd = 243,
    // <tr>
    FC_TrStart = 242,
    FC_TrEnd = 241,
    // <a>
    FC_A = 240,
    // <br>
    FC_Br = 239,
    // <div>
    FC_DivStart = 238,
    FC_DivEnd = 237,
    // <font>
    FC_FontStart = 236,
    FC_FontEnd = 235,
    // <h2>
    FC_H2Start = 234,
    FC_H2End = 233,
    // <img>
    FC_Img = 232,
    // <ol>
    FC_OlStart = 231,
    FC_OlEnd = 230,
    // <li>
    FC_LiStart = 229,
    FC_LiEnd = 228,
    // <span>
    FC_SpanStart = 227,
    FC_SpanEnd = 226,
    // <sup>
    FC_SupStart = 225,
    FC_SupEnd = 224,
    // <u>
    FC_UnderlineStart = 223,
    FC_UnderlineEnd = 222,
    // <ul>
    FC_UlStart = 221,
    FC_UlEnd = 220,

    FC_Last = 219
};

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

// return true if s consists of only spaces
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

    CrashIf('>' != *currPos);
    if (currPos == start) {
        // skip empty tags (<>), because we're lenient
        ++currPos;
        goto Next;
    }

    // skip <? and <! (processing instructions and comments)
    if (('?' == *start) || ('!' == *start)) {
        ++currPos;
        goto Next;
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
    CrashIf('>' != *currPos);
    ++currPos;
    currToken.type = type;
    currToken.s = start;
    currToken.sLen = len;
    return &currToken;
}

// enum names match the order of tags in an array
enum TagEnum {
    TagNotFound = -1,
    Tag_A = 0,
    Tag_B = 1,
    Tag_Blockquote = 2,
    Tag_Body = 3,
    Tag_Br = 4,
    Tag_Div = 5,
    Tag_Font = 6,
    Tag_Guide = 7,
    Tag_H2 = 8,
    Tag_Head = 9,
    Tag_Html = 10,
    Tag_I = 11,
    Tag_Img = 12,
    Tag_Li = 13,
    Tag_Mbp_Pagebreak = 14,
    Tag_Ol = 15,
    Tag_P = 16,
    Tag_Reference = 17,
    Tag_Span = 18,
    Tag_Sup = 19,
    Tag_Table = 20,
    Tag_Td = 21,
    Tag_Tr = 22,
    Tag_U = 23,
    Tag_Ul = 24
};

char *gTags = "a\0b\0blockquote\0body\0br\0div\0font\0guide\0h2\0head\0html\0i\0img\0li\0mbp:pagebreak\0ol\0p\0reference\0span\0sup\0table\0td\0tr\0u\0ul\0";

TagEnum FindTag(char *tag, size_t len)
{
    char *curr = gTags;
    char *end = tag + len;
    char firstTagChar = *tag;
    int n = 0;
    for (;;) {
        // we're at the start of the next tag
        char c = *curr;
        if ((0 == c) || (c > firstTagChar)) {
            // gTags are sorted alphabetically, so we
            // can quit if current tag is > tag
            return TagNotFound;
        }
        char *s = tag;
        while (*curr && (s < end)) {
            if (*s++ != *curr++)
                goto NextTag;
        }
        if ((s == end) && (0 == *curr))
            return (TagEnum)n;
NextTag:
        while (*curr) {
            ++curr;
        }
        ++curr;
        ++n;
    }
    return TagNotFound;
}

#if 0
void DumpTag(HtmlToken *t)
{
    char buf[1024];
    if (t->sLen + 3 > dimof(buf))
        return;
    memcpy(buf, t->s, t->sLen);
    char *end = buf + t->sLen;
    char *tmp = buf;
    while ((tmp < end) && (*tmp != ' ')) {
        ++tmp;
    }
    *tmp++ = '\n';
    *tmp = 0;
    printf(buf);
}
#endif

static int IsName(int c)
{
    return c == '.' || c == '-' || c == '_' || c == ':' ||
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

static size_t GetTagLen(uint8_t *s, size_t len)
{
    uint8_t *end = s + len;
    uint8_t *curr = s;
    while (curr < end) {
        if (!IsName(*curr))
            return curr - s;
        ++curr;
    }
    return len;
}

static bool IsWs(uint8_t c) {
    return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

static void SkipWs(uint8_t*& s, uint8_t *end)
{
    while ((s < end) && IsWs(*s)) {
        ++s;
    }
}

static void SkipNonWs(uint8_t*& s, uint8_t *end)
{
    while ((s < end) && !IsWs(*s)) {
        ++s;
    }
}

static void EmitWord(Vec<uint8_t>& res, uint8_t *s, size_t len)
{
    // TODO: convert html entities to text
    if (len >= FC_Last) {
        // TODO: emit FC_Last and then len as 2 bytes
        return;
    }
    res.Append((uint8_t)len);
    res.Append(s, len);
}

static void EmitText(Vec<uint8_t>& res, HtmlToken *t)
{
    CrashIf(!t->IsText());
    uint8_t *end = t->s + t->sLen;
    uint8_t *curr = t->s;
    SkipWs(curr, end);
    while (curr < end) {
        uint8_t *currStart = curr;
        SkipNonWs(curr, end);
        size_t len = curr - currStart;
        if (len > 0)
            EmitWord(res, currStart, len);
        SkipWs(curr, end);
    }
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
        if (t->IsTag()) {
            // HtmlToken string includes potential attributes,
            // get the length of just the tagb
            size_t tagLen = GetTagLen(t->s, t->sLen);
            TagEnum tag = FindTag((char*)t->s, tagLen);
            assert(tag != TagNotFound);
        } else {
            // TODO: make sure valid utf8 or convert to utf8
            EmitText(res, t);
        }
#if 0
        if (t->IsTag()) {
            DumpTag(t);
        }
#endif
    }
    lenOut = res.Size();
    uint8_t *resData = res.StealData();
    return resData;
}
