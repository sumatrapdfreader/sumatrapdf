/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#define Mui_h

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "Sigslot.h"
#include "TxtParser.h"

enum AlignAttr;

namespace mui {

using namespace Gdiplus;

#include "MuiBase.h"
#include "MuiCss.h"

using namespace css;
#include "MuiLayout.h"
#include "MuiControl.h"
#include "MuiButton.h"
#include "MuiScrollBar.h"
#include "MuiGrid.h"
#include "MuiHwndWrapper.h"
#include "MuiPainter.h"
#include "MuiEventMgr.h"
#include "MuiFromText.h"

#define SizeInfinite ((INT)-1)

struct CtrlAndOffset {
    Control *c;
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
    virtual bool Matches(Control *c, int offX, int offY) {
        if ((c->wantedInputBits & wantedInputMask) != 0) {
            Rect r = Rect(offX, offY, c->pos.Width, c->pos.Height);
            return r.Contains(x, y);
        }
        return false;
    }
};

void        Initialize();
void        Destroy();
void        SetDebugPaint(bool debug);
bool        IsDebugPaint();
size_t      CollectWindowsAt(Control *wndRoot, int x, int y, uint16 wantedInputMask, Vec<CtrlAndOffset> *ctrls);
void        CollectWindowsBreathFirst(Control *c, int offX, int offY, WndFilter *wndFilter, Vec<CtrlAndOffset> *ctrls);
void        RequestRepaint(Control *c, const Rect *r1 = NULL, const Rect *r2 = NULL);
void        RequestLayout(Control *c);
void        DrawBorder(Graphics *gfx, const Rect r, CachedStyle *s);
HwndWrapper *GetRootHwndWnd(const Control *c);

} // namespace mui

#endif
