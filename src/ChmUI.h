/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// TODO: rename to HtmlUI or similar, as the same code should
//       be reusable for other HTML based formats such as EPUB

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
    HWND        hwndHtml;
    HWND        hwndToolbar;
    HWND        hwndReBar;

    HMENU       menu;
};

ChmWindowInfo *CreateChmWindowInfo();
bool RegisterChmWinClass(HINSTANCE hinst);

#endif
