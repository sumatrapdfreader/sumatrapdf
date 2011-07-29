/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmUI_h
#define ChmUI_h

#include "BaseUtil.h"
class ChmEngine;

class ChmWindowInfo {
public:
    ChmWindowInfo(HWND hwndFrame);
    ~ChmWindowInfo();

    TCHAR *     loadedFilePath;
    ChmEngine * chmEngine;

    HWND        hwndFrame;
    HWND        hwndCanvas;
    HWND        hwndToolbar;
    HWND        hwndReBar;
};

ChmWindowInfo *CreateChmWindowInfo();
bool RegisterChmWinClass(HINSTANCE hinst);

#endif
