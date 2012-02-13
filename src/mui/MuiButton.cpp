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

static void AddBorders(int& dx, int& dy, CachedStyle *s)
{
    const BorderWidth& bw = s->borderWidth;
    // note: width is a float, not sure how I should round them
    dx += (int)(bw.left + bw.right);
    dy += (int)(bw.top + bw.bottom);
}

static Size GetBorderAndPaddingSize(CachedStyle *s)
{
    Padding pad = s->padding;
    int dx = pad.left + pad.right;
    int dy = pad.top  + pad.bottom;
    AddBorders(dx, dy, s);
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

    desiredSize = GetBorderAndPaddingSize(cachedStyle);
    textDx = 0;
    if (text) {
        Graphics *gfx = AllocGraphicsForMeasureText();
        Font *font = CachedFontFromCachedStyle(cachedStyle);
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

    CachedStyle *s = cachedStyle;

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromColorData(s->bgColor, bbox);
    gfx->FillRectangle(brBgColor, bbox);

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, s);
    if (str::IsEmpty(text))
        return;

    Padding pad = s->padding;
    int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, textDx, s->textAlign);
    int x = offX + alignedOffX + pad.left + (int)s->borderWidth.left;
    int y = offY + pad.top + (int)s->borderWidth.top;
    Brush *brColor = BrushFromColorData(s->color, bbox); // restrict bbox to just the text?
    Font *font = CachedFontFromCachedStyle(s);
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

// TODO: the position still seems a bit off wrt. padding
void ButtonVector::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    CachedStyle *s = cachedStyle;
    desiredSize = GetBorderAndPaddingSize(s);

    Rect bbox;
    Brush *brStroke = BrushFromColorData(s->stroke, bbox);
    if (0.f == s->strokeWidth) {
        graphicsPath->GetBounds(&bbox);
    } else {
        Pen pen(brStroke, s->strokeWidth);
        // pen widith is multiplied by MiterLimit(), which is 10 by default
        // so set it explicitly to 1 for the size we expect
        pen.SetMiterLimit(1.f);
        pen.SetAlignment(PenAlignmentInset);
        graphicsPath->GetBounds(&bbox, NULL, &pen);
    }
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

    CachedStyle *s = cachedStyle;

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromColorData(s->bgColor, bbox);
    gfx->FillRectangle(brBgColor, bbox);

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, s);
    if (!graphicsPath)
        return;

    // graphicsPath bbox can have non-zero X,Y, so must take that
    // into account

    Brush *brFill = BrushFromColorData(s->fill, bbox);
    Brush *brStroke = BrushFromColorData(s->stroke, bbox);
    Pen pen(brStroke, s->strokeWidth);
    pen.SetMiterLimit(1.f);
    pen.SetAlignment(PenAlignmentInset);
    if (0.f == s->strokeWidth)
        graphicsPath->GetBounds(&bbox);
    else
        graphicsPath->GetBounds(&bbox, NULL, &pen);

    // TODO: center the path both vertically and horizontally

    Padding pad = s->padding;
    //int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, desiredSize.Width, Align_Center);
    int x = offX + pad.left + (int)(s->borderWidth.left - bbox.X);
    int y = offY + pad.top + (int)(s->borderWidth.top - bbox.Y);

    // TODO: can I avoid making a copy of GraphicsPath?
    GraphicsPath *tmp = graphicsPath->Clone();
    Matrix m;
    m.Translate((float)x, (float)y);
    tmp->Transform(&m);
    gfx->FillPath(brFill, tmp);
    if (0.f != s->strokeWidth)
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
