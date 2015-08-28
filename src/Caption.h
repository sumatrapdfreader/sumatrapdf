/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// factor by how large the non-maximized caption should be in relation to the tabbar
#define CAPTION_TABBAR_HEIGHT_FACTOR  1.25f

void CreateCaption(WindowInfo *win);
void RegisterCaptionWndClass();
LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool *callDef, WindowInfo *win);
void PaintParentBackground(HWND hwnd, HDC hdc);
void RelayoutCaption(WindowInfo *win);

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
    // form the inner rectangle where the button image is drawn
    RECT margins;

    ButtonInfo();

    void SetMargins(LONG left, LONG top, LONG right, LONG bottom);
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

    explicit CaptionInfo(HWND hwndCaption);
    ~CaptionInfo();

    void UpdateTheme();
    void UpdateColors(bool activeWindow);
    void UpdateBackgroundAlpha();
};
