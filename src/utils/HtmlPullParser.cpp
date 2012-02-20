/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Allocator.h"

#include "HtmlPullParser.h"
#include "StrUtil.h"

// map of entity names to their Unicde runes, based on
// http://en.wikipedia.org/wiki/List_of_XML_and_HTML_character_entity_references
// the order of strings in gHtmlEntityNames corresponds to
// order of Unicode runes in gHtmlEntityRunes
static const char *gHtmlEntityNames = "aacute\0aacute\0acirc\0acirc\0acute\0aelig\0aelig\0agrave\0agrave\0alefsym\0alpha\0alpha\0amp\0and\0ang\0apos\0aring\0aring\0asymp\0atilde\0atilde\0auml\0auml\0bdquo\0beta\0beta\0brvbar\0bull\0cap\0ccedil\0ccedil\0cedil\0cent\0chi\0chi\0circ\0clubs\0cong\0copy\0crarr\0cup\0curren\0dagger\0dagger\0darr\0darr\0deg\0delta\0delta\0diams\0divide\0eacute\0eacute\0ecirc\0ecirc\0egrave\0egrave\0empty\0emsp\0ensp\0epsilon\0epsilon\0equiv\0eta\0eta\0eth\0eth\0euml\0euml\0euro\0exist\0fnof\0forall\0frac12\0frac14\0frac34\0frasl\0gamma\0gamma\0ge\0gt\0harr\0harr\0hearts\0hellip\0iacute\0iacute\0icirc\0icirc\0iexcl\0igrave\0igrave\0image\0infin\0int\0iota\0iota\0iquest\0isin\0iuml\0iuml\0kappa\0kappa\0lambda\0lambda\0lang\0laquo\0larr\0larr\0lceil\0ldquo\0le\0lfloor\0lowast\0loz\0lrm\0lsaquo\0lsquo\0lt\0macr\0mdash\0micro\0middot\0minus\0mu\0mu\0nabla\0nbsp\0ndash\0ne\0ni\0not\0notin\0nsub\0ntilde\0ntilde\0nu\0nu\0oacute\0oacute\0ocirc\0ocirc\0oelig\0oelig\0ograve\0ograve\0oline\0omega\0omega\0omicron\0omicron\0oplus\0or\0ordf\0ordm\0oslash\0oslash\0otilde\0otilde\0otimes\0ouml\0ouml\0para\0part\0permil\0perp\0phi\0phi\0pi\0pi\0piv\0plusmn\0pound\0prime\0prime\0prod\0prop\0psi\0psi\0quot\0radic\0rang\0raquo\0rarr\0rarr\0rceil\0rdquo\0real\0reg\0rfloor\0rho\0rho\0rlm\0rsaquo\0rsquo\0sbquo\0scaron\0scaron\0sdot\0sect\0shy\0sigma\0sigma\0sigmaf\0sim\0spades\0sub\0sube\0sum\0sup\0sup1\0sup2\0sup3\0supe\0szlig\0tau\0tau\0there4\0theta\0theta\0thetasym\0thinsp\0thorn\0thorn\0tilde\0times\0trade\0uacute\0uacute\0uarr\0uarr\0ucirc\0ucirc\0ugrave\0ugrave\0uml\0upsih\0upsilon\0upsilon\0uuml\0uuml\0weierp\0xi\0xi\0yacute\0yacute\0yen\0yuml\0yuml\0zeta\0zeta\0zwj\0zwnj\0";

static uint16 gHtmlEntityRunes[] = { 193, 225, 194, 226, 180, 198, 230, 192, 224, 8501, 913, 945, 38, 8743, 8736, 39, 197, 229, 8776, 195, 227, 196, 228, 8222, 914, 946, 166, 8226, 8745, 199, 231, 184, 162, 935, 967, 710, 9827, 8773, 169, 8629, 8746, 164, 8224, 8225, 8595, 8659, 176, 916, 948, 9830, 247, 201, 233, 202, 234, 200, 232, 8709, 8195, 8194, 917, 949, 8801, 919, 951, 208, 240, 203, 235, 8364, 8707, 402, 8704, 189, 188, 190, 8260, 915, 947, 8805, 62, 8596, 8660, 9829, 8230, 205, 237, 206, 238, 161, 204, 236, 8465, 8734, 8747, 921, 953, 191, 8712, 207, 239, 922, 954, 923, 955, 9001, 171, 8592, 8656, 8968, 8220, 8804, 8970, 8727, 9674, 8206, 8249, 8216, 60, 175, 8212, 181, 183, 8722, 924, 956, 8711, 160, 8211, 8800, 8715, 172, 8713, 8836, 209, 241, 925, 957, 211, 243, 212, 244, 338, 339, 210, 242, 8254, 937, 969, 927, 959, 8853, 8744, 170, 186, 216, 248, 213, 245, 8855, 214, 246, 182, 8706, 8240, 8869, 934, 966, 928, 960, 982, 177, 163, 8242, 8243, 8719, 8733, 936, 968, 34, 8730, 9002, 187, 8594, 8658, 8969, 8221, 8476, 174, 8971, 929, 961, 8207, 8250, 8217, 8218, 352, 353, 8901, 167, 173, 931, 963, 962, 8764, 9824, 8834, 8838, 8721, 8835, 185, 178, 179, 8839, 223, 932, 964, 8756, 920, 952, 977, 8201, 222, 254, 732, 215, 8482, 218, 250, 8593, 8657, 219, 251, 217, 249, 168, 978, 933, 965, 220, 252, 8472, 926, 958, 221, 253, 165, 255, 376, 918, 950, 8205, 8204 };

// returns -1 if didn't find
static int HtmlEntityNameToRune(const char *name, size_t nameLen)
{
    int pos = str::FindStrPos(gHtmlEntityNames, name, nameLen);
    if (-1 == pos)
        return -1;
    return (int)gHtmlEntityRunes[pos];
}

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

void MemAppend(char *& dst, const char *s, size_t len)
{
    if (0 == len)
        return;
    memcpy(dst, s, len);
    dst += len;
}

// rune is a unicode character (borrowing terminology from Go)
// We encode rune as utf8 to dst buffer and advance dst pointer.
// The caller must ensure there is enough free space (6 bytes)
// in dst
static void Utf8Encode(char *& dst, int rune)
{
    uint8 *tmp = (uint8*)dst;
    if (rune <= 0x7f)
        *tmp++ = (uint8)rune;
    else {
        // TODO: implement me
        CrashIf(true);
    }
    dst = (char*)tmp;
}

// if "&foo;" was the entity, s points at the char
// after '&' and len is lenght of the string (3 in case of "foo")
// the caller must ensure that there is terminating ';'
static int ResolveHtmlEntity(const char *s, size_t len)
{
    int rune;
    if (str::Parse(s, "%d;", &rune))
        return rune;
    if (str::Parse(s, "#%x;", &rune))
        return rune;
    return HtmlEntityNameToRune(s, len);
}

// if s doesn't contain html entities, we just return it
// if it contains html entities, we'll return string allocated
// with alloc in which entities are converted to their values
// Entities are encoded as utf8 in the result.
// alloc can be NULL, in which case we'll allocate with malloc()
const char *ResolveHtmlEntities(const char *s, const char *end, Allocator *alloc)
{
    char *        res = NULL;
    size_t        resLen;
    char *        dst;
    const char *  colon;

    const char *curr = s;
    for (;;) {
        bool found = SkipUntil(curr, end, '&');
        if (!found) {
            if (!res)
                return s;
            // copy the remaining string
            MemAppend(dst, s, end - s);
            break;
        }
        if (!res) {
            // allocate memory for the result string
            // I'm banking that text after resolving entities will
            // be smaller than the original
            resLen = end - s + 8; // +8 just in case
            res = (char*)Allocator::Alloc(alloc, resLen);
            dst = res;
        }
        MemAppend(dst, s, curr - s);
        // curr points at '&'. Make sure there is ';' within source string
        colon = curr;
        found = SkipUntil(colon, end, ';');
        if (!found) {
            MemAppend(dst, curr, end - curr);
            break;
        }
        int rune = ResolveHtmlEntity(curr+1, colon - curr - 1);
        if (-1 == rune) {
            // unknown entity, copy the string verbatim
            MemAppend(dst, curr, colon - curr + 1);
        } else {
            Utf8Encode(dst, rune);
        }
        curr = colon + 1;
        s = curr;
    }
    *dst = 0;
    CrashIf(dst >= res + resLen);
    return (const char*)res;
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

AttrInfo *HtmlToken::GetAttrByName(const char *name)
{
    nextAttr = NULL; // start from the beginning
    for (AttrInfo *a = NextAttr(); a; a = NextAttr()) {
        if (a->NameIs(name))
            return a;
    }
    return NULL;
}

// We expect:
// whitespace | attribute name | = | attribute value
// where attribute value can be quoted
AttrInfo *HtmlToken::NextAttr()
{
    // start after the last attribute found (or the beginning)
    const char *curr = nextAttr;
    if (!curr)
        curr = s + GetTagLen(this);
    const char *end = s + sLen;

    // parse attribute name
    SkipWs(curr, end);
    if (curr == end) {
NoNextAttr:
        nextAttr = NULL;
        return NULL;
    }
    attrInfo.name = curr;
    SkipName(curr, end);
    attrInfo.nameLen = curr - attrInfo.name;
    if (0 == attrInfo.nameLen)
        goto NoNextAttr;
    SkipWs(curr, end);
    if ((curr == end) || ('=' != *curr)) {
        // attributes without values get their names as value in HTML
        attrInfo.val = attrInfo.name;
        attrInfo.valLen = attrInfo.nameLen;
        nextAttr = curr;
        return &attrInfo;
    }

    // parse attribute value
    ++curr; // skip '='
    SkipWs(curr, end);
    if (curr == end) {
        // attribute with implicit empty value
        attrInfo.val = curr;
        attrInfo.valLen = 0;
    } else if (('\'' == *curr) || ('\"' == *curr)) {
        // attribute with quoted value
        ++curr;
        attrInfo.val = curr;
        if (!SkipUntil(curr, end, *(curr - 1)))
            goto NoNextAttr;
        attrInfo.valLen = curr - attrInfo.val;
        ++curr;
    } else {
        attrInfo.val = curr;
        SkipNonWs(curr, end);
        attrInfo.valLen = curr - attrInfo.val;
    }
    nextAttr = curr;
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

static void Test00(const char *s, HtmlToken::TokenType expectedType = HtmlToken::EmptyElementTag) {
    HtmlPullParser parser(s, str::Len(s));
    HtmlToken *t = parser.Next();
    assert(t->type == expectedType);
    assert(t->NameIs("p"));
    HtmlTag tag = FindTag(t);
    assert(Tag_P == tag);
    AttrInfo *a = t->GetAttrByName("a1");
    assert(a->NameIs("a1"));
    assert(a->ValIs(">"));

#if 0
    a = t->NextAttr();
    assert(a->NameIs("a1"));
    assert(a->ValIs(">"));
    a = t->NextAttr();
    assert(a->NameIs("foo"));
    assert(a->ValIs("bar"));
    a = t->NextAttr();
    assert(!a);
#endif
    t = parser.Next();
    assert(!t);
}

static void HtmlEntities()
{
    struct {
        const char *s; int rune;
    } entities[] = {
        { "&TIMES;", 215 },
        { "&aelig;", 198 },
        { "&zwnj;", 8204 },
        { "&58;", 58 },
        { "&32783;", 32783 },
        { "&#20;", 32 },
        { "&#Af34;", 44852 },
        { "&Auml;", 196 },
        { "&a3;", -1 },
        { "&#z312;", -1 },
        { "&aer;", -1 }
    };
    for (size_t i = 0; i < dimof(entities); i++ ) {
        const char *s = entities[i].s;
        int got = ResolveHtmlEntity(s + 1, str::Len(s) - 2);
        assert(got == entities[i].rune);
    }
    const char *unchanged[] = {
        "foo", "", " as;d "
    };
    for (size_t i = 0; i < dimof(unchanged); i++) {
        const char *s = unchanged[i];
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(res == s);
    }

    struct {
        const char *s; const char *res;
    } changed[] = {
        // implementation detail: if there is '&' in the string
        // we always allocate, even if it isn't a valid entity
        { "a&12", "a&12" },
        { "a&#30", "a&#30" },

        { "&32;b", " b" },
        { "&#20;ra", " ra" },
        { "&lt;", "<" },
        { "a&amp; &32;to&#20;end", "a&  to end" }
    };
    for (size_t i = 0; i < dimof(changed); i++) {
        const char *s = changed[i].s;
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(str::Eq(res, changed[i].res));
        free((void*)res);
    }
}

}

void HtmlPullParser_UnitTests()
{
    unittests::Test00("<p a1='>' foo=bar />");
    unittests::Test00("<p a1 ='>'     foo=\"bar\"/>");
    unittests::Test00("<p a1=  '>' foo=bar>", HtmlToken::StartTag);
    unittests::HtmlEntities();
}

#endif
