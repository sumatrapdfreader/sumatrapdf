/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/HtmlTags.h"

#include "GumboHtmlParser.h"

static Str GumboElementTagName(const GumboNode* node) {
    ReportIf(!node || node->type != GUMBO_NODE_ELEMENT);
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    if (node->v.element.tag != GUMBO_TAG_UNKNOWN) {
        return Str(gumbo_normalized_tagname(node->v.element.tag));
    }
    Str orig = Str((char*)node->v.element.original_tag.data, (int)node->v.element.original_tag.length);
    int off = 0;
    if (len(orig) > 0 && orig.s[0] == '<') {
        off = 1;
    }
    int end = off;
    while (end < orig.len && orig.s[end] != '>' && orig.s[end] != '/' && orig.s[end] != ' ' && orig.s[end] != '\t' &&
           orig.s[end] != '\n' && orig.s[end] != '\r') {
        end++;
    }
    return Str(orig.s + off, end - off);
}

// True if `node` is an element whose tag name matches `name`
// (case-insensitive). Handles both standard HTML tags (via
// gumbo_normalized_tagname) and unknown tags (case-preserved in
// original_tag) -- the latter covers e.g. PascalCase XML element names
// like <ComicInfo>'s <Title>, <Year>, ...
bool GumboTagNameIs(const GumboNode* node, Str name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    return str::EqI(GumboElementTagName(node), name);
}

bool GumboTagNameIsNS(const GumboNode* node, Str name, Str /*ns*/) {
    // Preserve the old parser's compatibility: namespace URI is ignored,
    // and a prefix in the source tag name is treated as optional.
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    Str tag = GumboElementTagName(node);
    if (str::EqI(tag, name)) {
        return true;
    }
    Str after;
    if (!str::CutChar(tag, ':', nullptr, &after)) {
        return false;
    }
    return str::EqI(after, name);
}

// First direct element child of `node` whose tag matches `name`.
// Returns nullptr if `node` isn't an element or no matching child exists.
const GumboNode* GumboFindChildByTag(const GumboNode* node, Str name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (GumboTagNameIs(child, name)) {
            return child;
        }
    }
    return nullptr;
}

static const GumboNode* GumboFindDescendantByTagImpl(const GumboNode* node, Str name, Str ns, bool matchNS) {
    // iterative pre-order DFS so a deeply nested document can't overflow the
    // stack (gumbo builds the tree iteratively, but recursing over it doesn't)
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, node);
    while (len(toVisit) > 0) {
        const GumboNode* n = VecPop(toVisit);
        if (!n) {
            continue;
        }
        const GumboVector* children = nullptr;
        if (n->type == GUMBO_NODE_ELEMENT) {
            bool matches = matchNS ? GumboTagNameIsNS(n, name, ns) : GumboTagNameIs(n, name);
            if (matches) {
                return n;
            }
            children = &n->v.element.children;
        } else if (n->type == GUMBO_NODE_DOCUMENT) {
            children = &n->v.document.children;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                VecAppend(toVisit, (const GumboNode*)children->data[i - 1]);
            }
        }
    }
    return nullptr;
}

// Depth-first search for the first element under `node` with the given
// tag name. Walks both ELEMENT and DOCUMENT nodes.
const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name) {
    return GumboFindDescendantByTagImpl(node, name, {}, false);
}

const GumboNode* GumboFindDescendantByTagNS(const GumboNode* node, Str name, Str ns) {
    return GumboFindDescendantByTagImpl(node, name, ns, true);
}

TempStr GumboAttributeValueTemp(const GumboNode* node, const char* name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    const GumboAttribute* attr = gumbo_get_attribute(&node->v.element.attributes, name);
    if (!attr) {
        return {};
    }
    return str::DupTemp(Str(attr->value));
}

// Concatenated text content (TEXT/WHITESPACE/CDATA children) of an
// element. Returns nullptr for non-element nodes or empty content.
TempStr GumboTextContentTemp(const GumboNode* node) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    str::Builder sb;
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE || child->type == GUMBO_NODE_CDATA) {
            sb.Append(Str(child->v.text.text));
        }
    }
    if (len(sb) == 0) {
        return {};
    }
    return ToStrTemp(sb);
}

static void* GumboMallocWrapper(void* /*userdata*/, size_t size) {
    return malloc(size);
}
static void GumboFreeWrapper(void* /*userdata*/, void* ptr) {
    free(ptr);
}

GumboOptions GumboMakeOptions() {
    GumboOptions opts{};
    opts.allocator = GumboMallocWrapper;
    opts.deallocator = GumboFreeWrapper;
    opts.userdata = nullptr;
    opts.tab_stop = 8;
    opts.stop_on_first_error = false;
    opts.max_errors = -1;
    opts.fragment_context = GUMBO_TAG_LAST;
    opts.fragment_namespace = GUMBO_NAMESPACE_HTML;
    return opts;
}

GumboOptions GumboMakeXmlFragmentOptions() {
    GumboOptions opts = GumboMakeOptions();
    // Gumbo only honors XML-style self-closing syntax for foreign content.
    // Parsing XML-ish metadata as an SVG-namespace fragment keeps <item />
    // and similar EPUB/ComicInfo nodes from swallowing their following siblings.
    opts.fragment_context = GUMBO_TAG_SVG;
    opts.fragment_namespace = GUMBO_NAMESPACE_SVG;
    return opts;
}

// returns -1 if didn't find
int HtmlEntityNameToRune(Str name) {
    return (int)FindHtmlEntityRune(name);
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
        char c = str.s[off];
        int digit = -1;
        if (base == 16) {
            digit = HtmlEntityHexDigit(c);
        } else if (c >= '0' && c <= '9') {
            digit = (int)(c - '0');
        }
        if (digit < 0 || digit >= base) {
            break;
        }
        any = true;
        if (codepoint > (0x10ffff - digit) / base) {
            overflow = true;
        } else if (!overflow) {
            codepoint = (codepoint * base) + digit;
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
    Str rest = Str(s.s + off, s.len - off);
    int n = str::TrimWs(rest);
    off += n;
    return n > 0;
}

// return true if skipped
bool SkipNonWs(Str s, int& off) {
    Str rest = Str(s.s + off, s.len - off);
    int n = str::TrimNonWs(rest);
    off += n;
    return n > 0;
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
    if (!buf || len(src) == 0) {
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
// with a in which entities are converted to their values
// Entities are encoded as utf8 in the result.
// a can be nullptr, in which case we'll allocate with malloc()
Str ResolveHtmlEntities(Str str, Arena* a) {
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
            res.s = (char*)Alloc(a, resLen);
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
bool AttrInfo::NameIsNS(Str nameToCheck, Str /*ns*/) const {
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
bool HtmlToken::NameIsNS(Str nameToCheck, Str /*ns*/) const {
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
    end = std::max(end, start);
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
    if (str::TrimPrefix(raw, StrL("<![CDATA[")) && str::EndsWith(raw, StrL("]]>"))) {
        raw.len -= 3;
        return raw;
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
    VecAppend(toVisit, {output->document, false});
    while (len(toVisit) > 0) {
        Frame frame = VecPop(toVisit);
        const GumboNode* node = frame.node;
        if (!node) {
            continue;
        }

        if (frame.emitEnd) {
            Str rawEnd = StrFromPiece(node->v.element.original_end_tag);
            if (len(rawEnd) > 0) {
                Str inner = EndTagInner(rawEnd);
                VecAppend(events, {HtmlToken::EndTag, node, inner, TagNameFromTagInner(inner), rawEnd,
                                   PosOfSource(html, rawEnd)});
            }
            continue;
        }

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            Str text = StrFromPiece(node->v.text.original_text);
            if (len(text) == 0) {
                text = Str(node->v.text.text);
            }
            VecAppend(events, {HtmlToken::Text, node, text, {}, text, PosOfSource(html, text)});
            continue;
        }

        if (node->type == GUMBO_NODE_CDATA) {
            Str raw = StrFromPiece(node->v.text.original_text);
            Str text = CDataText(raw, node);
            VecAppend(events, {HtmlToken::Text, node, text, {}, text, PosOfSource(html, text)});
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
                    VecAppend(toVisit, {(const GumboNode*)children->data[i - 1], false});
                }
                continue;
            }

            bool selfClosing = IsSelfClosingStartTag(rawStart);
            Str inner = StartTagInner(rawStart, selfClosing);
            HtmlToken::TokenType type = selfClosing ? HtmlToken::EmptyElementTag : HtmlToken::StartTag;
            VecAppend(events, {type, node, inner, TagNameFromTagInner(inner), rawStart, PosOfSource(html, rawStart)});

            if (!selfClosing) {
                if (len(StrFromPiece(node->v.element.original_end_tag)) > 0) {
                    VecAppend(toVisit, {node, true});
                }
                for (unsigned int i = children->length; i > 0; i--) {
                    VecAppend(toVisit, {(const GumboNode*)children->data[i - 1], false});
                }
            }
            continue;
        }

        for (unsigned int i = children->length; i > 0; i--) {
            VecAppend(toVisit, {(const GumboNode*)children->data[i - 1], false});
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
    off = std::max<ptrdiff_t>(off, 0);
    off = std::min<ptrdiff_t>(off, html.len);

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
