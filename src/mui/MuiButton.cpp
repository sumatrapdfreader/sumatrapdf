/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlParserLookup.h"

namespace mui {

Button::Button(const WCHAR *s, Style *def, Style *mouseOver)
{
    text = NULL;
    wantedInputBits = (uint16)-1; // wants everything
    styleDefault = def;
    styleMouseOver = mouseOver;
    SetStyle(styleDefault);
    SetText(s);
}

Button::~Button()
{
    free(text);
}

void Button::NotifyMouseEnter()
{
    Control::NotifyMouseEnter();
    bool changed = SetStyle(styleMouseOver);
    if (changed)
        RecalculateSize(true);
}

void Button::NotifyMouseLeave()
{
    Control::NotifyMouseLeave();
    bool changed = SetStyle(styleDefault);
    if (changed)
        RecalculateSize(true);
}

// Update desired size of the button. If the size changes, trigger layout
// (which will in turn request repaints of affected areas)
// To keep size of the button stable (to avoid re-layouts) we use font
// line spacing to be the height of the button, even if text is empty.
// Note: it might be that for some cases button with no text should collapse
// in size but we don't have a need for that yet
void Button::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    desiredSize = GetBorderAndPaddingSize(cachedStyle);
    Graphics *gfx = AllocGraphicsForMeasureText();
    CachedStyle *s = cachedStyle;
    Font *font = GetCachedFont(s->fontName, s->fontSize, s->fontWeight);

    textDx = 0;
    float fontDy = font->GetHeight(gfx);
    RectF bbox;
    if (text) {
        bbox = MeasureText(gfx, font, text);
        textDx = (size_t)bbox.Width; // TODO: round up?
        // bbox shouldn't be bigger than fontDy. We apply magic adjustment because
        // bbox is bigger in n-th decimal point
        CrashIf(fontDy + .5f < bbox.Height);
    }
    desiredSize.Width  += textDx;
    desiredSize.Height += (INT)fontDy; // TODO: round up?
    FreeGraphicsForMeasureText(gfx);

    if (!prevSize.Equals(desiredSize))
        RequestLayout(this);
    else if (repaintIfSizeDidntChange)
        RequestRepaint(this);
}

void Button::SetText(const WCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize(true);
}

Size Button::Measure(const Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
    return desiredSize;
}

void Button::SetStyles(Style *def, Style *mouseOver)
{
    styleDefault = def;
    styleMouseOver = mouseOver;

    if (IsMouseOver())
        SetStyle(styleMouseOver);
    else
        SetStyle(styleDefault);

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
    Font *font = GetCachedFont(s->fontName, s->fontSize, s->fontWeight);
    gfx->DrawString(text, str::Len(text), font, PointF((REAL)x, (REAL)y), NULL, brColor);
}

ButtonVector::ButtonVector(GraphicsPath *gp)
{
    wantedInputBits = (uint16)-1; // wants everything
    styleDefault = NULL;
    styleMouseOver = NULL;
    graphicsPath = NULL;
    SetStyle(styleDefault);
    SetGraphicsPath(gp);
}

ButtonVector::~ButtonVector()
{
    ::delete graphicsPath;
}

void ButtonVector::NotifyMouseEnter()
{
    Control::NotifyMouseEnter();
    bool changed = SetStyle(styleMouseOver);
    if (changed)
        RecalculateSize(true);
}

void ButtonVector::NotifyMouseLeave()
{
    Control::NotifyMouseLeave();
    bool changed = SetStyle(styleDefault);
    if (changed)
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

Size ButtonVector::Measure(const Size availableSize)
{
    // do nothing: calculated in RecalculateSize()
    return desiredSize;
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

    // graphicsPath bbox can have non-zero X,Y
    Rect gpBbox;
    Brush *brFill = BrushFromColorData(s->fill, bbox);
    Brush *brStroke = BrushFromColorData(s->stroke, bbox);
    Pen pen(brStroke, s->strokeWidth);
    pen.SetMiterLimit(1.f);
    pen.SetAlignment(PenAlignmentInset);
    if (0.f == s->strokeWidth)
        graphicsPath->GetBounds(&gpBbox);
    else
        graphicsPath->GetBounds(&gpBbox, NULL, &pen);

    // calculate the position of graphics path within given button position, size
    // and desired vertical/horizontal alignment.
    // Note: alignment is calculated against the size after substracting 
    // ncSize is the size of the non-client parts i.e. border and padding, on both sides
    Size ncSize = GetBorderAndPaddingSize(s);
    int elOffY = s->vertAlign.CalcOffset( gpBbox.Height, pos.Height - ncSize.Height);
    int elOffX = s->horizAlign.CalcOffset(gpBbox.Width,  pos.Width  - ncSize.Width );

    int x = offX + elOffX + s->padding.left + (int)s->borderWidth.left + gpBbox.X;
    int y = offY + elOffY + s->padding.top  + (int)s->borderWidth.top  + gpBbox.Y;

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
        SetStyle(styleMouseOver);
    else
        SetStyle(styleDefault);

    RecalculateSize(true);
}


}
