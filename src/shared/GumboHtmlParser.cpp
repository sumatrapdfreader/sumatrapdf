/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/HtmlTags.h"

#include "GumboHtmlParser.h"

const GumboVector* GumboChildrenOf(const GumboNode* node) {
    if (!node) {
        return nullptr;
    }
    if (node->type == GUMBO_NODE_ELEMENT) {
        return &node->v.element.children;
    }
    if (node->type == GUMBO_NODE_DOCUMENT) {
        return &node->v.document.children;
    }
    return nullptr;
}

// Push in reverse so a depth-first stack visits children in document order.
void GumboPushChildren(Vec<const GumboNode*>& stack, const GumboNode* node) {
    const GumboVector* children = GumboChildrenOf(node);
    if (!children) {
        return;
    }
    for (unsigned int i = children->length; i > 0; i--) {
        VecAppend(stack, (const GumboNode*)children->data[i - 1]);
    }
}

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

static bool LocalNameIs(Str s, Str name) {
    int colon = str::IndexOfChar(s, ':');
    if (colon >= 0) {
        s = Str(s.s + colon + 1, len(s) - colon - 1);
    }
    return str::EqNIx(s, len(s), name);
}

bool GumboTagNameIs(const GumboNode* node, Str name, HtmlNameMatch match) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    Str tag = GumboElementTagName(node);
    return str::EqI(tag, name) || (match == HtmlNameMatch::Local && LocalNameIs(tag, name));
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

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name, HtmlNameMatch match) {
    // iterative pre-order DFS so a deeply nested document can't overflow the
    // stack (gumbo builds the tree iteratively, but recursing over it doesn't)
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, node);
    while (len(toVisit) > 0) {
        const GumboNode* n = VecPop(toVisit);
        if (!n) {
            continue;
        }
        if (GumboTagNameIs(n, name, match)) {
            return n;
        }
        GumboPushChildren(toVisit, n);
    }
    return nullptr;
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

TempStr GumboTextContentTemp(const GumboNode* node, GumboTextMode mode) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    str::Builder sb;
    Vec<const GumboNode*> toVisit;
    GumboPushChildren(toVisit, node);
    while (len(toVisit) > 0) {
        const GumboNode* child = VecPop(toVisit);
        if (!child) {
            continue;
        }
        if (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE || child->type == GUMBO_NODE_CDATA) {
            sb.Append(Str(child->v.text.text));
        } else if (mode == GumboTextMode::Descendants && child->type == GUMBO_NODE_ELEMENT) {
            GumboPushChildren(toVisit, child);
        }
    }
    return len(sb) > 0 ? ToStrTemp(sb) : TempStr{};
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

constexpr int kMaxHtmlEntityRune = 0x10ffff;

static int ValidHtmlEntityRuneOrFallback(int rune) {
    if (rune <= 0 || rune > kMaxHtmlEntityRune || (rune >= 0xd800 && rune <= 0xdfff)) {
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
    int start = off;
    while (off < str.len) {
        int digit = str::HexDigitVal(str.s[off]);
        if (digit < 0 || digit >= base) {
            break;
        }
        if (codepoint < 0 || codepoint > (kMaxHtmlEntityRune - digit) / base) {
            codepoint = -1;
        } else {
            codepoint = (codepoint * base) + digit;
        }
        off++;
    }
    if (off == start) {
        return {};
    }
    if (off < str.len && str.s[off] == ';') {
        off++;
    }

    rune = ValidHtmlEntityRuneOrFallback(codepoint);
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

    rune = (int)FindHtmlEntityRune(Str(str.s, entLen));
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

// return true if s consists only of whitespace
bool IsSpaceOnly(Str s) {
    str::TrimWs(s);
    return len(s) == 0;
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

// Borrow unchanged text; decoded output belongs to the requested allocator.
Str ResolveHtmlEntities(Str text, Arena* arena) {
    Str chunk, rest;
    if (!str::CutChar(text, '&', &chunk, &rest)) {
        return text;
    }

    str::Builder out(arena);
    out.Reserve(len(text));
    for (;;) {
        out.Append(chunk);
        int rune = -1;
        Str end = ResolveHtmlEntity(rest, rune);
        if (str::IsNull(end)) {
            out.AppendChar('&');
        } else {
            constexpr int kMaxUtf8RuneBytes = 4;
            char encoded[kMaxUtf8RuneBytes];
            int n = 0;
            str::Utf8Encode(encoded, n, rune);
            out.Append(Str(encoded, n));
            rest = end;
        }
        if (!str::CutChar(rest, '&', &chunk, &rest)) {
            out.Append(chunk);
            break;
        }
    }
    return arena ? ToStr(out) : out.TakeStr();
}

// Copy unchanged input too, so both wrappers return owned, NUL-terminated text.
static Str CopyHtmlEntities(Str s, Arena* arena) {
    Str res = ResolveHtmlEntities(s, arena);
    return res.s == s.s ? str::Dup(arena, s) : res;
}

Str ResolveHtmlEntities(Str s) {
    return CopyHtmlEntities(s, nullptr);
}

Str ResolveHtmlEntitiesTemp(Str s) {
    return CopyHtmlEntities(s, GetTempArena());
}

bool AttrInfo::NameIs(Str s, HtmlNameMatch match) const {
    return match == HtmlNameMatch::Local ? LocalNameIs(name, s) : str::EqNIx(name, len(name), s);
}

bool AttrInfo::ValIs(Str s) const {
    return str::EqNIx(val, val.len, s);
}

void HtmlToken::SetTag(TokenType newType, Str tagName) {
    type = newType;
    s = tagName;
    int off = 0;
    while (off < len(tagName)) {
        char c = tagName.s[off];
        if (!str::IsAlNum(c) && c != '.' && c != '-' && c != '_' && c != ':') {
            break;
        }
        off++;
    }
    name = Str(tagName.s, off);
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

bool HtmlToken::NameIs(Str nameToFind, HtmlNameMatch match) const {
    return match == HtmlNameMatch::Local ? LocalNameIs(name, nameToFind) : str::EqI(name, nameToFind);
}

// Return views by value so another lookup cannot overwrite earlier attributes.
AttrInfo HtmlToken::GetAttrByName(Str attrName, HtmlNameMatch match) {
    if (!node || (node->type != GUMBO_NODE_ELEMENT && node->type != GUMBO_NODE_TEMPLATE)) {
        return {};
    }
    const GumboVector* attrs = &node->v.element.attributes;
    for (unsigned int i = 0; i < attrs->length; i++) {
        const GumboAttribute* attr = (const GumboAttribute*)attrs->data[i];
        AttrInfo info{Str(attr->name), Str(attr->value)};
        if (info.NameIs(attrName, match)) {
            return info;
        }
    }
    return {};
}

static Str StrFromPiece(GumboStringPiece piece) {
    if (!piece.data) {
        return {};
    }
    return Str((char*)piece.data, (int)piece.length);
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

static Str TagInner(Str raw, HtmlToken::TokenType type) {
    Str prefix = type == HtmlToken::EndTag ? StrL("</") : StrL("<");
    if (len(raw) <= len(prefix) || !str::TrimPrefix(raw, prefix)) {
        return {};
    }
    str::TrimSuffix(raw, StrL(">"));
    if (type == HtmlToken::EmptyElementTag) {
        int slash = len(raw) - 1;
        while (slash >= 0 && str::IsWs(raw.s[slash])) {
            slash--;
        }
        if (slash >= 0 && raw.s[slash] == '/') {
            raw.len = slash;
        }
    }
    return raw;
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

GumboDoc::GumboDoc(Str data, GumboMode mode) {
    opts = GumboMakeOptions();
    if (mode == GumboMode::XmlFragment) {
        // Foreign content honors XML self-closing tags, keeping siblings separate.
        opts.fragment_context = GUMBO_TAG_SVG;
        opts.fragment_namespace = GUMBO_NAMESPACE_SVG;
    }
    output = gumbo_parse_with_options(&opts, data.s, (size_t)len(data));
}

GumboDoc::~GumboDoc() {
    if (output) {
        gumbo_destroy_output_iter(&opts, output);
    }
}

GumboHtmlParser::GumboHtmlParser(Str s) : html(s), doc(s, GumboMode::XmlFragment) {
    SetCurrPosOff(0);
}

HtmlToken* GumboHtmlParser::ReadToken() {
    while (len(toVisit) > 0) {
        Frame frame = VecPop(toVisit);
        const GumboNode* node = frame.node;
        if (!node) {
            continue;
        }

        if (frame.emitEnd) {
            Str raw = StrFromPiece(node->v.element.original_end_tag);
            if (len(raw) == 0) {
                continue;
            }
            currToken.SetTag(HtmlToken::EndTag, TagInner(raw, HtmlToken::EndTag));
            currToken.reparsePoint = raw;
            currToken.node = node;
            return &currToken;
        }

        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
            Str text = StrFromPiece(node->v.text.original_text);
            if (node->type == GUMBO_NODE_CDATA) {
                text = CDataText(text, node);
            } else if (len(text) == 0) {
                text = Str(node->v.text.text);
            }
            currToken.SetText(text);
            currToken.node = node;
            return &currToken;
        }

        const GumboVector* children =
            node->type == GUMBO_NODE_TEMPLATE ? &node->v.element.children : GumboChildrenOf(node);
        if (!children) {
            continue;
        }

        Str raw;
        if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
            raw = StrFromPiece(node->v.element.original_tag);
        }
        bool selfClosing = IsSelfClosingStartTag(raw);
        if (!selfClosing) {
            if (len(raw) > 0 && len(StrFromPiece(node->v.element.original_end_tag)) > 0) {
                VecAppend(toVisit, {node, true});
            }
            for (unsigned int i = children->length; i > 0; i--) {
                VecAppend(toVisit, {(const GumboNode*)children->data[i - 1], false});
            }
        }
        if (len(raw) == 0) {
            continue;
        }
        auto type = selfClosing ? HtmlToken::EmptyElementTag : HtmlToken::StartTag;
        currToken.SetTag(type, TagInner(raw, type));
        currToken.reparsePoint = raw;
        currToken.node = node;
        return &currToken;
    }
    return nullptr;
}

void GumboHtmlParser::SetCurrPosOff(ptrdiff_t off) {
    seekOff = std::min<ptrdiff_t>(std::max<ptrdiff_t>(off, 0), len(html));
    VecClear(toVisit);
    if (doc.Document()) {
        VecAppend(toVisit, {doc.Document(), false});
    }
}

int GumboHtmlParser::PosOf(Str p) const {
    return (int)PosOfSource(html, p);
}

HtmlToken* GumboHtmlParser::Next() {
    while (HtmlToken* token = ReadToken()) {
        if (seekOff < 0) {
            return token;
        }
        ptrdiff_t off = PosOfSource(html, token->reparsePoint);
        if (token->IsText() && seekOff >= off && seekOff < off + len(token->s)) {
            int delta = (int)(seekOff - off);
            token->s = Str(token->s.s + delta, len(token->s) - delta);
            token->reparsePoint = token->s;
        } else if (off < seekOff) {
            continue;
        }
        seekOff = -1;
        return token;
    }
    return nullptr;
}

#if IS_DEBUG
bool GumboHtmlParser_UnitTest() {
    {
        GumboHtmlParser parser(StrL("<div>abc<br/>b<span>c</span>d</div>"));
        str::Builder stream;
        while (HtmlToken* t = parser.Next()) {
            Str kind = t->IsText()                 ? StrL("text")
                       : t->IsEndTag()             ? StrL("end")
                       : t->IsEmptyElementEndTag() ? StrL("empty")
                                                   : StrL("start");
            stream.Append(fmt("%s:%s;", kind, t->s));
        }
        if (!str::Eq(ToStr(stream),
                     StrL("start:div;text:abc;empty:br;text:b;start:span;text:c;end:span;text:d;end:div;"))) {
            return false;
        }
        parser.SetCurrPosOff(6);
        HtmlToken* resumed = parser.Next();
        if (!resumed || !resumed->IsText() || !str::Eq(resumed->s, StrL("bc"))) {
            return false;
        }
    }

    const Str entities[][2] = {
        {{}, {}},
        {StrL("plain text"), StrL("plain text")},
        {StrL("&"), StrL("&")},
        {StrL("&&amp;&amp;"), StrL("&&&")},
        {StrL("a &lt;b&gt; &unknown; &#x1F600;"), StrL("a <b> &unknown; 😀")},
        {StrL("&#0; / &#x110000;"), StrL("? / ?")},
    };
    for (const auto& c : entities) {
        if (!str::Eq(ResolveHtmlEntitiesTemp(c[0]), c[1])) {
            return false;
        }
    }

    HtmlToken token;
    token.SetTag(HtmlToken::StartTag, StrL("opf:metadata"));
    Str name = StrL("metadata!");
    name.len--;
    if (token.NameIs(name) || !token.NameIs(name, HtmlNameMatch::Local) || !token.NameIs(StrL("OPF:METADATA")) ||
        token.NameIs(StrL("opf:metadata"), HtmlNameMatch::Local)) {
        return false;
    }
    Str xml = StrL("<opf:metadata q:href='book' id='meta'>text</opf:metadata>");
    GumboDoc doc(xml, GumboMode::XmlFragment);
    const GumboNode* node = GumboFindDescendantByTag(doc.Document(), name, HtmlNameMatch::Local);
    bool ok = node && !GumboFindDescendantByTag(doc.Document(), name) &&
              GumboFindDescendantByTag(doc.Document(), StrL("opf:metadata")) == node &&
              GumboTagNameIs(node, StrL("opf:metadata"), HtmlNameMatch::Local);
    token.node = node;
    ok = ok && !token.GetAttrByName(StrL("href"));
    AttrInfo attr = token.GetAttrByName(StrL("href"), HtmlNameMatch::Local);
    ok = ok && attr && str::Eq(attr.val, StrL("book")) && !attr.NameIs(StrL("q:href"), HtmlNameMatch::Local) &&
         token.GetAttrByName(StrL("Q:HREF"));
    AttrInfo href = token.GetAttrByName(StrL("href"), HtmlNameMatch::Local);
    AttrInfo id = token.GetAttrByName(StrL("id"));
    ok = ok && href && id && str::Eq(href.val, StrL("book")) && str::Eq(id.val, StrL("meta"));
    return ok;
}
#endif
