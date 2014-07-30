/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Caption_h
#define Caption_h

#include <dwmapi.h>
#include <vssym32.h>

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A
#endif
#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif

// factor by how large the non-maximized caption should be in relation to the tabbar
#define CAPTION_TABBAR_HEIGHT_FACTOR  1.25f

class WindowInfo;

void CreateCaption(WindowInfo *win);
void RegisterCaptionWndClass();
LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool *callDef, WindowInfo *win);
void PaintParentBackground(HWND hwnd, HDC hdc);
void RelayoutCaption(WindowInfo *win);

namespace vss {

void Initialize();
HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
HRESULT CloseThemeData(HTHEME hTheme);
HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
BOOL IsThemeActive();
BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId);
HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);

};

namespace dwm {

void Initialize();
BOOL IsCompositionEnabled();
HRESULT ExtendFrameIntoClientArea(HWND hwnd, const MARGINS *pMarInset);
BOOL DefWindowProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
HRESULT GetWindowAttribute(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute);

};

enum CaptionButtons {
    CB_BTN_FIRST = 0,
    CB_MINIMIZE = CB_BTN_FIRST,
    CB_MAXIMIZE,
    CB_RESTORE,
    CB_CLOSE,
    CB_MENU,
    CB_BTN_COUNT
};

struct ButtonInfo
{
    HWND hwnd;
    bool highlighted;
    bool inactive;

    ButtonInfo(): hwnd(NULL), highlighted(false), inactive(false) { }
};

class CaptionInfo
{
    HWND hwnd;

public:
    ButtonInfo  btn[CB_BTN_COUNT];
    HTHEME      theme;
    COLORREF    bgColor;
    COLORREF    textColor;
    BYTE        bgAlpha;
    bool        isMenuOpen;

    CaptionInfo(HWND hwndCaption): hwnd(hwndCaption), theme(NULL), isMenuOpen(false) {
        UpdateTheme();
        UpdateColors(true);
        UpdateBackgroundAlpha();
    }

    ~CaptionInfo() {
        if (theme)
            vss::CloseThemeData(theme);
    }

    void UpdateTheme()
    {
        if (theme) {
            vss::CloseThemeData(theme);
            theme = NULL;
        }
        if (vss::IsThemeActive())
            theme = vss::OpenThemeData(hwnd, L"WINDOW");
    }

    void UpdateColors(bool activeWindow)
    {
        if (!theme || !SUCCEEDED(vss::GetThemeColor(theme, WP_CAPTION, 0,
            activeWindow ? TMT_FILLCOLORHINT : TMT_BORDERCOLORHINT, &bgColor))) {
                bgColor = activeWindow ? GetSysColor(COLOR_GRADIENTACTIVECAPTION)
                                       : GetSysColor(COLOR_GRADIENTINACTIVECAPTION);
        }
        if (!theme || !SUCCEEDED(vss::GetThemeColor(theme, WP_CAPTION, 0,
            activeWindow ? TMT_CAPTIONTEXT : TMT_INACTIVECAPTIONTEXT, &textColor))) {
                textColor = activeWindow ? GetSysColor(COLOR_CAPTIONTEXT)
                                         : GetSysColor(COLOR_INACTIVECAPTIONTEXT);
        }
    }

    void UpdateBackgroundAlpha()
    {
        bgAlpha = dwm::IsCompositionEnabled() ? 0 : 255;
    }
};

#endif
