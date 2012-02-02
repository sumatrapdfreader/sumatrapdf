/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "HtmlPullParser.h"
#include "BaseUtil.h"
#include "StrUtil.h"

static bool SkipUntil(const char*& s, const char *end, char c)
{
    while ((s < end) && (*s != c)) {
        ++s;
    }
    return *s == c;
}

static bool IsWs(int c) {
    return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

void SkipWs(const char* & s, const char *end)
{
    while ((s < end) && IsWs(*s)) {
        ++s;
    }
}

void SkipNonWs(const char* & s, const char *end)
{
    while ((s < end) && !IsWs(*s)) {
        ++s;
    }
}

static int IsNameChar(int c)
{
    return c == '.' || c == '-' || c == '_' || c == ':' ||
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

// skip all html tag or attribute chatacters
static void SkipName(const char*& s, const char *end)
{
    while ((s < end) && IsNameChar(*s)) {
        s++;
    }
}

// return true if s consists of only spaces
static bool IsSpaceOnly(const char *s, size_t len)
{
    const char *end = s + len;
    while (s < end) {
        if (*s++ != ' ')
            return false;
    }
    return true;
}

// at this point we're either after tag name or
// after previous attribute
// We expect: 
// whitespace | attribute name | = | attribute value
// where both attribute name and attribute value can
// be quoted
// Not multi-thread safe (uses static buffer)
AttrInfo *GetNextAttr(const char *&s, const char *end)
{
    static AttrInfo attrInfo;

    // parse attribute name
    SkipWs(s, end);
    if (s == end)
        return NULL;

    attrInfo.name = s;
    SkipName(s, end);
    attrInfo.nameLen = s - attrInfo.name;
    if (0 == attrInfo.nameLen)
        return NULL;
    SkipWs(s, end);
    if ((s == end) || ('=' != *s))
        return NULL;

    // parse attribute value
    ++s; // skip '='
    SkipWs(s, end);
    if (s == end)
        return NULL;
    char quoteChar = *s;
    if (('\'' == quoteChar) || ('\"' == quoteChar)) {
        ++s;
        attrInfo.val = s;
        if (!SkipUntil(s, end, quoteChar))
            return NULL;
        attrInfo.valLen = s - attrInfo.val;
        ++s;
    } else {
        attrInfo.val = s;
        while ((s < end) && !IsWs(*s)) {
            ++s;
        }
        attrInfo.valLen = s - attrInfo.val;
    }

    // TODO: should I allow empty values?
    if (0 == attrInfo.valLen)
        return NULL;
    return &attrInfo;
}

// Returns next part of html or NULL if finished
HtmlToken *HtmlPullParser::Next()
{
    const char *start;
 
    if (currPos >= end)
        return NULL;

Next:
    start = currPos;
    if (*currPos != '<') {
        // this must text between tags
        if (!SkipUntil(currPos, end, '<')) {
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

    if (!SkipUntil(currPos, end, '>'))
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


// strings is an array of 0-separated strings consequitevely laid out
// in memory. This functions find the position of str in this array,
// -1 means not found. The search is case-insensitive
int FindStrPos(const char *strings, const char *str, size_t len)
{
    const char *curr = strings;
    const char *end = str + len;
    char firstChar = tolower(*str);
    int n = 0;
    for (;;) {
        // we're at the start of the next tag
        char c = *curr;
        if ((0 == c) || (c > firstChar)) {
            // strings are sorted alphabetically, so we
            // can quit if current str is > tastringg
            return -1;
        }
        const char *s = str;
        while (*curr && (s < end)) {
            char c = tolower(*s++);
            if (c != *curr++)
                goto Next;
        }
        if ((s == end) && (0 == *curr))
            return n;
Next:
        while (*curr) {
            ++curr;
        }
        ++curr;
        ++n;
    }
    return -1;
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

HtmlTag FindTag(const char *tag, size_t len)
{
    return (HtmlTag)FindStrPos(HTML_TAGS_STRINGS, tag, len);
}

HtmlAttr FindAttr(const char *attr, size_t len)
{
    return (HtmlAttr)FindStrPos(HTML_ATTRS_STRINGS, attr, len);
}

AlignAttr FindAlignAttr(const char *attr, size_t len)
{
    return (AlignAttr)FindStrPos(ALIGN_ATTRS_STRINGS, attr, len);
}

bool IsSelfClosingTag(HtmlTag tag)
{
    // TODO: add more tags
    // TODO: optimize by sorting selfClosingTags and doing early bailout
    static HtmlTag selfClosingTags[] = { Tag_Br, Tag_Img, Tag_Hr };
    for (size_t i = 0; i < dimof(selfClosingTags); i++) {
        if (tag == selfClosingTags[i])
            return true;
    }
    return false;
}

size_t GetTagLen(const char *s, size_t len)
{
    const char *end = s + len;
    const char *curr = s;
    while (curr < end) {
        if (!IsNameChar(*curr))
            return curr - s;
        ++curr;
    }
    return len;
}

static void HtmlAddWithNesting(Vec<char>* out, HtmlTag tag, bool isStartTag, size_t nesting, const char *s, size_t sLen)
{
    static HtmlTag inlineTags[] = {Tag_Font, Tag_B, Tag_I, Tag_U};
    bool isInline = false;
    for (size_t i = 0; i < dimof(inlineTags); i++) {
        if (tag == inlineTags[i]) {
            isInline = true;
            break;
        }
    }
    if (!isInline && isStartTag) {
        for (size_t i = 0; i < nesting; i++) {
            out->Append(' ');
        }
    }
    out->Append(s, sLen);
    if (isInline || isStartTag)
        return;
    out->Append('\n');
}

// record the tag for the purpose of building current state
// of html tree
void RecordStartTag(Vec<HtmlTag>* tagNesting, HtmlTag tag)
{
    if (IsSelfClosingTag(tag))
        return;
    tagNesting->Append(tag);
}

// remove the tag from state of html tree
void RecordEndTag(Vec<HtmlTag> *tagNesting, HtmlTag tag)
{
    // TODO: this logic might need to be a bit more complicated
    // e.g. when closing a tag, if the top tag doesn't match
    // but there are only potentially self-closing tags
    // on the stack between the matching tag, we should pop
    // all of them
    if (tagNesting->Count() > 0)
        tagNesting->Pop();
}

static void PrettifyHtmlToken(HtmlToken *t, Vec<HtmlTag> *tagNesting, Vec<char>* out)
{
    size_t nesting = tagNesting->Count();
    if (t->IsText()) {
        out->Append((char*)t->s, t->sLen);
        //HtmlAddWithNesting(state->html, t->tag, nesting, t->s, t->sLen);
        return;
    }

    if (!t->IsTag())
        return;

    HtmlTag tag = FindTag(t->s, t->sLen);
    if (t->IsEmptyElementEndTag()) {
        HtmlAddWithNesting(out, tag, false, nesting, t->s - 1, t->sLen + 3);
        return;
    }

    if (t->IsStartTag()) {
        HtmlAddWithNesting(out, tag, true, nesting, t->s - 1, t->sLen + 2);
        return;
    }

    if (t->IsEndTag()) {
        if (nesting > 0)
            --nesting;
        HtmlAddWithNesting(out, tag, false, nesting, t->s - 2, t->sLen + 3);
    }
}

// tags that I want to explicitly ignore and not define
// HtmlTag enums for them
// One file has a bunch of st1:* tags (st1:city, st1:place etc.)
static bool IgnoreTag(const char *s, size_t sLen)
{
    if (sLen >= 4 && s[3] == ':' && s[0] == 's' && s[1] == 't' && s[2] == '1')
        return true;
    // no idea what "o:p" is
    if (sLen == 3 && s[1] == ':' && s[0] == 'o'  && s[2] == 'p')
        return true;
    return false;
}

Vec<char> *PrettyPrintHtml(const char *s, size_t len)
{
    Vec<char> *res = new Vec<char>(len);
    Vec<HtmlTag> tagNesting(256);
    HtmlPullParser parser(s, len);
    for (;;)
    {
        HtmlToken *t = parser.Next();
        if (!t || t->IsError())
            break;

        PrettifyHtmlToken(t, &tagNesting, res);
        if (!t->IsTag())
            continue;

        // HtmlToken string includes potential attributes,
        // get the length of just the tag
        size_t tagLen = GetTagLen(t->s, t->sLen);
        if (IgnoreTag(t->s, tagLen))
            continue;

        HtmlTag tag = FindTag(t->s, tagLen);
        if (Tag_NotFound == tag)
            continue;
        // update the current state of html tree
        if (t->IsStartTag())
            RecordStartTag(&tagNesting, tag);
        else if (t->IsEndTag())
            RecordEndTag(&tagNesting, tag);
    }
    return res;
}

