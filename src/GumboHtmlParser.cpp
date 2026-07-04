/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/HtmlTags.h"

#include "GumboHelpers.h"
#include "GumboHtmlParser.h"

// returns -1 if didn't find
int HtmlEntityNameToRune(Str name) {
    return FindHtmlEntityRune(name);
}

static int HtmlEntityHexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int)(c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (int)(c - 'A') + 10;
    }
    return -1;
}

static int ValidHtmlEntityRuneOrFallback(int rune) {
    if (rune <= 0 || rune > 0x10ffff || (rune >= 0xd800 && rune <= 0xdfff)) {
        return '?';
    }
    return rune;
}

// if str starts with a numeric entity after the leading '&', sets rune and returns a slice after the entity
static Str ParseHtmlNumericEntity(Str str, int& rune) {
    if (str.len < 2 || str.s[0] != '#') {
        return {};
    }

    int base = 10;
    int off = 1;
    if (off < str.len && (str.s[off] == 'x' || str.s[off] == 'X')) {
        base = 16;
        off++;
    }

    int codepoint = 0;
    bool any = false;
    bool overflow = false;
    while (off < str.len) {
        int digit = base == 16                               ? HtmlEntityHexDigit(str.s[off])
                    : str.s[off] >= '0' && str.s[off] <= '9' ? (int)(str.s[off] - '0')
                                                             : -1;
        if (digit < 0 || digit >= base) {
            break;
        }
        any = true;
        if (codepoint > (0x10ffff - digit) / base) {
            overflow = true;
        } else if (!overflow) {
            codepoint = codepoint * base + digit;
        }
        off++;
    }
    if (!any) {
        return {};
    }
    if (off < str.len && str.s[off] == ';') {
        off++;
    }

    rune = ValidHtmlEntityRuneOrFallback(overflow ? -1 : codepoint);
    return Str(str.s + off, str.len - off);
}

static Str ResolveHtmlNamedEntity(Str str, int& rune) {
    int entLen = 0;
    while (entLen < str.len && isalnum((u8)str.s[entLen])) {
        entLen++;
    }
    if (entLen == 0) {
        return {};
    }

    rune = HtmlEntityNameToRune(Str(str.s, entLen));
    if (-1 == rune) {
        return {};
    }
    rune = ValidHtmlEntityRuneOrFallback(rune);

    int endOff = entLen;
    if (endOff < str.len && str.s[endOff] == ';') {
        endOff++;
    }
    return Str(str.s + endOff, str.len - endOff);
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

static void MemAppend(char* buf, int& off, Str src) {
    if (!src) {
        return;
    }
    memcpy(buf + off, src.s, src.len);
    off += src.len;
}

// if "&foo;" was the entity, str points at the char after '&'
// returns a slice starting after the entity, or empty on failure
Str ResolveHtmlEntity(Str str, int& rune) {
    Str entEnd = ParseHtmlNumericEntity(str, rune);
    if (!str::IsNull(entEnd)) {
        return entEnd;
    }

    entEnd = ResolveHtmlNamedEntity(str, rune);
    if (!str::IsNull(entEnd)) {
        return entEnd;
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
            resLen = (size_t)str.len + 8; // +8 just in case
            res.s = (char*)Alloc(alloc, resLen);
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
    int colonIdx = str::IndexOfChar(s, ':');
    if (colonIdx >= 0) {
        int prefixLen = colonIdx + 1;
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

static Str TagNameFromTagInner(Str s) {
    int off = 0;
    SkipName(s, off);
    return Str(s.s, off);
}

void HtmlToken::SetTag(TokenType newType, Str tagName) {
    type = newType;
    s = tagName;
    name = TagNameFromTagInner(tagName);
    reparsePoint = {};
    tag = FindHtmlTag(name);
    node = nullptr;
}

void HtmlToken::SetText(Str slice) {
    type = Text;
    s = slice;
    name = {};
    reparsePoint = slice;
    tag = Tag_NotFound;
    node = nullptr;
}

bool HtmlToken::NameIs(Str nameToFind) const {
    return str::EqI(name, nameToFind);
}

// for now just ignores any namespace qualifier
// (i.e. succeeds for "opf:content" with name="content" and any value of ns)
// TODO: add proper namespace support
bool HtmlToken::NameIsNS(Str nameToCheck, Str) const {
    // ReportIf(!ns);
    return IsNameWithNS(name, nameToCheck);
}

Str HtmlToken::GetReparsePoint() const {
    if (IsError()) {
        ReportIf(true); // don't call us on error tokens
        return {};
    }
    return reparsePoint;
}

AttrInfo* HtmlToken::GetAttrByName(Str attrName) {
    if (!node || (node->type != GUMBO_NODE_ELEMENT && node->type != GUMBO_NODE_TEMPLATE)) {
        return nullptr;
    }
    const GumboVector* attrs = &node->v.element.attributes;
    for (unsigned int i = 0; i < attrs->length; i++) {
        const GumboAttribute* attr = (const GumboAttribute*)attrs->data[i];
        attrInfo.name = Str(attr->name);
        attrInfo.val = Str(attr->value);
        if (attrInfo.NameIs(attrName)) {
            return &attrInfo;
        }
    }
    return nullptr;
}

AttrInfo* HtmlToken::GetAttrByNameNS(Str attrName, Str attrNS) {
    if (!node || (node->type != GUMBO_NODE_ELEMENT && node->type != GUMBO_NODE_TEMPLATE)) {
        return nullptr;
    }
    const GumboVector* attrs = &node->v.element.attributes;
    for (unsigned int i = 0; i < attrs->length; i++) {
        const GumboAttribute* attr = (const GumboAttribute*)attrs->data[i];
        attrInfo.name = Str(attr->name);
        attrInfo.val = Str(attr->value);
        if (attrInfo.NameIsNS(attrName, attrNS)) {
            return &attrInfo;
        }
    }
    return nullptr;
}

static Str StrFromPiece(GumboStringPiece piece) {
    if (!piece.data) {
        return {};
    }
    return Str((char*)piece.data, (int)piece.length);
}

static const GumboVector* ChildrenOf(const GumboNode* node) {
    if (!node) {
        return nullptr;
    }
    if (node->type == GUMBO_NODE_DOCUMENT) {
        return &node->v.document.children;
    }
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        return &node->v.element.children;
    }
    return nullptr;
}

static bool IsSelfClosingStartTag(Str raw) {
    if (raw.len < 3 || raw.s[0] != '<') {
        return false;
    }
    int off = raw.len - 1;
    if (raw.s[off] == '>') {
        off--;
    }
    while (off > 0 && str::IsWs(raw.s[off])) {
        off--;
    }
    return raw.s[off] == '/';
}

static Str StartTagInner(Str raw, bool selfClosing) {
    if (raw.len < 2 || raw.s[0] != '<') {
        return {};
    }
    int start = 1;
    int end = raw.len;
    if (end > start && raw.s[end - 1] == '>') {
        end--;
    }
    if (selfClosing) {
        int slash = end - 1;
        while (slash >= start && str::IsWs(raw.s[slash])) {
            slash--;
        }
        if (slash >= start && raw.s[slash] == '/') {
            end = slash;
        }
    }
    if (end < start) {
        end = start;
    }
    return Str(raw.s + start, end - start);
}

static Str EndTagInner(Str raw) {
    if (raw.len < 3 || raw.s[0] != '<' || raw.s[1] != '/') {
        return {};
    }
    int start = 2;
    int end = raw.len;
    if (end > start && raw.s[end - 1] == '>') {
        end--;
    }
    return Str(raw.s + start, end - start);
}

static Str CDataText(Str raw, const GumboNode* node) {
    if (str::StartsWith(raw, StrL("<![CDATA[")) && str::EndsWith(raw, StrL("]]>"))) {
        return Str(raw.s + 9, raw.len - 12);
    }
    return Str(node->v.text.text);
}

static ptrdiff_t PosOfSource(Str html, Str p) {
    if (!p.s || p.s < html.s || p.s > html.s + html.len) {
        return 0;
    }
    return p.s - html.s;
}

GumboHtmlParser::GumboHtmlParser(Str s) : html(s) {
    opts = GumboMakeXmlFragmentOptions();
    output = gumbo_parse_with_options(&opts, html.s, (size_t)html.len);
    BuildEvents();
}

GumboHtmlParser::~GumboHtmlParser() {
    if (output) {
        gumbo_destroy_output_iter(&opts, output);
    }
}

void GumboHtmlParser::BuildEvents() {
    if (!output || !output->document) {
        return;
    }

    struct Frame {
        const GumboNode* node;
        bool emitEnd;
    };

    Vec<Frame> toVisit;
    toVisit.Append({output->document, false});
    while (len(toVisit) > 0) {
        Frame frame = toVisit.Pop();
        const GumboNode* node = frame.node;
        if (!node) {
            continue;
        }

        if (frame.emitEnd) {
            Str rawEnd = StrFromPiece(node->v.element.original_end_tag);
            if (len(rawEnd) > 0) {
                Str inner = EndTagInner(rawEnd);
                events.Append(
                    {HtmlToken::EndTag, node, inner, TagNameFromTagInner(inner), rawEnd, PosOfSource(html, rawEnd)});
            }
            continue;
        }

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            Str text = StrFromPiece(node->v.text.original_text);
            if (!text) {
                text = Str(node->v.text.text);
            }
            events.Append({HtmlToken::Text, node, text, {}, text, PosOfSource(html, text)});
            continue;
        }

        if (node->type == GUMBO_NODE_CDATA) {
            Str raw = StrFromPiece(node->v.text.original_text);
            Str text = CDataText(raw, node);
            events.Append({HtmlToken::Text, node, text, {}, text, PosOfSource(html, text)});
            continue;
        }

        const GumboVector* children = ChildrenOf(node);
        if (!children) {
            continue;
        }

        if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
            Str rawStart = StrFromPiece(node->v.element.original_tag);
            if (len(rawStart) == 0) {
                for (unsigned int i = children->length; i > 0; i--) {
                    toVisit.Append({(const GumboNode*)children->data[i - 1], false});
                }
                continue;
            }

            bool selfClosing = IsSelfClosingStartTag(rawStart);
            Str inner = StartTagInner(rawStart, selfClosing);
            HtmlToken::TokenType type = selfClosing ? HtmlToken::EmptyElementTag : HtmlToken::StartTag;
            events.Append({type, node, inner, TagNameFromTagInner(inner), rawStart, PosOfSource(html, rawStart)});

            if (!selfClosing) {
                if (len(StrFromPiece(node->v.element.original_end_tag)) > 0) {
                    toVisit.Append({node, true});
                }
                for (unsigned int i = children->length; i > 0; i--) {
                    toVisit.Append({(const GumboNode*)children->data[i - 1], false});
                }
            }
            continue;
        }

        for (unsigned int i = children->length; i > 0; i--) {
            toVisit.Append({(const GumboNode*)children->data[i - 1], false});
        }
    }
}

HtmlToken* GumboHtmlParser::TokenFromEvent(Event& ev) {
    currToken.type = ev.type;
    currToken.s = ev.s;
    currToken.name = ev.name;
    currToken.reparsePoint = ev.reparsePoint;
    currToken.tag = FindHtmlTag(ev.name);
    currToken.node = ev.node;

    if (ev.type == HtmlToken::Text && textStartOff >= 0) {
        ptrdiff_t delta = textStartOff - ev.off;
        if (delta > 0 && delta < currToken.s.len) {
            currToken.s = Str(currToken.s.s + delta, currToken.s.len - (int)delta);
            currToken.reparsePoint = currToken.s;
        }
        textStartOff = -1;
    }
    return &currToken;
}

void GumboHtmlParser::SetCurrPosOff(ptrdiff_t off) {
    if (off < 0) {
        off = 0;
    }
    if (off > html.len) {
        off = html.len;
    }

    textStartOff = -1;
    eventIdx = (size_t)len(events);
    for (int i = 0; i < len(events); i++) {
        Event& ev = events[i];
        if (ev.type == HtmlToken::Text && off >= ev.off && off < ev.off + ev.s.len) {
            eventIdx = (size_t)i;
            textStartOff = off;
            return;
        }
        if (ev.off >= off) {
            eventIdx = (size_t)i;
            return;
        }
    }
}

int GumboHtmlParser::PosOf(Str p) const {
    return (int)PosOfSource(html, p);
}

HtmlToken* GumboHtmlParser::Next() {
    if (eventIdx >= (size_t)len(events)) {
        return nullptr;
    }
    Event& ev = events[(int)eventIdx++];
    return TokenFromEvent(ev);
}
