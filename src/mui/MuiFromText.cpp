/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

#include "HtmlParserLookup.h"
#include "SquareTreeParser.h"
#include "SvgPath.h"

namespace mui {

void ParsedMui::AddControl(Control *ctrl, const char *type)
{
    controls.Append(ctrl);
    controlTypes.Append(str::Dup(type));
}

void ParsedMui::AddLayout(ILayout *layout)
{
    layouts.Append(layout);
}

Control *ParsedMui::FindControl(const char *name, const char *type) const
{
    for (size_t i = 0; i < controls.Count(); i++) {
        if (controls.At(i)->IsNamed(name) && (!type || str::Eq(controlTypes.At(i), type)))
            return controls.At(i);
    }
    return NULL;
}

ILayout *ParsedMui::FindLayout(const char *name, bool alsoControls) const
{
    for (size_t i = 0; i < layouts.Count(); i++) {
        if (layouts.At(i)->IsNamed(name))
            return layouts.At(i);
    }
    if (alsoControls)
        return FindControl(name, NULL);
    return NULL;
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
    if (str::EqI(s, "bottom"))
        return ElAlignBottom;
    if (str::EqI(s, "left"))
        return ElAlignLeft;
    if (str::EqI(s, "right"))
        return ElAlignRight;
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

// styles are cached globally, so we only add a style if it doesn't
// exist already
static void CacheStyleFromStruct(SquareTreeNode *def)
{
    const char *value = def->GetValue("name");
    CrashIf(!value); // must have name or else no way to refer to it
    if (StyleByName(value))
        return;

    Style *style = new Style();
    style->SetName(value);
    if ((value = def->GetValue("bg_col")) != NULL)
        style->Set(Prop::AllocColorSolid(PropBgColor, value));
    if ((value = def->GetValue("col")) != NULL)
        style->Set(Prop::AllocColorSolid(PropColor, value));
    if ((value = def->GetValue("parent")) != NULL) {
        Style *parentStyle = StyleByName(value);
        CrashIf(!parentStyle);
        style->SetInheritsFrom(parentStyle);
    }
    if ((value = def->GetValue("border_width")) != NULL)
        style->SetBorderWidth(ParseFloat(value));
    if ((value = def->GetValue("padding")) != NULL) {
        ParsedPadding padding = { 0 };
        ParsePadding(value, padding);
        style->SetPadding(padding.top, padding.right, padding.bottom, padding.left);
    }
    if ((value = def->GetValue("stroke_width")) != NULL)
        style->Set(Prop::AllocWidth(PropStrokeWidth, ParseFloat(value)));
    if ((value = def->GetValue("fill")) != NULL)
        style->Set(Prop::AllocColorSolid(PropFill, value));
    if ((value = def->GetValue("vert_align")) != NULL)
        style->Set(Prop::AllocAlign(PropVertAlign, ParseElAlign(value)));
    if ((value = def->GetValue("text_align")) != NULL)
        style->Set(Prop::AllocTextAlign(ParseAlignAttr(value)));
    if ((value = def->GetValue("font_size")) != NULL)
        style->Set(Prop::AllocFontSize(ParseFloat(value)));
    if ((value = def->GetValue("font_weight")) != NULL)
        style->Set(Prop::AllocFontWeight(ParseFontWeight(value)));

    CacheStyle(style);
}

static ButtonVector *ButtonVectorFromDef(SquareTreeNode *def)
{
    ButtonVector *b = new ButtonVector();

    b->SetName(def->GetValue("name"));
    b->SetNamedEventClick(def->GetValue("clicked"));

    const char *value;
    if ((value = def->GetValue("path")) != NULL)
        b->SetGraphicsPath(svg::GraphicsPathFromPathData(value));
    if ((value = def->GetValue("style_default")) != NULL) {
        Style *style = StyleByName(value);
        CrashIf(!style);
        b->SetDefaultStyle(style);
    }
    if ((value = def->GetValue("style_mouse_over")) != NULL) {
        Style *style = StyleByName(value);
        CrashIf(!style);
        b->SetMouseOverStyle(style);
    }

    return b;
}

static Button *ButtonFromDef(SquareTreeNode *def)
{
    Style *style = StyleByName(def->GetValue("style"));
    ScopedMem<WCHAR> text;
    if (def->GetValue("text"))
        text.Set(str::conv::FromUtf8(def->GetValue("text")));
    Button *b = new Button(text, style, style);
    b->SetName(def->GetValue("name"));
    return b;
}

static ScrollBar *ScrollBarFromDef(SquareTreeNode *def)
{
    ScrollBar *sb = new ScrollBar();
    Style *style = StyleByName(def->GetValue("style"));
    sb->SetStyle(style);
    sb->SetName(def->GetValue("name"));

    // TODO: support def->cursor

    return sb;
}

static float ParseLayoutFloat(const char *s)
{
    if (str::StartsWithI(s, "self") && (!s[4] || str::IsWs(s[4])))
        return SizeSelf;
    return ParseFloat(s);
}

static void NextToken(const char *& s)
{
    for (; *s && !str::IsWs(*s); s++);
    for (; str::IsWs(*s); s++);
}

static void SetDirectionalLayoutData(DirectionalLayoutData& ld, SquareTreeNode::DataItem *item, ParsedMui *parsed)
{
    CrashIf(item->isChild);
    const char *data = item->value.str;
    float sla = ParseLayoutFloat(data);
    NextToken(data);
    float snla = ParseLayoutFloat(data);
    NextToken(data);
    ElAlign align = ParseElAlign(data);

    ILayout *el = parsed->FindLayout(item->key, true);
    ld.Set(el, sla, snla, GetElAlign(align));
}

static DirectionalLayout *LayoutFromDef(const char *name, SquareTreeNode *def, ParsedMui *parsed)
{
    DirectionalLayout *l;
    if (str::Eq(name, "HorizontalLayout"))
        l = new HorizontalLayout();
    else
        l = new VerticalLayout();
    l->SetName(def->GetValue("name"));

    SquareTreeNode *children = def->GetChild("children");
    for (size_t i = 0; children && i < children->data.Count(); i++) {
        DirectionalLayoutData ld;
        SetDirectionalLayoutData(ld, &children->data.At(i), parsed);
        l->Add(ld);
    }

    return l;
}

// TODO: create the rest of controls
ParsedMui *ParsedMui::Create(char *s, HwndWrapper *owner, UnknownControlCallback cb)
{
    SquareTree sqt(s);
    if (!sqt.root)
        return NULL;

    ParsedMui *res = new ParsedMui();
    for (SquareTreeNode::DataItem *item = sqt.root->data.IterStart(); item; item = sqt.root->data.IterNext()) {
        CrashIf(!item->isChild);
        if (str::Eq(item->key, "Style"))
            CacheStyleFromStruct(item->value.child);
        else if (str::Eq(item->key, "ButtonVector"))
            res->AddControl(ButtonVectorFromDef(item->value.child), item->key);
        else if (str::Eq(item->key, "Button"))
            res->AddControl(ButtonFromDef(item->value.child), item->key);
        else if (str::Eq(item->key, "ScrollBar"))
            res->AddControl(ScrollBarFromDef(item->value.child), item->key);
        else if (str::Eq(item->key, "HorizontalLayout") || str::Eq(item->key, "VerticalLayout"))
            res->AddLayout(LayoutFromDef(item->key, item->value.child, res));
        else {
            CrashIf(!cb);
            Control *c = cb(item->key, item->value.child);
            if (c)
                res->AddControl(c, item->key);
        }
    }
    for (size_t i = 0; i < res->controls.Count(); i++) {
        owner->AddChild(res->controls.At(i));
    }
    return res;
}

} // namespace mui
