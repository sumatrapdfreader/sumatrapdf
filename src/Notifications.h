/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "GeomUtil.h"
#include "WinUtil.h"
#include "Vec.h"

#define MESSAGE_WND_CLASS_NAME _T("SUMATRA_PDF_MESSAGE_WINDOW")

class MessageWnd;

class MessageWndCallback {
public:
    // called right before a MessageWnd is deleted
    virtual void CleanUp(MessageWnd *wnd) = 0;
};

class MessageWnd : public ProgressUpdateUI {
    static const int TIMEOUT_TIMER_ID = 1;
    static const int PROGRESS_WIDTH = 200;
    static const int PROGRESS_HEIGHT = 5;
    static const int TL_MARGIN = 8;
    static const int PADDING = 6;

    HWND self;
    bool hasProgress;
    bool highlight;
    HFONT font;
    MessageWndCallback *callback;
    // only used for progress notifications
    bool isCanceled;
    int progress;
    int progressWidth;
    TCHAR *progressMsg;

    void CreatePopup(HWND parent, const TCHAR *message, int dpi) {
        NONCLIENTMETRICS ncm = { 0 };
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        font = CreateFontIndirect(&ncm.lfMessageFont);
        progressWidth = MulDiv(PROGRESS_WIDTH, dpi, USER_DEFAULT_SCREEN_DPI);

        self = CreateWindowEx(WS_EX_TOPMOST, MESSAGE_WND_CLASS_NAME, message, WS_CHILD | SS_CENTER,
                              TL_MARGIN, TL_MARGIN, 0, 0,
                              parent, (HMENU)0, ghinst, NULL);
        SetWindowLongPtr(self, GWLP_USERDATA, (LONG_PTR)this);
        // TODO: add a Cancel button, if hasProgress
        UpdateWindowPosition(message, true);
        ShowWindow(self, SW_SHOW);
    }

    void UpdateWindowPosition(const TCHAR *message, bool init=false) {
        // compute the length of the message
        RECT rc = ClientRect(self).ToRECT();
        HDC hdc = GetDC(self);
        HFONT oldfnt = SelectFont(hdc, font);
        DrawText(hdc, message, -1, &rc, DT_CALCRECT | DT_SINGLELINE);
        SelectFont(hdc, oldfnt);
        ReleaseDC(self, hdc);

        RectI rectMsg = RectI::FromRECT(rc);
        rectMsg.Inflate(PADDING, PADDING);

        // adjust the window to fit the message (only shrink the window when there's no progress bar)
        if (!hasProgress) {
            SetWindowPos(self, NULL, 0, 0, rectMsg.dx, rectMsg.dy, SWP_NOMOVE | SWP_NOZORDER);
        } else if (init) {
            RectI rect = WindowRect(self);
            rect.dx = max(progressWidth + 2 * PADDING, rectMsg.dx);
            rect.dy = rectMsg.dy + PROGRESS_HEIGHT + PADDING / 2;
            SetWindowPos(self, NULL, 0, 0, rect.dx, rect.dy, SWP_NOMOVE | SWP_NOZORDER);
        } else if (rectMsg.dx > progressWidth + 2 * PADDING) {
            SetWindowPos(self, NULL, 0, 0, rectMsg.dx, WindowRect(self).dy, SWP_NOMOVE | SWP_NOZORDER);
        }
    }

public:
    MessageWnd(WindowInfo *win, const TCHAR *message, int timeoutInMS=0, bool highlight=false, MessageWndCallback *callback=NULL) :
        hasProgress(false), callback(callback), highlight(highlight), progressMsg(NULL) {
        CreatePopup(win->hwndCanvas, message, win->dpi);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }
    MessageWnd(WindowInfo *win, const TCHAR *message, const TCHAR *progressMsg, MessageWndCallback *callback=NULL) :
        hasProgress(true), callback(callback), highlight(false), isCanceled(false), progress(0) {
        this->progressMsg = progressMsg ? Str::Dup(progressMsg) : NULL;
        CreatePopup(win->hwndCanvas, message, win->dpi);
    }
    ~MessageWnd() {
        if (callback)
            callback->CleanUp(this);
        DestroyWindow(self);
        DeleteObject(font);
        free(progressMsg);
    }

    virtual bool ProgressUpdate(int count, int total) {
        progress = limitValue(100 * count / total, 0, 100);
        if (hasProgress && progressMsg) {
            ScopedMem<TCHAR> message(Str::Format(progressMsg, count, total));
            MessageUpdate(message);
        }
        return isCanceled;
    }

    void MessageUpdate(const TCHAR *message, int timeoutInMS=0, bool highlight=false) {
        Win::SetText(self, message);
        UpdateWindowPosition(message);
        this->highlight = highlight;
        InvalidateRect(self, NULL, TRUE);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        MessageWnd *wnd = (MessageWnd *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (WM_ERASEBKGND == message) {
            // do nothing, helps to avoid flicker
            return TRUE;
        }
        if (WM_TIMER == message && TIMEOUT_TIMER_ID == wParam) {
            delete wnd;
            return 0;
        }
        if (WM_PAINT == message && wnd) {
            PAINTSTRUCT ps;
            HDC hdcWnd = BeginPaint(hwnd, &ps);

            ClientRect rect(hwnd);
            DoubleBuffer buffer(hwnd, rect);
            HDC hdc = buffer.GetDC();
            HFONT oldfnt = SelectFont(hdc, wnd->font);

            DrawFrameControl(hdc, &rect.ToRECT(), DFC_BUTTON, DFCS_BUTTONPUSH);
            if (wnd->highlight) {
                SetBkMode(hdc, OPAQUE);
                SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
            }
            else {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            }

            rect.Inflate(-PADDING, -PADDING);
            RectI rectMsg = rect;
            if (wnd->hasProgress)
                rectMsg.dy -= PROGRESS_HEIGHT + PADDING / 2;

            ScopedMem<TCHAR> text(Win::GetText(hwnd));
            DrawText(hdc, text, -1, &rectMsg.ToRECT(), DT_SINGLELINE);

            if (wnd->hasProgress) {
                rect.dx = wnd->progressWidth;
                rect.y += rectMsg.dy + PADDING / 2;
                rect.dy = PROGRESS_HEIGHT;
                PaintRect(hdc, rect);

                rect.x += 2;
                rect.dx = wnd->progressWidth * wnd->progress / 100 - 3;
                rect.y += 2;
                rect.dy -= 3;

                HBRUSH brush = CreateSolidBrush(WIN_COL_BLACK);
                FillRect(hdc, &rect.ToRECT(), brush);
                DeleteObject(brush);
            }

            SelectFont(hdc, oldfnt);

            buffer.Flush(hdcWnd);
            EndPaint(hwnd, &ps);
            return WM_PAINT_HANDLED;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
};

// TODO: display several windows beneath each other
class MessageWndList : public MessageWndCallback {
    Vec<MessageWnd *> wnds;

public:
    ~MessageWndList() { DeleteVecMembers(wnds); }

    void Add(MessageWnd *wnd) { wnds.Append(wnd); }
    bool Contains(MessageWnd *wnd) { return wnds.Find(wnd) != -1; }
    void Remove(MessageWnd *wnd) { wnds.Remove(wnd); }
    bool IsEmpty() { return wnds.Count() == 0; }

    virtual void CleanUp(MessageWnd *wnd) { Remove(wnd); }
};
