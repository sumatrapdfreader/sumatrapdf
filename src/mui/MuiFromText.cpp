/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

#include "HtmlParserLookup.h"
#include "MuiDefs.h"
#include "SvgPath.h"
#include "TxtParser.h"

namespace mui {

struct ControlCreatorNode {
    ControlCreatorNode *    next;
    const char *            typeName;
    ControlCreatorFunc      creator;
};

// This is an extensiblity point that allows creating custom controls unknown
// to mui that appear in text description
static ControlCreatorNode *gControlCreators = NULL;

void RegisterControlCreatorFor(const char *typeName, ControlCreatorFunc creator)
{
    ControlCreatorNode *cc = AllocStruct<ControlCreatorNode>();
    cc->typeName = str::Dup(typeName);
    cc->creator = creator;
    ListInsert(&gControlCreators, cc);
}

static ControlCreatorFunc FindCreatorFuncFor(const char *typeName)
{
    ControlCreatorNode *curr = gControlCreators;
    while (curr) {
        if (str::EqI(typeName, curr->typeName))
            return curr->creator;
        curr = curr->next;
    }
    return NULL;
}

void FreeControlCreators()
{
    ControlCreatorNode *curr = gControlCreators;
    ControlCreatorNode *next;
    while (curr) {
        next = curr->next;
        free((void*)curr->typeName);
        free(curr);
        curr = next;
    }
}

Button *FindButtonNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.buttons.Count(); i++) {
        Button *c = muiInfo.buttons.At(i);
        if (c->IsNamed(name))
            return c;
    }
    return NULL;
}

ButtonVector *FindButtonVectorNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.vecButtons.Count(); i++) {
        ButtonVector *c = muiInfo.vecButtons.At(i);
        if (c->IsNamed(name))
            return c;
    }
    return NULL;
}

ScrollBar *FindScrollBarNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.scrollBars.Count(); i++) {
        ScrollBar *c = muiInfo.scrollBars.At(i);
        if (c->IsNamed(name))
            return c;
    }
    return NULL;
}

Control *FindControlNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.allControls.Count(); i++) {
        Control *c = muiInfo.allControls.At(i);
        if (c->IsNamed(name))
            return c;
    }
    return NULL;
}

ILayout *FindLayoutNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.layouts.Count(); i++) {
        ILayout *l = muiInfo.layouts.At(i);
        if (l->IsNamed(name))
            return l;
    }
    return NULL;
}

ILayout *FindElementNamed(ParsedMui& muiInfo, const char *name)
{
    Control *c = FindControlNamed(muiInfo, name);
    if (c)
        return c;
    return FindLayoutNamed(muiInfo, name);
}


static TxtNode *GetRootArray(TxtParser* parser)
{
    TxtNode *root = parser->nodes.At(0);
    CrashIf(!root->IsArray());
    return root;
}

static float ParseFloat(const char *s)
{
    char *end = (char*)s;
    return (float)strtod(s, &end);
}

struct ParsedPadding {
    int top;
    int right;
    int bottom;
    int left;
};

// TODO: be more forgiving with whitespace
// TODO: allow 1 or 2 elements
static void ParsePadding(const char *s, ParsedPadding& p)
{
    str::Parse(s, "%d %d %d %d", &p.top, &p.right, &p.bottom, &p.left);
}

static AlignAttr ParseAlignAttr(const char *s)
{
    return FindAlignAttr(s, str::Len(s));
}

// TODO: optimize using seqstrings
static ElAlign ParseElAlign(const char *s)
{
    if (str::EqI(s, "center"))
        return ElAlignCenter;
    if (str::EqI(s, "top"))
        return ElAlignTop;
    if (str::EqI(s, "bottome"))
        return ElAlignBottom;
    if (str::EqI(s, "left"))
        return ElAlignLeft;
    if (str::EqI(s, "right"))
        return ElAlignRight;
    CrashIf(true);
    return ElAlignLeft;
}

static ElAlignData ParseElAlignData(const char *s)
{
    if (str::EqI(s, "center"))
        return GetElAlignCenter();
    if (str::EqI(s, "top"))
        return GetElAlignTop();
    if (str::EqI(s, "bottom"))
        return GetElAlignBottom();
    if (str::EqI(s, "left"))
        return GetElAlignLeft();
    if (str::EqI(s, "right"))
        return GetElAlignRight();
    CrashIf(true);
    return GetElAlignCenter();
}

#if 0
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
#endif
static Gdiplus::FontStyle ParseFontWeight(const char *s)
{
    if (str::EqI(s, "regular"))
        return FontStyleRegular;
    CrashIf(true);
    // TODO: more
    return FontStyleRegular;
}

static void AddStyleProp(Style *style, TxtNode *prop)
{
    ScopedMem<char> tmp(prop->ValDup());

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
        Style *parentStyle = StyleByName(tmp);
        CrashIf(!parentStyle);
        style->SetInheritsFrom(parentStyle);
        return;
    }

    if (prop->IsTextWithKey("border_width")) {
        style->SetBorderWidth(ParseFloat(tmp));
        return;
    }

    if (prop->IsTextWithKey("padding")) {
        ParsedPadding padding = { 0 };
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

static Style* StyleFromStruct(TxtNode* def)
{
    CrashIf(!def->IsStructWithName("style"));
    Style *style = new Style();
    size_t n = def->children->Count();
    for (size_t i = 0; i < n; i++) {
        TxtNode *node = def->children->At(i);
        CrashIf(!node->IsText());
        AddStyleProp(style, node);
    }
    CacheStyle(style);
    return style;
}

static ButtonVector* ButtonVectorFromDef(TxtNode* structDef)
{
    CrashIf(!structDef->IsStructWithName("ButtonVector"));
    ButtonVectorDef *def = DeserializeButtonVectorDef(structDef);
    ButtonVector *b = new ButtonVector();
    b->SetName(def->name);
    b->SetNamedEventClick(def->clicked);

    if (def->path ){
        GraphicsPath *gp = svg::GraphicsPathFromPathData(def->path);
        b->SetGraphicsPath(gp);
    }
    if (def->styleDefault) {
        Style *style = StyleByName(def->styleDefault);
        CrashIf(!style);
        b->SetDefaultStyle(style);
    }
    if (def->styleMouseOver) {
        Style *style = StyleByName(def->styleMouseOver);
        CrashIf(!style);
        b->SetMouseOverStyle(style);
    }
    FreeButtonVectorDef(def);
    return b;
}

static Button* ButtonFromDef(TxtNode* structDef)
{
    CrashIf(!structDef->IsStructWithName("Button"));
    ButtonDef *def = DeserializeButtonDef(structDef);
    Style *style = StyleByName(def->style);
    Button *b = new Button(def->text, style, style);
    b->SetName(def->name);
    FreeButtonDef(def);
    return b;
}

static ScrollBar *ScrollBarFromDef(TxtNode *structDef)
{
    CrashIf(!structDef->IsStructWithName("ScrollBar"));
    ScrollBarDef *def = DeserializeScrollBarDef(structDef);
    ScrollBar *sb = new ScrollBar();
    Style *style = StyleByName(def->style);
    sb->SetStyle(style);
    sb->SetName(def->name);

    // TODO: support def->cursor

    FreeScrollBarDef(def);
    return sb;
}

static float ParseLayoutFloat(const char *s)
{
    if (str::EqI(s, "self"))
        return SizeSelf;
    return ParseFloat(s);
}

static void SetDirectionalLayouData(DirectionalLayoutData& ld, ParsedMui& parsed, DirectionalLayoutDataDef *def)
{
    float sla = ParseLayoutFloat(def->sla);
    float snla = ParseLayoutFloat(def->snla);
    ElAlignData elAlign = ParseElAlignData(def->align);
    ILayout *el = FindElementNamed(parsed, def->controlName);
    ld.Set(el, sla, snla, elAlign);
}

static HorizontalLayout *HorizontalLayoutFromDef(ParsedMui& parsed, TxtNode *structDef)
{
    CrashIf(!structDef->IsStructWithName("HorizontalLayout"));
    HorizontalLayoutDef *def = DeserializeHorizontalLayoutDef(structDef);
    HorizontalLayout *l = new HorizontalLayout();
    l->SetName(def->name);
    Vec<DirectionalLayoutDataDef*> *children = def->children;

    DirectionalLayoutData ld;
    for (size_t i = 0; children && i < children->Count(); i++) {
        SetDirectionalLayouData(ld, parsed, children->At(i));
        l->Add(ld);
    }

    FreeHorizontalLayoutDef(def);
    return l;
}

static VerticalLayout *VerticalLayoutFromDef(ParsedMui& parsed, TxtNode *structDef)
{
    CrashIf(!structDef->IsStructWithName("VerticalLayout"));
    VerticalLayoutDef *def = DeserializeVerticalLayoutDef(structDef);
    VerticalLayout *l = new VerticalLayout();
    l->SetName(def->name);
    Vec<DirectionalLayoutDataDef*> *children = def->children;

    DirectionalLayoutData ld;
    for (size_t i = 0; children && i < children->Count(); i++) {
        SetDirectionalLayouData(ld, parsed, children->At(i));
        l->Add(ld);
    }

    FreeVerticalLayoutDef(def);
    return l;
}

// TODO: create the rest of controls
static void ParseMuiDefinition(TxtNode *root, ParsedMui& res)
{
    TxtNode **n;
    for (n = root->children->IterStart(); n; n = root->children->IterNext()) {
        TxtNode *node = *n;
        CrashIf(!node->IsStruct());
        if (node->IsStructWithName("Style")) {
            res.styles.Append(StyleFromStruct(node));
        } else if (node->IsStructWithName("ButtonVector")) {
            ButtonVector *b = ButtonVectorFromDef(node);
            res.allControls.Append(b);
            res.vecButtons.Append(b);
        } else if (node->IsStructWithName("Button")) {
            Button *b = ButtonFromDef(node);
            res.allControls.Append(b);
            res.buttons.Append(b);
        } else if (node->IsStructWithName("ScrollBar")) {
            ScrollBar *sb = ScrollBarFromDef(node);
            res.allControls.Append(sb);
            res.scrollBars.Append(sb);
        } else if (node->IsStructWithName("HorizontalLayout")) {
            HorizontalLayout *l = HorizontalLayoutFromDef(res, node);
            res.layouts.Append(l);
        } else if (node->IsStructWithName("VerticalLayout")) {
            VerticalLayout *l = VerticalLayoutFromDef(res, node);
            res.layouts.Append(l);
        } else {
            ScopedMem<char> keyName(node->KeyDup());
            ControlCreatorFunc creatorFunc = FindCreatorFuncFor(keyName);
            CrashIf(!creatorFunc);
            Control *c = creatorFunc(node);
            if (c)
                res.allControls.Append(c);
        }
    }
}

bool MuiFromText(char *s, ParsedMui& res)
{
    TxtParser parser;
    parser.SetToParse(s, str::Len(s));
    bool ok = ParseTxt(parser);
    if (!ok)
        return false;
    ParseMuiDefinition(GetRootArray(&parser), res);
    CrashIf(!ok);
    return ok;
}

} // namespace mui

