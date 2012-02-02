/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#define Mui_h

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "Vec.h"
#include "BitManip.h"
#include "GeomUtil.h"
#include "HtmlPullParser.h"

namespace mui {

using namespace Gdiplus;

#include "MuiBase.h"
#include "MuiCss.h"

using namespace css;
#include "MuiControl.h"
#include "MuiButton.h"
#include "MuiScrollBar.h"
#include "MuiHwndWrapper.h"
#include "MuiLayout.h"
#include "MuiPainter.h"
#include "MuiEventMgr.h"

#define SizeInfinite ((INT)-1)

struct WndAndOffset {
    Control *wnd;
    int offX, offY;
};

class WndFilter
{
public:
    bool skipInvisibleSubtrees;

    WndFilter() : skipInvisibleSubtrees(true) {}

    virtual ~WndFilter() {}

    virtual bool Matches(Control *w, int offX, int offY) {
        return true;
    }
};

class WndInputWantedFilter : public WndFilter
{
    int x, y;
    uint16 wantedInputMask;

public:
    WndInputWantedFilter(int x, int y, uint16 wantedInputMask) :
        x(x), y(y), wantedInputMask(wantedInputMask)
    {
    }
    virtual ~WndInputWantedFilter() {}
    virtual bool Matches(Control *w, int offX, int offY) {
        if ((w->wantedInputBits & wantedInputMask) != 0) {
            Rect r = Rect(offX, offY, w->pos.Width, w->pos.Height);
            return r.Contains(x, y);
        }
        return false;
    }
};

struct BorderProps {
    Prop *  topWidth, *topColor;
    Prop *  rightWidth, *rightColor;
    Prop *  bottomWidth, *bottomColor;
    Prop *  leftWidth, *leftColor;
};

void        Initialize();
void        Destroy();
size_t      CollectWindowsAt(Control *wndRoot, int x, int y, uint16 wantedInputMask, Vec<WndAndOffset> *windows);
void        CollectWindowsBreathFirst(Control *w, int offX, int offY, WndFilter *wndFilter, Vec<WndAndOffset> *windows);
void        RequestRepaint(Control *w, const Rect *r1 = NULL, const Rect *r2 = NULL);
void        RequestLayout(Control *w);
void        DrawBorder(Graphics *gfx, const Rect r, const BorderProps& bp);
HwndWrapper *GetRootHwndWnd(const Control *w);

} // namespace mui

#endif
