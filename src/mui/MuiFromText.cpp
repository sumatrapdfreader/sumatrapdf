/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/StrSlice.h"
#include "utils/HtmlParserLookup.h"
#include "Mui.h"
#include "utils/SerializeTxt.h"
#include "MuiDefs.h"
#include "SvgPath.h"
#include "utils/TxtParser.h"

/*
Code to create mui controls from text description (so that it can be loaded
at runtime).
*/

namespace mui {

struct ControlCreatorNode {
    ControlCreatorNode* next;
    const char* typeName;
    ControlCreatorFunc creator;
};

struct LayoutCreatorNode {
    LayoutCreatorNode* next;
    const char* typeName;
    LayoutCreatorFunc creator;
};

// This is an extensiblity point that allows creating custom controls and layouts
// unknown to mui that appear in text description
static ControlCreatorNode* gControlCreators = nullptr;
static LayoutCreatorNode* gLayoutCreators = nullptr;

void RegisterControlCreatorFor(const char* typeName, ControlCreatorFunc creator) {
    ControlCreatorNode* cc = AllocStruct<ControlCreatorNode>();
    cc->typeName = str::Dup(typeName);
    cc->creator = creator;
    ListInsert(&gControlCreators, cc);
}

static ControlCreatorFunc FindControlCreatorFuncFor(const char* typeName) {
    ControlCreatorNode* curr = gControlCreators;
    while (curr) {
        if (str::EqI(typeName, curr->typeName))
            return curr->creator;
        curr = curr->next;
    }
    return nullptr;
}

void FreeControlCreators() {
    ControlCreatorNode* curr = gControlCreators;
    ControlCreatorNode* next;
    while (curr) {
        next = curr->next;
        str::Free(curr->typeName);
        free(curr);
        curr = next;
    }
}

void RegisterLayoutCreatorFor(const char* layoutName, LayoutCreatorFunc creator) {
    LayoutCreatorNode* lc = AllocStruct<LayoutCreatorNode>();
    lc->typeName = str::Dup(layoutName);
    lc->creator = creator;
    ListInsert(&gLayoutCreators, lc);
}

static LayoutCreatorFunc FindLayoutCreatorFuncFor(const char* typeName) {
    LayoutCreatorNode* curr = gLayoutCreators;
    while (curr) {
        if (str::EqI(typeName, curr->typeName))
            return curr->creator;
        curr = curr->next;
    }
    return nullptr;
}

void FreeLayoutCreators() {
    LayoutCreatorNode* curr = gLayoutCreators;
    LayoutCreatorNode* next;
    while (curr) {
        next = curr->next;
        str::Free(curr->typeName);
        free(curr);
        curr = next;
    }
}

Button* FindButtonNamed(const ParsedMui& muiInfo, const char* name) {
    for (size_t i = 0; i < muiInfo.buttons.size(); i++) {
        Button* c = muiInfo.buttons.at(i);
        if (c->IsNamed(name))
            return c;
    }
    return nullptr;
}

ButtonVector* FindButtonVectorNamed(const ParsedMui& muiInfo, const char* name) {
    for (size_t i = 0; i < muiInfo.vecButtons.size(); i++) {
        ButtonVector* c = muiInfo.vecButtons.at(i);
        if (c->IsNamed(name))
            return c;
    }
    return nullptr;
}

ScrollBar* FindScrollBarNamed(const ParsedMui& muiInfo, const char* name) {
    for (size_t i = 0; i < muiInfo.scrollBars.size(); i++) {
        ScrollBar* c = muiInfo.scrollBars.at(i);
        if (c->IsNamed(name))
            return c;
    }
    return nullptr;
}

Control* FindControlNamed(const ParsedMui& muiInfo, const char* name) {
    for (size_t i = 0; i < muiInfo.allControls.size(); i++) {
        Control* c = muiInfo.allControls.at(i);
        if (c->IsNamed(name))
            return c;
    }
    return nullptr;
}

ILayout* FindLayoutNamed(const ParsedMui& muiInfo, const char* name) {
    for (size_t i = 0; i < muiInfo.layouts.size(); i++) {
        ILayout* l = muiInfo.layouts.at(i);
        if (l->IsNamed(name))
            return l;
    }
    return nullptr;
}

ILayout* FindElementNamed(ParsedMui& muiInfo, const char* name) {
    Control* c = FindControlNamed(muiInfo, name);
    if (c)
        return c;
    return FindLayoutNamed(muiInfo, name);
}

static TxtNode* GetRootArray(TxtParser* parser) {
    TxtNode* root = parser->nodes.at(0);
    CrashIf(!root->IsArray());
    return root;
}

static float ParseFloat(const char* s) {
    char* end = (char*)s;
    return (float)strtod(s, &end);
}

struct ParsedPadding {
    int top;
    int right;
    int bottom;
    int left;
};

static void ParsePadding(const char* s, ParsedPadding& p) {
    if (str::Parse(s, "%d%_%d%_%d%_%d%_%$", &p.top, &p.right, &p.bottom, &p.left))
        return;
    if (str::Parse(s, "%d%_%d%_%$", &p.top, &p.right)) {
        p.bottom = p.top;
        p.left = p.right;
    } else if (str::Parse(s, "%d%_%$", &p.top)) {
        p.left = p.right = p.bottom = p.top;
    }
}

static AlignAttr ParseAlignAttr(const char* s) {
    return FindAlignAttr(s, str::Len(s));
}

// TODO: optimize using seqstrings
static ElAlign ParseElAlign(const char* s) {
    if (str::EqI(s, "center"))
        return ElAlign::Center;
    if (str::EqI(s, "top"))
        return ElAlign::Top;
    if (str::EqI(s, "bottom"))
        return ElAlign::Bottom;
    if (str::EqI(s, "left"))
        return ElAlign::Left;
    if (str::EqI(s, "right"))
        return ElAlign::Right;
    CrashIf(true);
    return ElAlign::Left;
}

static ElAlignData ParseElAlignData(const char* s) {
    ElAlign align = ParseElAlign(s);
    return GetElAlign(align);
}

#if 0
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
#endif
static Gdiplus::FontStyle ParseFontWeight(const char* s) {
    if (str::EqI(s, "regular"))
        return FontStyleRegular;
    CrashIf(true);
    // TODO: more
    return FontStyleRegular;
}

static void AddStyleProp(Style* style, TxtNode* prop) {
    AutoFree tmp(prop->ValDup());

    if (prop->IsTextWithKey("name")) {
        style->SetName(tmp);
        return;
    }

    if (prop->IsTextWithKey("bg_col")) {
        style->Set(Prop::AllocColorSolid(PropBgColor, tmp));
        return;
    }

    if (prop->IsTextWithKey("col")) {
        style->Set(Prop::AllocColorSolid(PropColor, tmp));
        return;
    }

    if (prop->IsTextWithKey("parent")) {
        Style* parentStyle = StyleByName(tmp);
        CrashIf(!parentStyle);
        style->SetInheritsFrom(parentStyle);
        return;
    }

    if (prop->IsTextWithKey("border_width")) {
        style->SetBorderWidth(ParseFloat(tmp));
        return;
    }

    if (prop->IsTextWithKey("padding")) {
        ParsedPadding padding = {0};
        ParsePadding(tmp, padding);
        style->SetPadding(padding.top, padding.right, padding.bottom, padding.left);
        return;
    }

    if (prop->IsTextWithKey("stroke_width")) {
        style->Set(Prop::AllocWidth(PropStrokeWidth, ParseFloat(tmp)));
        return;
    }

    if (prop->IsTextWithKey("fill")) {
        style->Set(Prop::AllocColorSolid(PropFill, tmp));
        return;
    }

    if (prop->IsTextWithKey("vert_align")) {
        style->Set(Prop::AllocAlign(PropVertAlign, ParseElAlign(tmp)));
        return;
    }

    if (prop->IsTextWithKey("text_align")) {
        style->Set(Prop::AllocTextAlign(ParseAlignAttr(tmp)));
        return;
    }

    if (prop->IsTextWithKey("font_size")) {
        style->Set(Prop::AllocFontSize(ParseFloat(tmp)));
        return;
    }

    if (prop->IsTextWithKey("font_weight")) {
        style->Set(Prop::AllocFontWeight(ParseFontWeight(tmp)));
        return;
    }

    CrashIf(true);
}

static TxtNode* TxtChildNodeWithKey(TxtNode* top, const char* keyName) {
    for (TxtNode* node = top->firstChild; node != nullptr; node = node->sibling) {
        if (node->IsTextWithKey(keyName)) {
            return node;
        }
    }
    return nullptr;
}

// styles are cached globally, so we only add a style if it doesn't
// exist already
static void CacheStyleFromStruct(TxtNode* def) {
    CrashIf(!def->IsStructWithName("style"));
    TxtNode* nameNode = TxtChildNodeWithKey(def, "name");
    CrashIf(!nameNode); // must have name or else no way to refer to it
    AutoFree tmp(nameNode->ValDup());
    if (StyleByName(tmp))
        return;

    Style* style = new Style();
    for (TxtNode* node = def->firstChild; node != nullptr; node = node->sibling) {
        CrashIf(!node->IsText());
        AddStyleProp(style, node);
    }
    CacheStyle(style, nullptr);
}

static ButtonVector* ButtonVectorFromDef(TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("ButtonVector"));
    ButtonVectorDef* def = DeserializeButtonVectorDef(structDef);
    ButtonVector* b = new ButtonVector();
    b->SetName(def->name);
    b->SetNamedEventClick(def->clicked);

    if (def->path) {
        GraphicsPath* gp = svg::GraphicsPathFromPathData(def->path);
        b->SetGraphicsPath(gp);
    }
    if (def->styleDefault) {
        Style* style = StyleByName(def->styleDefault);
        CrashIf(!style);
        b->SetDefaultStyle(style);
    }
    if (def->styleMouseOver) {
        Style* style = StyleByName(def->styleMouseOver);
        CrashIf(!style);
        b->SetMouseOverStyle(style);
    }
    FreeButtonVectorDef(def);
    return b;
}

static Button* ButtonFromDef(TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("Button"));
    ButtonDef* def = DeserializeButtonDef(structDef);
    Style* style = StyleByName(def->style);
    Button* b = new Button(def->text, style, style);
    b->SetName(def->name);
    FreeButtonDef(def);
    return b;
}

static ScrollBar* ScrollBarFromDef(TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("ScrollBar"));
    ScrollBarDef* def = DeserializeScrollBarDef(structDef);
    ScrollBar* sb = new ScrollBar();
    Style* style = StyleByName(def->style);
    sb->SetStyle(style);
    sb->SetName(def->name);

    // TODO: support def->cursor

    FreeScrollBarDef(def);
    return sb;
}

static float ParseLayoutFloat(const char* s) {
    if (str::EqI(s, "self"))
        return SizeSelf;
    return ParseFloat(s);
}

static void SetDirectionalLayouData(DirectionalLayoutData& ld, ParsedMui& parsed, DirectionalLayoutDataDef* def) {
    float sla = ParseLayoutFloat(def->sla);
    float snla = ParseLayoutFloat(def->snla);
    ElAlignData elAlign = ParseElAlignData(def->align);
    ILayout* el = FindElementNamed(parsed, def->controlName);
    ld.Set(el, sla, snla, elAlign);
}

static HorizontalLayout* HorizontalLayoutFromDef(ParsedMui& parsed, TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("HorizontalLayout"));
    HorizontalLayoutDef* def = DeserializeHorizontalLayoutDef(structDef);
    HorizontalLayout* l = new HorizontalLayout();
    l->SetName(def->name);
    Vec<DirectionalLayoutDataDef*>* children = def->children;

    DirectionalLayoutData ld;
    for (size_t i = 0; children && i < children->size(); i++) {
        SetDirectionalLayouData(ld, parsed, children->at(i));
        l->Add(ld);
    }

    FreeHorizontalLayoutDef(def);
    return l;
}

static VerticalLayout* VerticalLayoutFromDef(ParsedMui& parsed, TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("VerticalLayout"));
    VerticalLayoutDef* def = DeserializeVerticalLayoutDef(structDef);
    VerticalLayout* l = new VerticalLayout();
    l->SetName(def->name);
    Vec<DirectionalLayoutDataDef*>* children = def->children;

    DirectionalLayoutData ld;
    for (size_t i = 0; children && i < children->size(); i++) {
        SetDirectionalLayouData(ld, parsed, children->at(i));
        l->Add(ld);
    }

    FreeVerticalLayoutDef(def);
    return l;
}

// TODO: create the rest of controls
static void ParseMuiDefinition(TxtNode* root, ParsedMui& res) {
    for (TxtNode* node = root->firstChild; node != nullptr; node = node->sibling) {
        CrashIf(!node->IsStruct());
        if (node->IsStructWithName("Style")) {
            CacheStyleFromStruct(node);
        } else if (node->IsStructWithName("ButtonVector")) {
            ButtonVector* b = ButtonVectorFromDef(node);
            res.allControls.push_back(b);
            res.vecButtons.push_back(b);
        } else if (node->IsStructWithName("Button")) {
            Button* b = ButtonFromDef(node);
            res.allControls.push_back(b);
            res.buttons.push_back(b);
        } else if (node->IsStructWithName("ScrollBar")) {
            ScrollBar* sb = ScrollBarFromDef(node);
            res.allControls.push_back(sb);
            res.scrollBars.push_back(sb);
        } else if (node->IsStructWithName("HorizontalLayout")) {
            HorizontalLayout* l = HorizontalLayoutFromDef(res, node);
            res.layouts.push_back(l);
        } else if (node->IsStructWithName("VerticalLayout")) {
            VerticalLayout* l = VerticalLayoutFromDef(res, node);
            res.layouts.push_back(l);
        } else {
            AutoFree keyName(node->KeyDup());
            ControlCreatorFunc creatorFunc = FindControlCreatorFuncFor(keyName);
            if (creatorFunc) {
                Control* c = creatorFunc(node);
                if (c) {
                    res.allControls.push_back(c);
                }
                continue;
            }

            LayoutCreatorFunc layoutCreatorFunc = FindLayoutCreatorFuncFor(keyName);
            CrashIf(!layoutCreatorFunc);
            ILayout* layout = layoutCreatorFunc(&res, node);
            if (layout) {
                res.layouts.push_back(layout);
            }
        }
    }
}

// s might be modified in-place
bool MuiFromText(ParsedMui& res, const std::string_view& str) {
    TxtParser parser;
    parser.SetToParse(str);
    bool ok = ParseTxt(parser);
    if (!ok)
        return false;
    ParseMuiDefinition(GetRootArray(&parser), res);
    CrashIf(!ok);
    return ok;
}

} // namespace mui
