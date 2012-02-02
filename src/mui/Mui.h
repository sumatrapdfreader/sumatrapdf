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

// Graphics objects cannot be used across threads. This class
// allows an easy allocation of small Graphics objects that
// can be used for measuring text
class GraphicsForMeasureText
{
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    Graphics *  gfx;
    Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];
public:
    GraphicsForMeasureText();
    ~GraphicsForMeasureText();
    bool Create();
    Graphics *Get();
};

struct BorderProps {
    Prop *  topWidth, *topColor;
    Prop *  rightWidth, *rightColor;
    Prop *  bottomWidth, *bottomColor;
    Prop *  leftWidth, *leftColor;
};

void                     Initialize();
void                     Destroy();
GraphicsForMeasureText * AllocGraphicsForMeasureText();
Graphics *               UIThreadGraphicsForMeasureText();
void                     InitGraphicsMode(Graphics *g);
size_t                   CollectWindowsAt(Control *wndRoot, int x, int y, uint16 wantedInputMask, Vec<WndAndOffset> *windows);
void                     CollectWindowsBreathFirst(Control *w, int offX, int offY, WndFilter *wndFilter, Vec<WndAndOffset> *windows);
void                     RequestRepaint(Control *w, const Rect *r1 = NULL, const Rect *r2 = NULL);
void                     RequestLayout(Control *w);
Brush *                  BrushFromProp(Prop *p, const Rect& r);
Brush *                  BrushFromProp(Prop *p, const RectF& r);
HwndWrapper *            GetRootHwndWnd(const Control *w);
void                     DrawBorder(Graphics *gfx, const Rect r, const BorderProps& bp);

} // namespace mui

#endif
