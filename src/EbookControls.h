/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookControls_h
#define EbookControls_h

#include "BaseUtil.h"
#include "Mui.h"

class PageControl;
using namespace mui;

// controls managed by EbookController
struct EbookControls {
    HwndWrapper *   mainWnd;
    PageControl *   page;
    ButtonVector *  next;
    ButtonVector *  prev;
    ScrollBar *     progress;
    Button *        status;
};

EbookControls * CreateEbookControls(HWND hwnd);
void            DestroyEbookControls(EbookControls* controls);

class PageData;

// control that shows a single ebook page
// TODO: move to a separate file
class PageControl : public Control
{
    PageData *  page;
    int         cursorX, cursorY;

public:
    PageControl() : page(NULL), cursorX(-1), cursorY(-1) {
        bit::Set(wantedInputBits, WantsMouseMoveBit);
    }
    virtual ~PageControl() { }

    void      SetPage(PageData *newPage);
    PageData* GetPage() const { return page; }

    Size GetDrawableSize();

    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseMove(int x, int y);
};

#endif
