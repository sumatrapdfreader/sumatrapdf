/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include "../../ext/a-gumbo/gumbo.h"
}

enum class HtmlNameMatch {
    Exact,
    Local
};

const GumboVector* GumboChildrenOf(const GumboNode* node);
void GumboPushChildren(Vec<const GumboNode*>& stack, const GumboNode* node);

bool GumboTagNameIs(const GumboNode* node, Str name, HtmlNameMatch match = HtmlNameMatch::Exact);

const GumboNode* GumboFindChildByTag(const GumboNode* node, Str name);

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name, HtmlNameMatch match = HtmlNameMatch::Exact);

TempStr GumboAttributeValueTemp(const GumboNode* node, const char* name);

enum class GumboTextMode {
    Direct,
    Descendants
};

TempStr GumboTextContentTemp(const GumboNode* node, GumboTextMode mode = GumboTextMode::Direct);

// Returns a GumboOptions struct configured with our malloc/free wrappers
// and otherwise-default values. We avoid the kGumboDefaultOptions data
// extern because it's awkward to import across the libsumatrapdf.dll boundary.
GumboOptions GumboMakeOptions();

enum class GumboMode {
    Html,
    XmlFragment
};

class GumboDoc {
    GumboOptions opts;
    GumboOutput* output = nullptr;

  public:
    GumboDoc(Str data, GumboMode mode);
    ~GumboDoc();
    GumboDoc(const GumboDoc&) = delete;
    GumboDoc& operator=(const GumboDoc&) = delete;
    const GumboNode* Document() const { return output ? output->document : nullptr; }
};

struct AttrInfo {
    Str name;
    Str val;

    explicit operator bool() const { return name.s != nullptr; }
    bool NameIs(Str s, HtmlNameMatch match = HtmlNameMatch::Exact) const;
    bool ValIs(Str s) const;
};

struct HtmlToken {
    enum TokenType {
        StartTag,
        EndTag,
        EmptyElementTag,
        Text
    };

    bool IsStartTag() const { return type == StartTag; }
    bool IsEndTag() const { return type == EndTag; }
    bool IsEmptyElementEndTag() const { return type == EmptyElementTag; }
    bool IsTag() const { return IsStartTag() || IsEndTag() || IsEmptyElementEndTag(); }
    bool IsText() const { return type == Text; }

    void SetTag(TokenType newType, Str name);
    void SetText(Str slice);

    TokenType type = Text;
    Str s;
    Str name;
    Str reparsePoint;
    HtmlTag tag = Tag_NotFound;
    const GumboNode* node = nullptr;

    bool NameIs(Str nameToFind, HtmlNameMatch match = HtmlNameMatch::Exact) const;
    AttrInfo GetAttrByName(Str name, HtmlNameMatch match = HtmlNameMatch::Exact);
};

class GumboHtmlParser {
    struct Frame {
        const GumboNode* node;
        bool emitEnd;
    };

    Str html;
    GumboDoc doc;
    Vec<Frame> toVisit;
    ptrdiff_t seekOff = -1;

    HtmlToken currToken{};

    HtmlToken* ReadToken();

  public:
    explicit GumboHtmlParser(Str s);

    void SetCurrPosOff(ptrdiff_t off);
    size_t Len() const { return (size_t)html.len; }
    int PosOf(Str p) const;

    HtmlToken* Next();
};

Str ResolveHtmlEntity(Str str, int& rune);
Str ResolveHtmlEntities(Str s, Arena* a);
Str ResolveHtmlEntities(Str s);
Str ResolveHtmlEntitiesTemp(Str s);
