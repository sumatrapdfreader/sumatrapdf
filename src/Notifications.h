/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Notifications_h
#define Notifications_h

#include "TextSearch.h"

#define MESSAGE_WND_CLASS_NAME _T("SUMATRA_PDF_MESSAGE_WINDOW")

class WindowInfo;
class MessageWnd;

enum NotificationGroup {
    NG_RESPONSE_TO_ACTION = 1,
    NG_FIND_PROGRESS,
    NG_PRINT_PROGRESS,
    NG_PAGE_INFO_HELPER,
    NG_STRESS_TEST_BENCHMARK,
    NG_STRESS_TEST_SUMMARY,
};

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
    bool  highlight;
    MessageWndCallback *callback;

    // only used for progress notifications
    bool isCanceled;
    int  progress;
    int  progressWidth;
    TCHAR *progressMsg; // must contain two %d (for current and total)

    void CreatePopup(HWND parent, const TCHAR *message);
    void UpdateWindowPosition(const TCHAR *message, bool init=false);

public:
    static const int TL_MARGIN = 8;
    int groupId; // for use by MessageWndList

    MessageWnd(HWND parent, const TCHAR *message, int timeoutInMS=0, bool highlight=false, MessageWndCallback *callback=NULL) :
        hasProgress(false), hasCancel(!timeoutInMS), callback(callback), highlight(highlight), progressMsg(NULL) {
        CreatePopup(parent, message);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }
    MessageWnd(HWND parent, const TCHAR *message, const TCHAR *progressMsg, MessageWndCallback *callback=NULL) :
        hasProgress(true), hasCancel(true), callback(callback), highlight(false), isCanceled(false), progress(0) {
        this->progressMsg = progressMsg ? str::Dup(progressMsg) : NULL;
        CreatePopup(parent, message);
    }
    ~MessageWnd() {
        DestroyWindow(self);
        DeleteObject(font);
        free(progressMsg);
    }

    HWND hwnd() { return self; }

    static RectI GetCancelRect(HWND hwnd) {
        return RectI(ClientRect(hwnd).dx - 16 - PADDING, PADDING, 16, 16);
    }

    void MessageUpdate(const TCHAR *message, int timeoutInMS=0, bool highlight=false);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // ProgressUpdateUI methods
    virtual bool ProgressUpdate(int current, int total);

};

class MessageWndList : public MessageWndCallback {
    Vec<MessageWnd *> wnds;

    int  GetWndX(MessageWnd *wnd);
    void MoveBelow(MessageWnd *fix, MessageWnd *move);
    void Remove(MessageWnd *wnd);

public:
    ~MessageWndList() { DeleteVecMembers(wnds); }

    bool Contains(MessageWnd *wnd) { return wnds.Find(wnd) != -1; }

    void         Add(MessageWnd *wnd, int groupId=0);
    MessageWnd * GetFirst(int groupId);
    void         CleanUp(int groupId);
    void         Relayout();

    // MessageWndCallback methods
    virtual void CleanUp(MessageWnd *wnd);
};

void ShowNotification(WindowInfo *win, const TCHAR *message, bool autoDismiss=true, bool highlight=false, NotificationGroup groupId=NG_RESPONSE_TO_ACTION);
bool RegisterNotificationsWndClass(HINSTANCE inst);
#endif

