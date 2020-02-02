/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SIZE MeasureTextInHwnd(HWND hwnd, const WCHAR* txt, HFONT font) {
    SIZE sz{};
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);

    RECT r{};
    UINT fmt = DT_CALCRECT | DT_LEFT | DT_NOCLIP | DT_EDITCONTROL;
    DrawTextExW(dc, (WCHAR*)txt, (int)txtLen, &r, fmt, nullptr);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    int dx = RectDx(r);
    int dy = RectDy(r);
    return {dx, dy};
}

// TODDO: add rest of messages:
// https://codeeval.dev/gist/9f8b444a5a181fbb6391d304b2dace52

struct WinMsgWithName {
    UINT msg;
    char* name;
};

#define dm(n) \
    { n, #n }

// https://social.msdn.microsoft.com/Forums/windowsapps/en-US/f677f319-9f02-4438-92fb-6e776924425d/windowproc-and-messages-0x90-0x91-0x92-0x93?forum=windowsuidevelopment
#define WM_UAHDESTROYWINDOW 0x0090
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
#define WM_UAHINITMENU 0x0093
#define WM_UAHMEASUREMENUITEM 0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

static WinMsgWithName gWinMessageNames[] = {
    dm(WM_NULL),
    dm(WM_CREATE),
    dm(WM_DESTROY),
    dm(WM_MOVE),
    dm(WM_SIZE),
    dm(WM_ACTIVATE),
    dm(WM_SETFOCUS),
    dm(WM_KILLFOCUS),
    dm(WM_ENABLE),
    dm(WM_SETREDRAW),
    dm(WM_SETTEXT),
    dm(WM_GETTEXT),
    dm(WM_GETTEXTLENGTH),
    dm(WM_PAINT),
    dm(WM_CLOSE),
    dm(WM_QUERYENDSESSION),
    dm(WM_QUERYOPEN),
    dm(WM_ENDSESSION),
    dm(WM_QUIT),
    dm(WM_ERASEBKGND),
    dm(WM_SYSCOLORCHANGE),
    dm(WM_SHOWWINDOW),
    dm(WM_WININICHANGE),
    dm(WM_SETTINGCHANGE),
    dm(WM_DEVMODECHANGE),
    dm(WM_ACTIVATEAPP),
    dm(WM_FONTCHANGE),
    dm(WM_TIMECHANGE),
    dm(WM_CANCELMODE),
    dm(WM_SETCURSOR),
    dm(WM_MOUSEACTIVATE),
    dm(WM_CHILDACTIVATE),
    dm(WM_QUEUESYNC),
    dm(WM_GETMINMAXINFO),
    dm(WM_PAINTICON),
    dm(WM_ICONERASEBKGND),
    dm(WM_NEXTDLGCTL),
    dm(WM_SPOOLERSTATUS),
    dm(WM_DRAWITEM),
    dm(WM_MEASUREITEM),
    dm(WM_DELETEITEM),
    dm(WM_VKEYTOITEM),
    dm(WM_CHARTOITEM),
    dm(WM_SETFONT),
    dm(WM_GETFONT),
    dm(WM_SETHOTKEY),
    dm(WM_GETHOTKEY),
    dm(WM_QUERYDRAGICON),
    dm(WM_COMPAREITEM),
    dm(WM_GETOBJECT),
    dm(WM_COMPACTING),
    dm(WM_COMMNOTIFY),
    dm(WM_WINDOWPOSCHANGING),
    dm(WM_WINDOWPOSCHANGED),
    dm(WM_POWER),
    dm(WM_COPYDATA),
    dm(WM_CANCELJOURNAL),
    dm(WM_NOTIFY),
    dm(WM_INPUTLANGCHANGEREQUEST),
    dm(WM_INPUTLANGCHANGE),
    dm(WM_TCARD),
    dm(WM_HELP),
    dm(WM_USERCHANGED),
    dm(WM_NOTIFYFORMAT),
    dm(WM_CONTEXTMENU),
    dm(WM_STYLECHANGING),
    dm(WM_STYLECHANGED),
    dm(WM_DISPLAYCHANGE),
    dm(WM_GETICON),
    dm(WM_SETICON),
    dm(WM_NCCREATE),
    dm(WM_NCDESTROY),
    dm(WM_NCCALCSIZE),
    dm(WM_NCHITTEST),
    dm(WM_NCPAINT),
    dm(WM_NCACTIVATE),
    dm(WM_GETDLGCODE),
    dm(WM_SYNCPAINT),
    dm(WM_NCMOUSEMOVE),
    dm(WM_NCLBUTTONDOWN),
    dm(WM_NCLBUTTONUP),
    dm(WM_NCLBUTTONDBLCLK),
    dm(WM_NCRBUTTONDOWN),
    dm(WM_NCRBUTTONUP),
    dm(WM_NCRBUTTONDBLCLK),
    dm(WM_NCMBUTTONDOWN),
    dm(WM_NCMBUTTONUP),
    dm(WM_NCMBUTTONDBLCLK),

    dm(WM_TOUCH),
    dm(WM_NCPOINTERUPDATE),
    dm(WM_NCPOINTERDOWN),
    dm(WM_NCPOINTERUP),
    dm(WM_POINTERUPDATE),
    dm(WM_POINTERDOWN),
    dm(WM_POINTERUP),
    dm(WM_POINTERENTER),
    dm(WM_POINTERLEAVE),
    dm(WM_POINTERACTIVATE),
    dm(WM_POINTERCAPTURECHANGED),
    dm(WM_TOUCHHITTESTING),
    dm(WM_POINTERWHEEL),
    dm(WM_POINTERHWHEEL),
    dm(DM_POINTERHITTEST),
    dm(WM_POINTERROUTEDTO),
    dm(WM_POINTERROUTEDAWAY),
    dm(WM_POINTERROUTEDRELEASED),

    dm(WM_IME_SETCONTEXT),
    dm(WM_IME_NOTIFY),
    dm(WM_IME_CONTROL),
    dm(WM_IME_COMPOSITIONFULL),
    dm(WM_IME_SELECT),
    dm(WM_IME_CHAR),
    dm(WM_IME_REQUEST),
    dm(WM_IME_KEYDOWN),
    dm(WM_IME_KEYUP),

    dm(WM_MOUSEHOVER),
    dm(WM_MOUSELEAVE),

    dm(WM_MENUSELECT),
    dm(WM_MENUCHAR),
    dm(WM_ENTERIDLE),
    dm(WM_MENURBUTTONUP),
    dm(WM_MENUDRAG),
    dm(WM_MENUGETOBJECT),
    dm(WM_UNINITMENUPOPUP),
    dm(WM_MENUCOMMAND),
    dm(WM_CHANGEUISTATE),
    dm(WM_UPDATEUISTATE),
    dm(WM_QUERYUISTATE),
    dm(WM_CTLCOLORMSGBOX),
    dm(WM_CTLCOLOREDIT),
    dm(WM_CTLCOLORLISTBOX),
    dm(WM_CTLCOLORBTN),
    dm(WM_CTLCOLORDLG),
    dm(WM_CTLCOLORSCROLLBAR),
    dm(WM_CTLCOLORSTATIC),
    dm(MN_GETHMENU),

    dm(WM_MOUSEMOVE),
    dm(WM_LBUTTONDOWN),
    dm(WM_LBUTTONUP),
    dm(WM_LBUTTONDBLCLK),
    dm(WM_RBUTTONDOWN),
    dm(WM_RBUTTONUP),
    dm(WM_RBUTTONDBLCLK),
    dm(WM_MBUTTONDOWN),
    dm(WM_MBUTTONUP),
    dm(WM_MBUTTONDBLCLK),
    dm(WM_MOUSEWHEEL),
    dm(WM_XBUTTONDOWN),
    dm(WM_XBUTTONUP),
    dm(WM_XBUTTONDBLCLK),

    dm(WM_NCXBUTTONDOWN),
    dm(WM_NCXBUTTONUP),
    dm(WM_NCXBUTTONDBLCLK),
    dm(WM_INPUT_DEVICE_CHANGE),
    dm(WM_INPUT),
    dm(WM_KEYFIRST),
    dm(WM_KEYDOWN),
    dm(WM_KEYUP),
    dm(WM_CHAR),
    dm(WM_DEADCHAR),
    dm(WM_SYSKEYDOWN),
    dm(WM_SYSKEYUP),
    dm(WM_SYSCHAR),
    dm(WM_SYSDEADCHAR),
    dm(WM_UNICHAR),
    dm(WM_KEYLAST),
    dm(WM_KEYLAST),
    dm(WM_IME_STARTCOMPOSITION),
    dm(WM_IME_ENDCOMPOSITION),
    dm(WM_IME_COMPOSITION),
    dm(WM_IME_KEYLAST),
    dm(WM_INITDIALOG),
    dm(WM_COMMAND),
    dm(WM_SYSCOMMAND),
    dm(WM_TIMER),
    dm(WM_HSCROLL),
    dm(WM_VSCROLL),
    dm(WM_INITMENU),
    dm(WM_INITMENUPOPUP),

    dm(WM_CUT),
    dm(WM_COPY),
    dm(WM_PASTE),
    dm(WM_CLEAR),
    dm(WM_UNDO),
    dm(WM_RENDERFORMAT),
    dm(WM_RENDERALLFORMATS),
    dm(WM_DESTROYCLIPBOARD),
    dm(WM_DRAWCLIPBOARD),
    dm(WM_PAINTCLIPBOARD),
    dm(WM_VSCROLLCLIPBOARD),
    dm(WM_SIZECLIPBOARD),
    dm(WM_ASKCBFORMATNAME),
    dm(WM_CHANGECBCHAIN),
    dm(WM_HSCROLLCLIPBOARD),
    dm(WM_QUERYNEWPALETTE),
    dm(WM_PALETTEISCHANGING),
    dm(WM_PALETTECHANGED),
    dm(WM_HOTKEY),

    dm(WM_UAHDESTROYWINDOW),
    dm(WM_UAHDRAWMENU),
    dm(WM_UAHDRAWMENUITEM),
    dm(WM_UAHINITMENU),
    dm(WM_UAHMEASUREMENUITEM),
    dm(WM_UAHNCPAINTMENUPOPUP),
};
#undef dm

char* getWinMessageName(UINT msg) {
    for (size_t i = 0; i < dimof(gWinMessageNames); i++) {
        if (gWinMessageNames[i].msg == msg) {
            return gWinMessageNames[i].name;
        }
    }
    return "msg unknown";
}
