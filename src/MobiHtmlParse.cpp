/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MobiHtmlParse.h"
#include "Vec.h"
#include "FileUtil.h"
#include "HtmlPullParser.h"
#include "StrUtil.h"
#include "Varint.h"

// TODO: internal format was a bad idea. Instead of re-encoding html into
// slightly-different-but-almost-the-same format, we should just have an
// iterator over html that returns consequitive instructions. It'll be
// less code, less memory used and more convenient api to use

/*
Converts mobi html to our internal format optimized for further layout/display.

Our format can be thought of as a stream of Virtual Machine instructions.

A VM instruction is either a string to be displayed or a formatting code (possibly
with some additional data for this formatting code).

We tokenize strings into words during this process because words are basic units
of layout. During laytou/display, a string implies a space (unless string is followed
by a html tag (formatting code) like Tag_Font etc., in which case it's elided).

Our format is a sequence of instructions encoded as sequence of bytes.

Conceptually, the first part of an instruction is a 32-bit word.

If first bit of that word is 0, the instruction is a string to
be drawn. The rest of the word encodes length of the string, followed
by string bytes (in utf8).

If first bit of the word is 1, the instruction is a tag and the
rest of the word encodes HtmlTag enum. 

Tag is followed by a byte:
* if bit 0 is set, it's an end tag, otherwise a start (or self-closing tag)
* if bit 1 is set, the tag has attributes

If there are attributes, they follow the tag.

The 32-bit word is encoded as a variant.

TODO: this encoding doesn't support a case of formatting code inside a string e.g.
"partially<i>itallic</i>". We can solve that by introducing a formatting code
denoting a string with embedded formatting codes. It's length would be 16-bit
(for simplicity of construction) telling the total size of nested data. I don't
think it happens very often in practice, though.
*/

static inline void EmitVarint(Vec<uint8_t>* out, uint32_t v)
{
    uint8_t *data = out->EnsureEndPadding(4);
    uint8_t *dataEnd = (uint8_t*)Varint::Encode32((char*)data, v);
    CrashAlwaysIf(dataEnd - data > 4);
    out->IncreaseLen(dataEnd - data);
}

void EncodeParsedElementString(Vec<uint8_t>* out, const uint8_t *s, size_t sLen)
{
    // first bit is 0, followed by sLen
    uint32_t v = (uint32_t)sLen;
    v = v << 1;
    EmitVarint(out, v);
    out->Append((uint8_t*)s, sLen);
}

static void EncodeParsedElementInt(Vec<uint8_t>* out, int n)
{
    // first bit is 1, followed by tag
    uint32_t v = (uint32_t)n;
    v = (v << 1) | 1;
    EmitVarint(out, v);
}

static void EncodeParsedElementTag(Vec<uint8_t>* out, HtmlTag tag)
{
    EncodeParsedElementInt(out, tag);
}

static void EncodeParsedElementAttr(Vec<uint8_t>* out, HtmlAttr attr)
{
    EncodeParsedElementInt(out, attr);
}

ParsedElement *DecodeNextParsedElement(const uint8_t* &s, const uint8_t *end)
{
    static ParsedElement res;
    uint32_t v;
    if (s == end)
        return NULL;
    CrashAlwaysIf(s > end);
    const char *sEnd = Varint::Parse32WithLimit((const char*)s, (const char*)end, &v);
    s = (const uint8_t*)sEnd;
    if ((v & 0x1) != 0) {
        res.type = ParsedElInt;
        v = v >> 1;
        res.tag = (HtmlTag)v;
        CrashAlwaysIf(res.tag >= Tag_Last);
    } else {
        res.type = ParsedElString;
        v = v >> 1;
        CrashAlwaysIf(v == 0);
        res.sLen = v;
        res.s = s;
        s += res.sLen;
        CrashAlwaysIf(s > end);
    }
    return &res;
}

static void EmitTag(Vec<uint8_t> *out, HtmlTag tag, bool isEndTag, bool hasAttributes)
{
    CrashAlwaysIf(hasAttributes && isEndTag);
    EncodeParsedElementTag(out, tag);
    uint8_t tagPostfix = 0;
    if (isEndTag)
        tagPostfix |= IS_END_TAG_MASK;
    if (hasAttributes)
        tagPostfix |= HAS_ATTR_MASK;
    out->Append(tagPostfix);
}

// Tokenize text into words and serialize those words to
// our layout/display format
// Note: I'm collapsing multiple whitespaces. This might
// not be the right thing to do (e.g. when showing source
// code snippets)
static void EmitText(Vec<uint8_t>* out, HtmlToken *t)
{
    CrashIf(!t->IsText());
    const uint8_t *end = t->s + t->sLen;
    const uint8_t *curr = t->s;
    SkipWs(curr, end);
    while (curr < end) {
        const uint8_t *currStart = curr;
        SkipNonWs(curr, end);
        size_t len = curr - currStart;
        if (len > 0)
            EncodeParsedElementString(out, currStart, len);
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
static bool EmitAttribute(Vec<uint8_t> *out, HtmlAttr attr, const uint8_t *attrVal, size_t attrValLen)
{
    const char *validValues = NULL;
    for (size_t i = 0; i < dimof(gValidAttrValues); i++) {
        if (attr == gValidAttrValues[i].attr) {
            validValues = gValidAttrValues[i].validValues;
            break;
        }
    }

    if (NULL != validValues) {
        int valIdx = FindStrPos(validValues, (const char*)attrVal, attrValLen);
        if (-1 == valIdx)
            return false;
        EncodeParsedElementAttr(out, attr);
        EncodeParsedElementInt(out, valIdx);
        return true;
    }
    EncodeParsedElementAttr(out, attr);
    EncodeParsedElementString(out, attrVal, attrValLen);
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
    const uint8_t *s = t->s;
    const uint8_t *end = s + t->sLen;
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
    EncodeParsedElementInt(out, (int)attrCount);
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
static bool IgnoreTag(const uint8_t *s, size_t sLen)
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

    // TODO: this could probably be data driven i.e. any special
    // processing for a given tag can be describes with the following
    // data:
    // * should we ignore end tag (e.g. for <hr> tag). This probably is
    //   the same as IsSelfClosingTag().
    // * a list of acceptable attributes for this tag
    if (Tag_P == tag) {
        EmitTagP(state->out, t);
        return;
    } else if (Tag_Hr == tag) {
        EmitTagHr(state->out, t);
        return;
    }
    // TODO: handle other tags
    EmitTag(state->out, tag, t->IsEndTag(), false);
    //EmitTag(state->out, tag, t);
}

// convert mobi html to a format optimized for layout/display
// returns NULL in case of an error
Vec<uint8_t> *MobiHtmlToDisplay(const uint8_t *s, size_t sLen)
{
    Vec<uint8_t> *res = new Vec<uint8_t>(sLen); // avoid re-allocations by pre-allocating expected size

    // tagsStack represents the current html tree nesting at current
    // parsing point. We add open tag to the stack when we encounter
    // them and we remove them from the stack when we encounter
    // its closing tag.
    Vec<HtmlTag> tagNesting(256);

    ConverterState state = { res, &tagNesting };

    HtmlPullParser parser(s, strlen((const char*)s));

    for (;;)
    {
        HtmlToken *t = parser.Next();
        if (!t)
            break;
        if (t->IsError()) {
            delete res;
            return NULL;
        }
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
