/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiHtmlParse.h"
#include "Vec.h"
#include "FileUtil.h"

/*
Converts mobi html to our internal format optimized for further layout/display.

Our format can be thought of as a stream of Virtual Machine instructions.

A VM instruction is either a string to be displayed or a formatting code (possibly
with some additional data for this formatting code).

We tokenize strings into words during this process because words are basic units
of layout. During laytou/display, a string implies a space (unless string is followed
by a html tag (formatting code) like Tag_Font etc., in which case it's elided).

Our format is a sequence of instructions encoded as sequence of bytes.

The last bytes are reserved for formatting code. 

Bytes smaller than that encode length of the string followed by bytes of the string.

If string length is >= Tag_First, we emit Tag_First followed by
a 2-byte length (i.e. a string cannot be longer than 65 kB).

Strings are utf8.

Tag is followed by a byte:
* if bit 0 is set, it's an end tag, otherwise a start (or self-closing tag)
* if bit 1 is set, the tag has attributes

If there are attributes, they follow the tag.

TODO: this encoding doesn't support a case of formatting code inside a string e.g.
"partially<i>itallic</i>". We can solve that by introducing a formatting code
denoting a string with embedded formatting codes. It's length would be 16-bit
(for simplicity of construction) telling the total size of nested data. I don't
think it happens very often in practice, though.
*/

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

// *sPtr must be zero-terminated
static bool SkipUntil(uint8_t **sPtr, char c)
{
    return SkipUntil((char**)sPtr, c);
}

static bool SkipUntil(uint8_t*& s, uint8_t *end, uint8_t c)
{
    while ((s < end) && (*s != c)) {
        ++s;
    }
    return *s == c;
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

static int IsName(int c)
{
    return c == '.' || c == '-' || c == '_' || c == ':' ||
        (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z');
}

// skip all html tag or attribute chatacters
static void SkipName(uint8_t*& s, uint8_t *end)
{
    while ((s < end) && IsName(*s)) {
        s++;
    }
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

struct AttrInfo {
    uint8_t *   name;
    size_t      nameLen;
    uint8_t *   val;
    size_t      valLen;
};

// at this point we're either after tag name or
// after previous attribute
// We expect: 
// whitespace | attribute name | = | attribute value
// where both attribute name and attribute value can
// be quoted
// Not multi-thread safe (uses static buffer)
AttrInfo *GetNextAttr(uint8_t *&s, uint8_t *end)
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
    uint8_t quoteChar = *s;
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

// strings is an array of 0-separated strings consequitevely laid out
// in memory. This functions find the position of str in this array,
// -1 means not found. The search is case-insensitive
static int FindStrPos(const char *strings, char *str, size_t len)
{
    const char *curr = strings;
    char *end = str + len;
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
        char *s = str;
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

HtmlTag FindTag(char *tag, size_t len)
{
    return (HtmlTag)FindStrPos(HTML_TAGS_STRINGS, tag, len);
}

static HtmlAttr FindAttr(char *attr, size_t len)
{
    return (HtmlAttr)FindStrPos(HTML_ATTRS_STRINGS, attr, len);
}

static AlignAttr FindAlignAttr(char *attr, size_t len)
{
    return (AlignAttr)FindStrPos(ALIGN_ATTRS_STRINGS, attr, len);
}

static bool IsSelfClosingTag(HtmlTag tag)
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

static void EmitByte(Vec<uint8_t> *out, uint8_t v)
{
    out->Append(v);
}

static void EmitTag(Vec<uint8_t> *out, HtmlTag tag, bool isEndTag, bool hasAttributes)
{
    CrashAlwaysIf(hasAttributes && isEndTag);
    out->Append((uint8_t)255-tag);
    uint8_t tagPostfix = 0;
    if (isEndTag)
        tagPostfix |= IS_END_TAG_MASK;
    if (hasAttributes)
        tagPostfix |= HAS_ATTR_MASK;
    out->Append(tagPostfix);
}

static void EmitWord(Vec<uint8_t>* out, uint8_t *s, size_t len)
{
    // TODO: convert html entities to text
    if (len >= Tag_First) {
        // TODO: emit Tag_First and then len as 2 bytes
        // TODO: fix EmitAttribute() when this limit is fixed
        // TODO: fix DecodeAttributes() when this limit is fixed
        return;
    }
    out->Append((uint8_t)len);
    out->Append(s, len);
}

// Tokenize text into words and serialize those words to
// our layout/display format
// Note: I'm collapsing multiple whitespaces. This might
// not be the right thing to do (e.g. when showing source
// code snippets)
static void EmitText(Vec<uint8_t>* out, HtmlToken *t)
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
            EmitWord(out, currStart, len);
        SkipWs(curr, end);
    }
}

static bool IsAllowedAttr(HtmlAttr *allowed, HtmlAttr attr)
{
    while (-1 != *allowed) {
        if (attr == *allowed)
            return true;
        ++allowed;
    }
    return false;
}

static struct {
    HtmlAttr attr;
    const char *validValues;
} gValidAttrValues[] = {
    { Attr_Align, ALIGN_ATTRS_STRINGS } };

bool AttrHasEnumVal(HtmlAttr attr)
{
    for (size_t i = 0; i < dimof(gValidAttrValues); i++) {
        if (gValidAttrValues[i].attr == attr)
            return true;
    }
    return false;
}

// Emit a given attribute and its value.
// Attribute value either belongs to a set of known value (in which case we
// encode it as a byte) or an arbitrary string, in which case we encode it as
// as a string.
// Returns false if we couldn't emit the attribute (e.g. because we expected
// a known attribute value and it didn' match any).
static bool EmitAttribute(Vec<uint8_t> *out, HtmlAttr attr, uint8_t *attrVal, size_t attrValLen)
{
    const char *validValues = NULL;
    for (size_t i = 0; i < dimof(gValidAttrValues); i++) {
        if (attr == gValidAttrValues[i].attr) {
            validValues = gValidAttrValues[i].validValues;
            break;
        }
    }

    if (NULL != validValues) {
        int valIdx = FindStrPos(validValues, (char*)attrVal, attrValLen);
        if (-1 == valIdx)
            return false;
        out->Append((uint8_t)attr);
        out->Append((uint8_t)valIdx);
        return true;
    }
    // TODO: remove this check when EmitWord() can handle larger strings
    if (attrValLen > Tag_First)
        return false;
    out->Append((uint8_t)attr);
    EmitWord(out, attrVal, attrValLen);
    return true;
}

#if 0
void DumpAttr(uint8_t *s, size_t sLen)
{
    static Vec<char *> seen;
    char *sCopy = str::DupN((char*)s, sLen);
    bool didSee = false;
    for (size_t i = 0; i < seen.Count(); i++) {
        char *tmp = seen.At(i);
        if (str::EqI(sCopy, tmp)) {
            didSee = true;
            break;
        }
    }
    if (didSee) {
        free(sCopy);
        return;
    }
    seen.Append(sCopy);
    printf("%s\n", sCopy);
}
#endif

static bool EmitAttributes(Vec<uint8_t> *out, HtmlToken *t, HtmlAttr *allowedAttributes)
{
    AttrInfo *attrInfo;
    HtmlAttr attr;
    uint8_t *s = t->s;
    uint8_t *end = s + t->sLen;
    s += GetTagLen(s, t->sLen);

    // we need to cache attribute info because we first
    // need to emit attribute count and we don't know it
    // yet here
    Vec<uint8_t> attributesEncoded;
    size_t attrCount = 0;

    for (;;) {
        attrInfo = GetNextAttr(s, end);
        if (!attrInfo)
            break;
        attr = FindAttr((char*)attrInfo->name, attrInfo->nameLen);
#if 0
        if (Attr_NotFound == attr)
            DumpAttr(attrInfo->name, attrInfo->nameLen);
#endif
        CrashAlwaysIf(Attr_NotFound == attr);
        if (!IsAllowedAttr(allowedAttributes, attr))
            continue;
        if (EmitAttribute(&attributesEncoded, attr, attrInfo->val, attrInfo->valLen))
            ++attrCount;
    }
    if (0 == attrCount)
        return false;
    CrashAlwaysIf(attributesEncoded.Count() == 0);
    out->Append((uint8_t)attrCount);
    out->Append(attributesEncoded.LendData(), attributesEncoded.Count());
    return true;
}

static void EmitTagP(Vec<uint8_t>* out, HtmlToken *t)
{
    static HtmlAttr validPAttrs[] = { Attr_Align, Attr_NotFound };
    if (t->IsEmptyElementEndTag()) {
        // TODO: should I generate both start and end tags?
        return;
    }

    if (t->IsEndTag()) {
        EmitTag(out, Tag_P, t->IsEndTag(), false);
    } else {
        CrashAlwaysIf(!t->IsStartTag());
        Vec<uint8_t> attrs;
        bool hasAttributes = EmitAttributes(&attrs, t, validPAttrs);
        EmitTag(out, Tag_P, t->IsEndTag(), hasAttributes);
        if (hasAttributes)
            out->Append(attrs.LendData(), attrs.Count());
    }
}

static void EmitTagHr(Vec<uint8_t>* out, HtmlToken *t)
{
    if (t->IsEndTag()) {
        // we shouldn't be getting this at all, so ignore it
        return;
    }
    // note: hr tag sometimes has width attribute but
    // I've only seen it set to 100%, so I just assume
    // all hr are full page width.
    EmitTag(out, Tag_Hr, false, false);
}

#if 0
static void EmitTag(Vec<uint8_t>* out, HtmlTag tag, HtmlToken *t)
{
    static HtmlAttr validAttrs[] = { Attr_NotFound };
    if (t->IsEmptyElementEndTag()) {
        // TODO: should I generate both start and end tags?
        return;
    }
    if (t->IsEndTag()) {
        EmitTag(out, tag, t->IsEndTag(), false);
    } else {
        CrashAlwaysIf(!t->IsStartTag());
        Vec<uint8_t> attrs;
        bool hasAttributes = EmitAttributes(&attrs, t, validAttrs);
        EmitTag(out, tag, t->IsEndTag(), hasAttributes);
        if (hasAttributes)
            out->Append(attrs.LendData(), attrs.Count());
    }
}
#endif

struct ConverterState {
    Vec<uint8_t> *out;
    Vec<HtmlTag> *tagNesting;
    Vec<uint8_t> *html; // prettified html
};

// record the tag for the purpose of building current state
// of html tree
static void RecordStartTag(Vec<HtmlTag>* tagNesting, HtmlTag tag)
{
    if (IsSelfClosingTag(tag))
        return;
    tagNesting->Append(tag);
}

// remove the tag from state of html tree
static void RecordEndTag(Vec<HtmlTag> *tagNesting, HtmlTag tag)
{
    // TODO: this logic might need to be a bit more complicated
    // e.g. when closing a tag, if the top tag doesn't match
    // but there are only potentially self-closing tags
    // on the stack between the matching tag, we should pop
    // all of them
    if (tagNesting->Count() > 0)
        tagNesting->Pop();
}

// tags that I want to explicitly ignore and not define
// HtmlTag enums for them
// One file has a bunch of st1:* tags (st1:city, st1:place etc.)
static bool IgnoreTag(uint8_t *s, size_t sLen)
{
    if (sLen >= 4 && s[3] == ':' && s[0] == 's' && s[1] == 't' && s[2] == '1')
        return true;
    // no idea what "o:p" is
    if (sLen == 3 && s[1] == ':' && s[0] == 'o'  && s[2] == 'p')
        return true;
    return false;
}

static void HandleTag(ConverterState *state, HtmlToken *t)
{
    CrashIf(!t->IsTag());

    // HtmlToken string includes potential attributes,
    // get the length of just the tag
    size_t tagLen = GetTagLen(t->s, t->sLen);
    if (IgnoreTag(t->s, tagLen))
        return;

    HtmlTag tag = FindTag((char*)t->s, tagLen);
    // TODO: ignore instead of crashing once we're satisfied we covered all the tags
    CrashIf(tag == Tag_NotFound);

    // update the current state of html tree
    if (t->IsStartTag())
        RecordStartTag(state->tagNesting, tag);
    else if (t->IsEndTag())
        RecordEndTag(state->tagNesting, tag);

    if (Tag_P == tag) {
        EmitTagP(state->out, t);
        return;
    }
    // TODO: handle other tags
    EmitTag(state->out, tag, t->IsEndTag(), false);
    //EmitTag(state->out, tag, t);
}

static void HtmlAddWithNesting(Vec<uint8_t>* html, HtmlTag tag, bool isStartTag, size_t nesting, uint8_t *s, size_t sLen)
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
            html->Append(' ');
        }
    }
    html->Append(s, sLen);
    if (isInline || isStartTag)
        return;
    html->Append('\n');
}

static void PrettifyHtml(ConverterState *state, HtmlToken *t)
{
    if (!state->html)
        return;

    size_t nesting = state->tagNesting->Count();
    if (t->IsText()) {
        state->html->Append(t->s, t->sLen);
        //HtmlAddWithNesting(state->html, t->tag, nesting, t->s, t->sLen);
        return;
    }

    if (!t->IsTag())
        return;

    HtmlTag tag = FindTag((char*)t->s, t->sLen);
    if (t->IsEmptyElementEndTag()) {
        HtmlAddWithNesting(state->html, tag, false, nesting, t->s - 1, t->sLen + 3);
        return;
    }

    if (t->IsStartTag()) {
        HtmlAddWithNesting(state->html, tag, true, nesting, t->s - 1, t->sLen + 2);
        return;
    }

    if (t->IsEndTag()) {
        if (nesting > 0)
            --nesting;
        HtmlAddWithNesting(state->html, tag, false, nesting, t->s - 2, t->sLen + 3);
    }
}

// convert mobi html to a format optimized for layout/display
// returns NULL in case of an error
Vec<uint8_t> *MobiHtmlToDisplay(uint8_t *s, size_t sLen, Vec<uint8_t> *html)
{
    Vec<uint8_t> *res = new Vec<uint8_t>(sLen); // avoid re-allocations by pre-allocating expected size

    // tagsStack represents the current html tree nesting at current
    // parsing point. We add open tag to the stack when we encounter
    // them and we remove them from the stack when we encounter
    // its closing tag.
    Vec<HtmlTag> tagNesting(256);

    ConverterState state = { res, &tagNesting, html };

    HtmlPullParser parser(s);

    for (;;)
    {
        HtmlToken *t = parser.Next();
        if (!t)
            break;
        if (t->IsError()) {
            delete res;
            return NULL;
        }
        PrettifyHtml(&state, t);
        // TODO: interpret the tag
        if (t->IsTag()) {
            HandleTag(&state, t);
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
    return res;
}
