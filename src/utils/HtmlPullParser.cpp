/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "HtmlParserLookup.h"
#include "HtmlPullParser.h"

// returns -1 if didn't find
int HtmlEntityNameToRune(Str name) {
    return FindHtmlEntityRune(name);
}

#define MAX_ENTITY_NAME_LEN 8

// A unicode version of HtmlEntityNameToRune. It's safe because
// entity names only contain ascii (<127) characters so if a simplistic
// conversion from unicode to ascii succeeds, we can use ascii
// version, otherwise it wouldn't match anyway
// returns -1 if didn't find
int HtmlEntityNameToRune(WStr name) {
    char asciiName[MAX_ENTITY_NAME_LEN]{};
    if ((size_t)name.len > MAX_ENTITY_NAME_LEN) {
        return -1;
    }
    for (int i = 0; i < name.len; i++) {
        if (name.s[i] > 127) {
            return -1;
        }
        asciiName[i] = (char)name.s[i];
    }
    return FindHtmlEntityRune(Str(asciiName, name.len));
}

bool SkipUntil(Str s, int& off, char c) {
    while (off < s.len && s.s[off] != c) {
        ++off;
    }
    return off < s.len;
}

bool SkipUntil(Str s, int& off, Str term) {
    for (; off < s.len; off++) {
        if (off + term.len <= s.len && str::StartsWith(Str(s.s + off, s.len - off), term)) {
            return true;
        }
    }
    return false;
}

// return true if skipped
bool SkipWs(Str s, int& off) {
    int start = off;
    while (off < s.len && str::IsWs(s.s[off])) {
        ++off;
    }
    return start != off;
}

// return true if skipped
bool SkipNonWs(Str s, int& off) {
    int start = off;
    while (off < s.len && !str::IsWs(s.s[off])) {
        ++off;
    }
    return start != off;
}

static bool IsNameChar(char c) {
    return c == '.' || c == '-' || c == '_' || c == ':' || str::IsDigit(c) || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

static bool IsValidTagStart(char c) {
    return c == '/' || c == '!' || c == '?' || IsNameChar(c);
}

// skip all html tag or attribute characters
static void SkipName(Str s, int& off) {
    while (off < s.len && IsNameChar(s.s[off])) {
        off++;
    }
}

// return true if s consists only of whitespace
bool IsSpaceOnly(Str s) {
    int off = 0;
    SkipWs(s, off);
    return off == s.len;
}

static void MemAppend(char* buf, int& off, Str src) { // str-port: owned heap write buffer
    if (!src) {
        return;
    }
    memcpy(buf + off, src.s, src.len);
    off += src.len;
}

// if "&foo;" was the entity, str points at the char after '&'
// returns a slice starting after the entity, or empty on failure
Str ResolveHtmlEntity(Str str, int& rune) {
    Str entEnd = str::Parse(str, str.len, "#%d%?;", &rune);
    if (!str::IsNull(entEnd)) {
        return entEnd;
    }
    entEnd = str::Parse(str, str.len, "#x%x%?;", &rune);
    if (!str::IsNull(entEnd)) {
        return entEnd;
    }

    // go to the end of a potential named entity
    int entLen = 0;
    while (entLen < str.len && isalnum((u8)str.s[entLen])) {
        entLen++;
    }
    if (entLen > 0) {
        rune = HtmlEntityNameToRune(Str(str.s, entLen));
        if (-1 == rune) {
            return {};
        }
        int endOff = entLen;
        // skip the trailing colon - if there is one
        if (endOff < str.len && str.s[endOff] == ';') {
            endOff++;
        }
        return Str(str.s + endOff, str.len - endOff);
    }

    rune = -1;
    return {};
}

// if s doesn't contain html entities, we just return it
// if it contains html entities, we'll return string allocated
// with alloc in which entities are converted to their values
// Entities are encoded as utf8 in the result.
// alloc can be nullptr, in which case we'll allocate with malloc()
Str ResolveHtmlEntities(Str str, Arena* alloc) {
    Str res;
    size_t resLen = 0;
    int dstOff = 0;

    int off = 0;
    int chunkStart = 0;
    for (;;) {
        bool found = SkipUntil(str, off, '&');
        if (!found) {
            if (str::IsNull(res)) {
                return str;
            }
            // copy the remaining string
            MemAppend(res.s, dstOff, Str(str.s + chunkStart, str.len - chunkStart));
            break;
        }
        if (str::IsNull(res)) {
            // allocate memory for the result string
            // I'm banking that text after resolving entities will
            // be smaller than the original
            resLen = (size_t)str.len + 8;        // +8 just in case
            res.s = (char*)Alloc(alloc, resLen); // str-port: owned heap
        }
        MemAppend(res.s, dstOff, Str(str.s + chunkStart, off - chunkStart));
        // off points at '&'
        int rune = -1;
        Str entEnd = ResolveHtmlEntity(Str(str.s + off + 1, str.len - off - 1), rune);
        if (str::IsNull(entEnd)) {
            // unknown entity, just copy the '&'
            MemAppend(res.s, dstOff, Str(str.s + off, 1));
            off++;
        } else {
            str::Utf8Encode(res.s, dstOff, rune);
            off = (int)(entEnd.s - str.s);
        }
        chunkStart = off;
    }
    res.s[dstOff] = 0;
    ReportIf(dstOff >= (int)resLen);
    res.len = dstOff;
    return res;
}

// convenience function for the above that always allocates
Str ResolveHtmlEntities(Str s) {
    Str res = ResolveHtmlEntities(s, nullptr);
    if (res.s == s.s) {
        // ensure 0-terminated string is returned
        return str::Dup(s);
    }
    return res;
}

Str ResolveHtmlEntitiesTemp(Str s) {
    Str res = ResolveHtmlEntities(s, GetTempArena());
    if (res.s == s.s) {
        // ensure 0-terminated string is returned
        return str::DupTemp(s);
    }
    return res;
}

bool AttrInfo::NameIs(Str s) const {
    return str::EqNIx(name, name.len, s);
}

// return true if nameToCheck is the same as s after skipping namespace preifix
static bool IsNameWithNS(Str s, Str nameToCheck) {
    Str name = s;
    Str colon = str::FindChar(s, ':');
    if (colon) {
        int prefixLen = (int)(colon.s - s.s) + 1;
        name = Str(s.s + prefixLen, s.len - prefixLen);
    }
    return str::EqNIx(name, name.len, nameToCheck);
}

// for now just ignores any namespace qualifier
// (i.e. succeeds for "xlink:href" with name="href" and any value of attrNS)
// TODO: add proper namespace support
bool AttrInfo::NameIsNS(Str nameToCheck, Str) const {
    // ReportIf(!ns);
    return IsNameWithNS(name, nameToCheck);
}

bool AttrInfo::ValIs(Str s) const {
    return str::EqNIx(val, val.len, s);
}

void HtmlToken::SetTag(TokenType new_type, Str slice) {
    type = new_type;
    s = slice;
    int nameEnd = 0;
    SkipName(s, nameEnd);
    nLen = (size_t)nameEnd;
    tag = FindHtmlTag(Str(s.s, (int)nLen));
    nextAttrOff = -1;
}

void HtmlToken::SetText(Str slice) {
    type = Text;
    s = slice;
}

void HtmlToken::SetError(ParsingError err, Str errContext) {
    type = Error;
    error = err;
    s = errContext;
}

bool HtmlToken::NameIs(Str nameToFind) const {
    return (nameToFind.len == (int)nLen) && str::StartsWithI(Str(s.s, (int)nLen), nameToFind);
}

// for now just ignores any namespace qualifier
// (i.e. succeeds for "opf:content" with name="content" and any value of ns)
// TODO: add proper namespace support
bool HtmlToken::NameIsNS(Str nameToCheck, Str) const {
    // ReportIf(!ns);
    //  nLen is 'nameLen' i.e. first nLen characters of s is a name
    return IsNameWithNS(Str(s.s, (int)nLen), nameToCheck);
}

// reparse point is an address within html that we can
// can feed to HtmlPullParser() to start parsing from that point
Str HtmlToken::GetReparsePoint() const {
    if (IsStartTag() || IsEmptyElementEndTag()) {
        return Str(s.s - 1, s.len + 1);
    }
    if (IsEndTag()) {
        return Str(s.s - 2, s.len + 2);
    }
    if (IsText()) {
        return s;
    }
    ReportIf(true); // don't call us on error tokens
    return {};
}

AttrInfo* HtmlToken::GetAttrByName(Str name) {
    nextAttrOff = -1; // start from the beginning
    for (AttrInfo* a = NextAttr(); a; a = NextAttr()) {
        if (a->NameIs(name)) {
            return a;
        }
    }
    return nullptr;
}

AttrInfo* HtmlToken::GetAttrByNameNS(Str name, Str attrNS) {
    nextAttrOff = -1; // start from the beginning
    for (AttrInfo* a = NextAttr(); a; a = NextAttr()) {
        if (a->NameIsNS(name, attrNS)) {
            return a;
        }
    }
    return nullptr;
}

// We expect:
// whitespace | attribute name | = | attribute value
// where attribute value can be quoted
AttrInfo* HtmlToken::NextAttr() {
    // start after the last attribute found (or the beginning)
    int off = nextAttrOff >= 0 ? nextAttrOff : (int)nLen;

    // parse attribute name
    SkipWs(s, off);
    if (off == s.len) {
    NoNextAttr:
        nextAttrOff = -1;
        return nullptr;
    }
    int nameStart = off;
    SkipName(s, off);
    attrInfo.name = Str(s.s + nameStart, off - nameStart);
    if (!attrInfo.name) {
        goto NoNextAttr;
    }
    SkipWs(s, off);
    if ((off == s.len) || ('=' != s.s[off])) {
        // attributes without values get their names as value in HTML
        attrInfo.val = attrInfo.name;
        nextAttrOff = off;
        return &attrInfo;
    }

    // parse attribute value
    ++off; // skip '='
    SkipWs(s, off);
    if (off == s.len) {
        // attribute with implicit empty value
        attrInfo.val = Str(s.s + off, 0);
    } else if (('\'' == s.s[off]) || ('\"' == s.s[off])) {
        // attribute with quoted value
        char quote = s.s[off];
        ++off;
        int valStart = off;
        if (!SkipUntil(s, off, quote)) {
            goto NoNextAttr;
        }
        attrInfo.val = Str(s.s + valStart, off - valStart);
        ++off;
    } else {
        int valStart = off;
        SkipNonWs(s, off);
        attrInfo.val = Str(s.s + valStart, off - valStart);
    }
    nextAttrOff = off;
    return &attrInfo;
}

// Given a tag like e.g.:
// <tag attr=">" />
// tries to find the closing '>' and not be confused by '>' that
// are part of attribute value. We're not very strict here
// Returns false if didn't find
static bool SkipUntilTagEnd(Str s, int& off) {
    while (off < s.len) {
        char c = s.s[off++];
        if ('>' == c) {
            --off;
            return true;
        }
        if (('\'' == c) || ('"' == c)) {
            if (!SkipUntil(s, off, c)) {
                return false;
            }
            ++off;
        }
    }
    return false;
}

// Returns next part of html or nullptr if finished
HtmlToken* HtmlPullParser::Next() {
    if (currPos >= html.len) {
        return nullptr;
    }

Next:
    int start = currPos;
    if (html.s[currPos] != '<' || currPos + 1 < html.len && !IsValidTagStart(html.s[currPos + 1])) {
        // this must be text between tags
        if (html.s[currPos] == '<') {
            ++currPos;
        }
        if (!SkipUntil(html, currPos, '<') && IsSpaceOnly(Str(html.s + start, currPos - start))) {
            // ignore whitespace after the last tag
            return nullptr;
        }
        currToken.SetText(Str(html.s + start, currPos - start));
        return &currToken;
    }

    // '<' - tag begins
    ++start;

    // skip <? and <! (processing instructions and comments)
    if (start < html.len && (('?' == html.s[start]) || ('!' == html.s[start]))) {
        if ('!' == html.s[start] && start + 2 < html.len &&
            str::StartsWith(Str(html.s + start, html.len - start), "!--")) {
            currPos = start + 3;
            if (!SkipUntil(html, currPos, StrL("-->"))) {
                currToken.SetError(HtmlToken::UnclosedTag, Str(html.s + start, html.len - start));
                return &currToken;
            }
            currPos += 3;
        } else if (!SkipUntil(html, currPos, '>')) {
            currToken.SetError(HtmlToken::UnclosedTag, Str(html.s + start, html.len - start));
            return &currToken;
        } else {
            ++currPos;
        }
        goto Next;
    }

    if (!SkipUntilTagEnd(html, currPos)) {
        currToken.SetError(HtmlToken::UnclosedTag, Str(html.s + start, html.len - start));
        return &currToken;
    }

    ReportIf('>' != html.s[currPos]);
    if (currPos == start || currPos == start + 1 && html.s[start] == '/') {
        // skip empty tags (</>), because we're lenient
        ++currPos;
        goto Next;
    }

    if (('/' == html.s[start]) && ('/' == html.s[currPos - 1])) { // </foo/>
        currToken.SetError(HtmlToken::InvalidTag, Str(html.s + start, currPos - start));
    } else if ('/' == html.s[start]) { // </foo>
        currToken.SetTag(HtmlToken::EndTag, Str(html.s + start + 1, currPos - start - 1));
    } else if ('/' == html.s[currPos - 1]) { // <foo/>
        currToken.SetTag(HtmlToken::EmptyElementTag, Str(html.s + start, currPos - start - 1));
    } else {
        currToken.SetTag(HtmlToken::StartTag, Str(html.s + start, currPos - start));
    }
    ++currPos;
    return &currToken;
}
