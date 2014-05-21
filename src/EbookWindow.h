/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EbookWindow_h
#define EbookWindow_h

#include "Doc.h"
#include "SumatraWindow.h"
#include "WindowInfo.h"

struct EbookControls;
class  EbookController;

namespace mui { class HwndWrapper; }

class EbookWindow {
public:
    EbookWindow() : hwndFrame(NULL), menu(NULL), ebookControls(NULL),
        hwndWrapper(NULL), ebookController(NULL), isFullScreen(false),
        isMenuHidden(false)
    {
        touchState.panStarted = false;
    }
    ~EbookWindow() { }

    HWND                hwndFrame;
    HMENU               menu;
    bool                isMenuHidden;
    EbookControls *     ebookControls;
    mui::HwndWrapper *  hwndWrapper;
    EbookController *   ebookController;
    bool                isFullScreen;
    RectI               nonFullScreenFrameRect;
    long                nonFullScreenWindowStyle;
    TouchState          touchState;
    const WCHAR *       LoadedFilePath() const;
};

EbookWindow* FindEbookWindowByController(EbookController *controller);
WindowInfo *FindWindowInfoByController(EbookController *controller);
void   OpenEbookInWindow(Doc doc, SumatraWindow& winToReplace);
void   RegisterMobiWinClass(HINSTANCE hinst);
void   RebuildMenuBarForEbookWindows();
void   DeleteEbookWindow(EbookWindow *win, bool forceDelete = false);
bool   IsEbookFile(const WCHAR *fileName);
void   RestartLayoutTimer(EbookController *controller);
void   EbookWindowRefreshUI(EbookWindow *);

Doc    GetDocForWindow(const SumatraWindow& win);

#endif
