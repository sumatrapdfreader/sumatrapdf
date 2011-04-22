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
    // called after a message has timed out or been canceled
    virtual void CleanUp(MessageWnd *wnd) = 0;
};

class MessageWnd : public ProgressUpdateUI {
    static const int TIMEOUT_TIMER_ID = 1;
    static const int PROGRESS_WIDTH = 188;
    static const int PROGRESS_HEIGHT = 5;
    static const int PADDING = 6;

    HWND self;
    bool hasProgress;
    bool hasCancel;

    HFONT font;
    bool highlight;
    MessageWndCallback *callback;

    // only used for progress notifications
    bool isCanceled;
    int progress;
    int progressWidth;
    TCHAR *progressMsg; // must contain two %d (for current and total)

    void CreatePopup(HWND parent, const TCHAR *message) {
        NONCLIENTMETRICS ncm = { 0 };
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        font = CreateFontIndirect(&ncm.lfMessageFont);

        HDC hdc = GetDC(parent);
        progressWidth = MulDiv(PROGRESS_WIDTH, GetDeviceCaps(hdc, LOGPIXELSX), USER_DEFAULT_SCREEN_DPI);
        ReleaseDC(parent, hdc);

        self = CreateWindowEx(WS_EX_TOPMOST, MESSAGE_WND_CLASS_NAME, message, WS_CHILD | SS_CENTER,
                              TL_MARGIN, TL_MARGIN, 0, 0,
                              parent, (HMENU)0, ghinst, NULL);
        SetWindowLongPtr(self, GWLP_USERDATA, (LONG_PTR)this);
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
        if (hasCancel) {
            rectMsg.dy = max(rectMsg.dy, 16);
            rectMsg.dx += 20;
        }
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
    static const int TL_MARGIN = 8;

    MessageWnd(HWND parent, const TCHAR *message, int timeoutInMS=0, bool highlight=false, MessageWndCallback *callback=NULL) :
        hasProgress(false), hasCancel(!timeoutInMS), callback(callback), highlight(highlight), progressMsg(NULL) {
        CreatePopup(parent, message);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }
    MessageWnd(HWND parent, const TCHAR *message, const TCHAR *progressMsg, MessageWndCallback *callback=NULL) :
        hasProgress(true), hasCancel(true), callback(callback), highlight(false), isCanceled(false), progress(0) {
        this->progressMsg = progressMsg ? Str::Dup(progressMsg) : NULL;
        CreatePopup(parent, message);
    }
    ~MessageWnd() {
        DestroyWindow(self);
        DeleteObject(font);
        free(progressMsg);
    }

    HWND hwnd() { return self; }

    virtual bool ProgressUpdate(int current, int total) {
        progress = limitValue(100 * current / total, 0, 100);
        if (hasProgress && progressMsg) {
            ScopedMem<TCHAR> message(Str::Format(progressMsg, current, total));
            MessageUpdate(message);
        }
        return isCanceled;
    }

    void MessageUpdate(const TCHAR *message, int timeoutInMS=0, bool highlight=false) {
        Win::SetText(self, message);
        this->highlight = highlight;
        if (timeoutInMS)
            hasCancel = false;
        UpdateWindowPosition(message);
        InvalidateRect(self, NULL, TRUE);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }

    static RectI GetCancelRect(HWND hwnd) {
        return RectI(ClientRect(hwnd).dx - 16 - PADDING, PADDING, 16, 16);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        MessageWnd *wnd = (MessageWnd *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (WM_ERASEBKGND == message) {
            // do nothing, helps to avoid flicker
            return TRUE;
        }
        if (WM_TIMER == message && TIMEOUT_TIMER_ID == wParam) {
            if (wnd->callback)
                wnd->callback->CleanUp(wnd);
            else
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
            if (wnd->hasCancel)
                rectMsg.dx -= 20;
            ScopedMem<TCHAR> text(Win::GetText(hwnd));
            DrawText(hdc, text, -1, &rectMsg.ToRECT(), DT_SINGLELINE);

            if (wnd->hasCancel)
                DrawFrameControl(hdc, &GetCancelRect(hwnd).ToRECT(), DFC_CAPTION, DFCS_CAPTIONCLOSE | DFCS_FLAT);

            if (wnd->hasProgress) {
                rect.dx = wnd->progressWidth;
                rect.y += rectMsg.dy + PADDING / 2;
                rect.dy = PROGRESS_HEIGHT;
                PaintRect(hdc, rect);

                rect.x += 2;
                rect.dx = (wnd->progressWidth - 3) * wnd->progress / 100;
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
        if (WM_SETCURSOR == message && wnd->hasCancel) {
            POINT pt;
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt) &&
                GetCancelRect(hwnd).Inside(PointI(pt.x, pt.y))) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
        }
        if (WM_LBUTTONUP == message && wnd->hasCancel) {
            if (GetCancelRect(hwnd).Inside(PointI(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))) {
                if (wnd->callback)
                    wnd->callback->CleanUp(wnd);
                else
                    delete wnd;
                return 0;
            }
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
};

class MessageWndList : public MessageWndCallback {
    Vec<MessageWnd *> wnds;

    void MoveBelow(MessageWnd *fix, MessageWnd *move) {
        RectI rect = WindowRect(fix->hwnd());
        rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(fix->hwnd()));
        SetWindowPos(move->hwnd(), NULL,
                     rect.x, rect.y + rect.dy + MessageWnd::TL_MARGIN,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

public:
    ~MessageWndList() { DeleteVecMembers(wnds); }

    bool Contains(MessageWnd *wnd) { return wnds.Find(wnd) != -1; }

    void Add(MessageWnd *wnd) {
        if (wnds.Count() > 0)
            MoveBelow(wnds[wnds.Count() - 1], wnd);
        wnds.Append(wnd);
    }

    void Remove(MessageWnd *wnd) {
        int ix = wnds.Find(wnd);
        if (ix == -1)
            return;
        wnds.Remove(wnd);
        if (ix == 0 && wnds.Count() > 0) {
            SetWindowPos(wnds[0]->hwnd(), NULL,
                         MessageWnd::TL_MARGIN, MessageWnd::TL_MARGIN,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
            ix = 1;
        }
        for (; ix < (int)wnds.Count(); ix++)
            MoveBelow(wnds[ix - 1], wnds[ix]);
    }

    virtual void CleanUp(MessageWnd *wnd) {
        if (Contains(wnd)) {
            Remove(wnd);
            delete wnd;
        }
    }
};

class MessageWndHolder : public MessageWndCallback {
    MessageWnd *wnd;
    MessageWndList *list;

public:
    MessageWndHolder(MessageWndList *list) : list(list), wnd(NULL) { }
    ~MessageWndHolder() {
        if (wnd)
            list->CleanUp(wnd);
    }

    void SetUp(MessageWnd *wnd) {
        if (this->wnd)
            list->CleanUp(this->wnd);
        this->wnd = wnd;
        list->Add(wnd);
    }

    virtual void CleanUp(MessageWnd *wnd) {
        this->wnd = NULL;
        list->CleanUp(wnd);
    }

    MessageWnd *GetWnd() const { return wnd; }
};
