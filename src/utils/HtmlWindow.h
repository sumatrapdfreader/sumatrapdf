/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */
#ifndef HtmlWindow_h
#define HtmlWindow_h

#include "BaseUtil.h"
#include <exdisp.h>

class FrameSite;

class HtmlWindow
{
protected:
    friend class FrameSite;

    HWND hwnd;

    IWebBrowser2 *      webBrowser;
    IOleObject *        oleObject;
    IOleInPlaceObject * oleInPlaceObject;
    IViewObject *       viewObject;
    IConnectionPoint *  connectionPoint;
    HWND                oleObjectHwnd;

    DWORD               adviseCookie;

    void CreateBrowser();
public:
    HtmlWindow(HWND hwnd);
    ~HtmlWindow();
};
#endif
