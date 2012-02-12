/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

namespace mui {

Button::Button(const TCHAR *s)
{
    text = NULL;
    wantedInputBits = (uint16)-1; // wants everything
    styleDefault = NULL;
    styleMouseOver = NULL;
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    SetText(s);
}

Button::~Button()
{
    free(text);
}

static void AddBorders(int& dx, int& dy, Prop **props)
{
    Prop *p1 = props[PropBorderLeftWidth];
    Prop *p2 =  props[PropBorderRightWidth];
    // note: width is a float, not sure how I should round them
    dx += (int)(p1->width + p2->width);
    p1 = props[PropBorderTopWidth];
    p2 =  props[PropBorderBottomWidth];
    dy += (int)(p1->width + p2->width);
}

static Size GetBorderAndPaddingSize(Prop **props)
{
    Padding pad = props[PropPadding]->padding;
    int dx = pad.left + pad.right;
    int dy = pad.top  + pad.bottom;
    AddBorders(dx, dy, props);
    return Size(dx, dy);
}

void Button::NotifyMouseEnter()
{
    SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    RecalculateSize(true);
}

void Button::NotifyMouseLeave()
{
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    RecalculateSize(true);
}

// Update desired size of the button.
// If the size changes, trigger layout (which will in
// turn request repaints of affected areas)
void Button::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    desiredSize = GetBorderAndPaddingSize(GetCachedProps());
    textDx = 0;
    if (text) {
        Graphics *gfx = AllocGraphicsForMeasureText();
        Font *font = CachedFontFromCachedProps(GetCachedProps());
        RectF bbox = MeasureText(gfx, font, text);
        textDx = (size_t)bbox.Width; // TODO: round up?
        desiredSize.Width  += textDx;
        desiredSize.Height += (INT)bbox.Height; // TODO: round up?
        FreeGraphicsForMeasureText(gfx);
    }

    if (!prevSize.Equals(desiredSize))
        RequestLayout(this);
    else if (repaintIfSizeDidntChange)
        RequestRepaint(this);
}

void Button::SetText(const TCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize(true);
}

void Button::Measure(const Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
}

void Button::SetStyles(Style *def, Style *mouseOver)
{
    styleDefault = def;
    styleMouseOver = mouseOver;

    if (IsMouseOver())
        SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    else
        SetCurrentStyle(styleDefault, gStyleButtonDefault);

    RecalculateSize(true);
}

// given the size of a container, the size of an element inside
// a container and alignment, calculates the position of
// element within the container.
static int AlignedOffset(int containerDx, int elDx, AlignAttr align)
{
    if (Align_Left == align)
        return 0;
    if (Align_Right == align)
        return containerDx - elDx;
    // Align_Center or Align_Justify
    return (containerDx - elDx) / 2;
}

void Button::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;

    Prop **props = GetCachedProps();
    Prop *col   = props[PropColor];
    Prop *bgCol = props[PropBgColor];
    Prop *padding = props[PropPadding];
    Prop *topWidth = props[PropBorderTopWidth];
    Prop *leftWidth = props[PropBorderLeftWidth];
    Prop *textAlign = props[PropTextAlign];

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromProp(bgCol, bbox);
    gfx->FillRectangle(brBgColor, bbox);

    BorderProps bp = {
        props[PropBorderTopWidth], props[PropBorderTopColor],
        props[PropBorderRightWidth], props[PropBorderRightColor],
        props[PropBorderBottomWidth], props[PropBorderBottomColor],
        props[PropBorderLeftWidth], props[PropBorderLeftColor],
    };

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, bp);
    if (str::IsEmpty(text))
        return;

    Padding pad = padding->padding;
    int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, textDx, textAlign->textAlign);
    int x = offX + alignedOffX + pad.left + (int)leftWidth->width;
    int y = offY + pad.top + (int)topWidth->width;
    Brush *brColor = BrushFromProp(col, bbox); // restrict bbox to just the text?
    Font *font = CachedFontFromCachedProps(props);
    gfx->DrawString(text, str::Len(text), font, PointF((REAL)x, (REAL)y), NULL, brColor);
}


ButtonVector::ButtonVector(GraphicsPath *gp)
{
    wantedInputBits = (uint16)-1; // wants everything
    styleDefault = NULL;
    styleMouseOver = NULL;
    graphicsPath = NULL;
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    SetGraphicsPath(gp);
}

ButtonVector::~ButtonVector()
{
    ::delete graphicsPath;
}

void ButtonVector::NotifyMouseEnter()
{
    SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    RecalculateSize(true);
}

void ButtonVector::NotifyMouseLeave()
{
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    RecalculateSize(true);
}

void ButtonVector::SetGraphicsPath(GraphicsPath *gp)
{
    ::delete graphicsPath;
    graphicsPath = gp;
    RecalculateSize(true);
}

void ButtonVector::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    desiredSize = GetBorderAndPaddingSize(GetCachedProps());

    Prop **props = GetCachedProps();
    Prop *stroke = props[PropStroke];
    Prop *strokeWidth = props[PropStrokeWidth];
    Rect bbox;
    Brush *brStroke = BrushFromProp(stroke, bbox);
    Pen pen(brStroke, strokeWidth->width);
    graphicsPath->GetBounds(&bbox, NULL, &pen);
    desiredSize.Width  += bbox.Width;
    desiredSize.Height += bbox.Height;

    if (!prevSize.Equals(desiredSize))
        RequestLayout(this);
    else if (repaintIfSizeDidntChange)
        RequestRepaint(this);
}

void ButtonVector::Measure(const Size availableSize)
{
    // do nothing: calculated in RecalculateSize()
}

void ButtonVector::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;

    Prop **props = GetCachedProps();
    Prop *col   = props[PropColor];
    Prop *bgCol = props[PropBgColor];
    Prop *padding = props[PropPadding];
    Prop *topWidth = props[PropBorderTopWidth];
    Prop *leftWidth = props[PropBorderLeftWidth];
    Prop *fill = props[PropFill];
    Prop *stroke = props[PropStroke];
    Prop *strokeWidth = props[PropStrokeWidth];

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromProp(bgCol, bbox);
    gfx->FillRectangle(brBgColor, bbox);

    BorderProps bp = {
        props[PropBorderTopWidth], props[PropBorderTopColor],
        props[PropBorderRightWidth], props[PropBorderRightColor],
        props[PropBorderBottomWidth], props[PropBorderBottomColor],
        props[PropBorderLeftWidth], props[PropBorderLeftColor],
    };

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, bp);
    if (!graphicsPath)
        return;

    // TODO: center the path both vertically and horizontally

    Padding pad = padding->padding;
    //int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, desiredSize.Width, Align_Center);
    int x = offX + pad.left + (int)leftWidth->width;
    int y = offY + pad.top + (int)topWidth->width;

    // TODO: can I avoid making a copy of GraphicsPath?
    GraphicsPath *tmp = graphicsPath->Clone();
    Matrix m;
    m.Translate((float)x, (float)y);
    tmp->Transform(&m);
    Brush *brStroke = BrushFromProp(stroke, bbox);
    Brush *brFill = BrushFromProp(fill, bbox);
    gfx->FillPath(brFill, tmp);
    Pen pen(brStroke, strokeWidth->width);
    gfx->DrawPath(&pen, tmp);
}

void ButtonVector::SetStyles(Style *def, Style *mouseOver)
{
    styleDefault = def;
    styleMouseOver = mouseOver;

    if (IsMouseOver())
        SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    else
        SetCurrentStyle(styleDefault, gStyleButtonDefault);

    RecalculateSize(true);
}


}
