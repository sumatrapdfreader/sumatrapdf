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

// skip all html tag or attribute characters
static void SkipName(const char*& s, const char *end)
{
    while ((s < end) && IsNameChar(*s)) {
        s++;
    }
}

// return true if s consists only of whitespace
static bool IsSpaceOnly(const char *s, const char *end)
{
    SkipWs(s, end);
    return s == end;
}

static bool StrLenIs(const char *s, size_t len, const char *s2)
{
    return str::Len(s2) == len && str::StartsWith(s, s2);
}

bool AttrInfo::NameIs(const char *s) const
{
    return StrLenIs(name, nameLen, s);
}

bool AttrInfo::ValIs(const char *s) const
{
    return StrLenIs(val, valLen, s);
}

void HtmlToken::SetValue(TokenType new_type, const char *new_s, const char *end)
{
    type = new_type;
    s = new_s;
    sLen = end - s;
    nextAttr = NULL;
}

void HtmlToken::SetError(ParsingError err, const char *errContext)
{
    type = Error;
    error = err;
    s = errContext;
}

bool HtmlToken::NameIs(const char *name) const
{
    return  (str::Len(name) == GetTagLen(this)) && str::StartsWith(s, name);
}

// We expect:
// whitespace | attribute name | = | attribute value
// where attribute value can be quoted
AttrInfo *HtmlToken::NextAttr()
{
    // restart after the last attribute (or start from the beginning)
    const char *s = nextAttr;
    if (!s)
        s = this->s + GetTagLen(this);
    const char *end = this->s + sLen;

    // parse attribute name
    SkipWs(s, end);
    if (s == end) {
NoNextAttr:
        nextAttr = NULL;
        return NULL;
    }
    attrInfo.name = s;
    SkipName(s, end);
    attrInfo.nameLen = s - attrInfo.name;
    if (0 == attrInfo.nameLen)
        goto NoNextAttr;
    SkipWs(s, end);
    if ((s == end) || ('=' != *s)) {
        // attributes without values get their names as value in HTML
        attrInfo.val = attrInfo.name;
        attrInfo.valLen = attrInfo.nameLen;
        nextAttr = s;
        return &attrInfo;
    }

    // parse attribute value
    ++s; // skip '='
    SkipWs(s, end);
    if (s == end) {
        // attribute with implicit empty value
        attrInfo.val = s;
        attrInfo.valLen = 0;
    } else if (('\'' == *s) || ('\"' == *s)) {
        // attribute with quoted value
        ++s;
        attrInfo.val = s;
        if (!SkipUntil(s, end, *(s - 1)))
            goto NoNextAttr;
        attrInfo.valLen = s - attrInfo.val;
        ++s;
    } else {
        attrInfo.val = s;
        SkipNonWs(s, end);
        attrInfo.valLen = s - attrInfo.val;
    }
    nextAttr = s;
    return &attrInfo;
}

// Given a tag like e.g.:
// <tag attr=">" />
// tries to find the closing '>' and not be confused by '>' that
// are part of attribute value. We're not very strict here
// Returns false if didn't find 
static bool SkipUntilTagEnd(const char*& s, const char *end)
{
    while (s < end) {
        char c = *s++;
        if ('>' == c) {
            --s;
            return true;
        }
        if (('\'' == c) || ('"' == c)) {
            if (!SkipUntil(s, end, c))
                return false;
            ++s;
        }
    }
    return false;
}

// Returns next part of html or NULL if finished
HtmlToken *HtmlPullParser::Next()
{
    if (currPos >= end)
        return NULL;

Next:
    const char *start = currPos;
    if (*currPos != '<') {
        // this must text between tags
        if (!SkipUntil(currPos, end, '<')) {
            // ignore whitespace after the last tag
            if (IsSpaceOnly(start, currPos))
                return NULL;
            // text cannot be at the end
            currToken.SetError(HtmlToken::NonTagAtEnd, start);
        } else {
            // don't report whitespace between tags
            if (IsSpaceOnly(start, currPos))
                goto Next;
            currToken.SetValue(HtmlToken::Text, start, currPos);
        }
        return &currToken;
    }

    // '<' - tag begins
    ++start;

    if (!SkipUntilTagEnd(currPos, end)) {
        currToken.SetError(HtmlToken::UnclosedTag, start);
        return &currToken;
    }

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

    if (('/' == *start) && ('/' == currPos[-1])) { // </foo/>
        currToken.SetError(HtmlToken::InvalidTag, start);
    } else if ('/' == *start) { // </foo>
        currToken.SetValue(HtmlToken::EndTag, start + 1, currPos);
    } else if ('/' == currPos[-1]) { // <foo/>
        currToken.SetValue(HtmlToken::EmptyElementTag, start, currPos - 1);
    } else {
        currToken.SetValue(HtmlToken::StartTag, start, currPos);
    }
    ++currPos;
    return &currToken;
}

HtmlTag FindTag(HtmlToken *tok)
{
    return (HtmlTag)str::FindStrPos(HTML_TAGS_STRINGS, tok->s, GetTagLen(tok));
}

HtmlAttr FindAttr(AttrInfo *attrInfo)
{
    return (HtmlAttr)str::FindStrPos(HTML_ATTRS_STRINGS, attrInfo->name, attrInfo->nameLen);
}

AlignAttr FindAlignAttr(const char *attr, size_t len)
{
    return (AlignAttr)str::FindStrPos(ALIGN_ATTRS_STRINGS, attr, len);
}

bool IsSelfClosingTag(HtmlTag tag)
{
    // TODO: add more tags
    switch (tag) {
    case Tag_Br: case Tag_Img: case Tag_Hr: case Tag_Link:
    case Tag_Meta: case Tag_Pagebreak: case Tag_Mbp_Pagebreak:
        return true;
    default:
        return false;
    }
}

size_t GetTagLen(const HtmlToken *tok)
{
    for (size_t i = 0; i < tok->sLen; i++) {
        if (!IsNameChar(tok->s[i]))
            return i;
    }
    return tok->sLen;
}

static bool IsInlineTag(HtmlTag tag)
{
    switch (tag) {
    case Tag_A: case Tag_Abbr: case Tag_Acronym: case Tag_B:
    case Tag_Br: case Tag_Em: case Tag_Font: case Tag_I:
    case Tag_Img: case Tag_S: case Tag_Small: case Tag_Span:
    case Tag_Strike: case Tag_Strong: case Tag_Sub: case Tag_Sup:
    case Tag_U:
        return true;
    default:
        return false;
    };
}

static void HtmlAddWithNesting(str::Str<char>* out, HtmlToken *tok, HtmlTag tag, size_t nesting)
{
    CrashIf(!tok->IsStartTag() && !tok->IsEndTag() && !tok->IsEmptyElementEndTag());
    bool isInline = IsInlineTag(tag);
    // add a newline before block start tags (unless there already is one)
    bool onNewLine = out->Count() == 0 || out->Last() == '\n';
    if (!onNewLine && !isInline && !tok->IsEndTag()) {
        out->Append('\n');
        onNewLine = true;
    }
    // indent the tag if it starts on a new line
    if (onNewLine) {
        for (size_t i = 0; i < nesting; i++)
            out->Append('\t');
        if (tok->IsEndTag() && nesting > 0)
            out->Pop();
    }
    // output the tag and all its attributes
    out->Append('<');
    if (tok->IsEndTag())
        out->Append('/');
    // TODO: normalize whitespace between attributes?
    out->Append(tok->s, tok->sLen);
    if (tok->IsEmptyElementEndTag())
        out->Append('/');
    out->Append('>');
    // add a newline after block end tags
    if (!isInline && !tok->IsStartTag())
        out->Append('\n');
}

// record the tag for the purpose of building current state
// of html tree
void RecordStartTag(Vec<HtmlTag>* tagNesting, HtmlTag tag)
{
    if (!IsSelfClosingTag(tag))
        tagNesting->Append(tag);
}

// remove the tag from state of html tree
void RecordEndTag(Vec<HtmlTag> *tagNesting, HtmlTag tag)
{
    // when closing a tag, if the top tag doesn't match but
    // there are only potentially self-closing tags on the
    // stack between the matching tag, we pop all of them
    if (tagNesting->Find(tag)) {
        while ((tagNesting->Count() > 0) && (tagNesting->Last() != tag))
            tagNesting->Pop();
    }
    if (tagNesting->Count() > 0) {
        CrashIf(tagNesting->Last() != tag);
        tagNesting->Pop();
    }
}

char *PrettyPrintHtml(const char *s, size_t len, size_t& lenOut)
{
    str::Str<char> res(len);
    Vec<HtmlTag> tagNesting(32);
    HtmlPullParser parser(s, len);
    HtmlToken *t;
    while ((t = parser.Next()) && !t->IsError())
    {
        if (t->IsText())
            res.Append(t->s, t->sLen);
        if (!t->IsTag())
            continue;

        HtmlTag tag = FindTag(t);
        size_t nesting = tagNesting.Count();
        HtmlAddWithNesting(&res, t, tag, nesting);

        if (Tag_NotFound == tag || IsInlineTag(tag))
            continue;

        // update the current state of html tree
        if (t->IsStartTag())
            RecordStartTag(&tagNesting, tag);
        else if (t->IsEndTag())
            RecordEndTag(&tagNesting, tag);
    }
    lenOut = res.Count();
    return res.StealData();
}

#ifdef DEBUG
#include <assert.h>

namespace unittests {

void Test00(const char *s, HtmlToken::TokenType expectedType = HtmlToken::EmptyElementTag) {
    HtmlPullParser parser(s, str::Len(s));
    HtmlToken *t = parser.Next();
    assert(t->type == expectedType);
    assert(t->NameIs("p"));
    HtmlTag tag = FindTag(t);
    assert(Tag_P == tag);
    AttrInfo *a = t->NextAttr();
    assert(a->NameIs("a1"));
    assert(a->ValIs(">"));
    a = t->NextAttr();
    assert(a->NameIs("foo"));
    assert(a->ValIs("bar"));
    a = t->NextAttr();
    assert(!a);
    t = parser.Next();
    assert(!t);
}

}

void HtmlPullParser_UnitTests()
{
    unittests::Test00("<p a1='>' foo=bar />");
    unittests::Test00("<p a1 ='>'     foo=\"bar\"/>");
    unittests::Test00("<p a1=  '>' foo=bar>", HtmlToken::StartTag);
}

#endif
