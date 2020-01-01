/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/HtmlParserLookup.h"
#include "Mui.h"

namespace mui {

float PercFromInt(int total, int n) {
    CrashIf(n > total);
    if (0 == total)
        return 0.f;
    return (float)n / (float)total;
}

int IntFromPerc(int total, float perc) {
    return (int)(total * perc);
}

ScrollBar::ScrollBar(int onOverDy, int inactiveDy) : onOverDy(onOverDy), inactiveDy(inactiveDy) {
    filledPerc = 0.f;
    bit::Set(wantedInputBits, WantsMouseOverBit, WantsMouseClickBit);
}

Size ScrollBar::Measure(const Size availableSize) {
    // dx is max available
    desiredSize.Width = availableSize.Width;

    // dy is bigger of inactiveDy and onHoverDy but
    // smaller than availableSize.Height
    int dy = inactiveDy;
    if (onOverDy > dy)
        dy = onOverDy;
    if (dy > availableSize.Height)
        dy = availableSize.Height;

    desiredSize.Height = dy;
    return desiredSize;
}

void ScrollBar::NotifyMouseEnter() {
    if (inactiveDy != onOverDy)
        RequestRepaint(this);
}

void ScrollBar::NotifyMouseLeave() {
    if (inactiveDy != onOverDy)
        RequestRepaint(this);
}

void ScrollBar::SetFilled(float perc) {
    CrashIf((perc < 0.f) || (perc > 1.f));
    int prev = IntFromPerc(pos.Width, filledPerc);
    int curr = IntFromPerc(pos.Width, perc);
    filledPerc = perc;
    if (prev != curr)
        RequestRepaint(this);
}

float ScrollBar::GetPercAt(int x) {
    return PercFromInt(pos.Width, x);
}

void ScrollBar::Paint(Graphics* gfx, int offX, int offY) {
    CrashIf(!IsVisible());
    // TODO: take padding into account
    CachedStyle* s = cachedStyle;

    int dy = inactiveDy;
    if (IsMouseOver())
        dy = onOverDy;

    Rect r(offX, offY + pos.Height - dy, pos.Width, dy);
    Brush* br = BrushFromColorData(s->bgColor, r);
    gfx->FillRectangle(br, r);

    int filledDx = IntFromPerc(pos.Width, filledPerc);
    if (0 == filledDx)
        return;

    r.Width = filledDx;
    br = BrushFromColorData(s->color, r);
    gfx->FillRectangle(br, r);
}
} // namespace mui
