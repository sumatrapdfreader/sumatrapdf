/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

#include "HtmlFormatter.h"
#include "MuiButtonVectorDef.h"
#include "MuiButtonDef.h"
#include "SvgPath.h"
#include "TxtParser.h"
#include "MuiScrollBarDef.h"

namespace mui {

Button *FindButtonNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.buttons.Count(); i++) {
        Button *b = muiInfo.buttons.At(i);
        if (b->IsNamed(name))
            return b;
    }
    return NULL;
}

ButtonVector *FindButtonVectorNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.vecButtons.Count(); i++) {
        ButtonVector *b = muiInfo.vecButtons.At(i);
        if (b->IsNamed(name))
            return b;
    }
    return NULL;
}

ScrollBar *FindScrollBarNamed(ParsedMui& muiInfo, const char *name)
{
    for (size_t i = 0; i < muiInfo.scrollBars.Count(); i++) {
        ScrollBar *sb = muiInfo.scrollBars.At(i);
        if (sb->IsNamed(name))
            return sb;
    }
    return NULL;
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

// TODO: more enums
static AlignAttr ParseAlignAttr(const char *s)
{
    if (str::EqI(s, "center"))
        return Align_Center;
    CrashIf(true);
    return Align_Left;
}

// TODO: more enums
static ElAlign ParseElAlign(const char *s)
{
    if (str::EqI(s, "center"))
        return ElAlignCenter;
    CrashIf(true);
    return ElAlignLeft;
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
    if (def->name)
        b->SetName(def->name);
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
    if (def->name)
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

    if (def->name)
        sb->SetName(def->name);

    // TODO: support def->cursor

    FreeScrollBarDef(def);
    return sb;
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
            res.all.Append(b);
            res.vecButtons.Append(b);
        } else if (node->IsStructWithName("Button")) {
            Button *b = ButtonFromDef(node);
            res.all.Append(b);
            res.buttons.Append(b);
        } else if (node->IsStructWithName("ScrollBar")) {
            ScrollBar *sb = ScrollBarFromDef(node);
            res.all.Append(sb);
            res.scrollBars.Append(sb);
        } else {
            CrashIf(true);
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

