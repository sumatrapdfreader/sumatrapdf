/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Notifications_h
#define Notifications_h

#include "TextSearch.h"

class WindowInfo;
class NotificationWnd;

enum NotificationGroup {
    NG_RESPONSE_TO_ACTION = 1,
    NG_FIND_PROGRESS,
    NG_PRINT_PROGRESS,
    NG_PAGE_INFO_HELPER,
    NG_STRESS_TEST_BENCHMARK,
    NG_STRESS_TEST_SUMMARY,
};

class NotificationWndCallback {
public:
    // called after a message has timed out or been canceled
    virtual void RemoveNotification(NotificationWnd *wnd) = 0;
};

class NotificationWnd : public ProgressUpdateUI {
    static const int TIMEOUT_TIMER_ID = 1;
    static const int PROGRESS_WIDTH = 188;
    static const int PROGRESS_HEIGHT = 5;
    static const int PADDING = 6;

    HWND self;
    bool hasProgress;
    bool hasCancel;

    HFONT font;
    bool  highlight;
    NotificationWndCallback *notificationCb;

    // only used for progress notifications
    bool isCanceled;
    int  progress;
    int  progressWidth;
    TCHAR *progressMsg; // must contain two %d (for current and total)

    void CreatePopup(HWND parent, const TCHAR *message);
    void UpdateWindowPosition(const TCHAR *message, bool init=false);

public:
    static const int TL_MARGIN = 8;
    int groupId; // for use by Notifications

    // Note: in most cases use ShowNotification() and not assemble them manually
    NotificationWnd(HWND parent, const TCHAR *message, int timeoutInMS=0, bool highlight=false, NotificationWndCallback *cb=NULL) :
        hasProgress(false), hasCancel(!timeoutInMS), notificationCb(cb), highlight(highlight), progressMsg(NULL) {
        CreatePopup(parent, message);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, NULL);
    }
    NotificationWnd(HWND parent, const TCHAR *message, const TCHAR *progressMsg, NotificationWndCallback *cb=NULL) :
        hasProgress(true), hasCancel(true), notificationCb(cb), highlight(false), isCanceled(false), progress(0) {
        this->progressMsg = progressMsg ? str::Dup(progressMsg) : NULL;
        CreatePopup(parent, message);
    }
    ~NotificationWnd() {
        DestroyWindow(self);
        DeleteObject(font);
        free(progressMsg);
    }

    HWND hwnd() { return self; }

    static RectI GetCancelRect(HWND hwnd) {
        return RectI(ClientRect(hwnd).dx - 16 - PADDING, PADDING, 16, 16);
    }

    void UpdateMessage(const TCHAR *message, int timeoutInMS=0, bool highlight=false);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // ProgressUpdateUI methods
    virtual bool UpdateProgress(int current, int total);

};

class Notifications : public NotificationWndCallback {
    Vec<NotificationWnd *> wnds;

    int  GetWndX(NotificationWnd *wnd);
    void MoveBelow(NotificationWnd *fix, NotificationWnd *move);
    void Remove(NotificationWnd *wnd);

public:
    ~Notifications() { DeleteVecMembers(wnds); }

    bool Contains(NotificationWnd *wnd) { return wnds.Find(wnd) != -1; }

    void         Add(NotificationWnd *wnd, int groupId=0);
    NotificationWnd * GetFirstInGroup(int groupId);
    void         RemoveAllInGroup(int groupId);
    void         Relayout();

    // NotificationWndCallback methods
    virtual void RemoveNotification(NotificationWnd *wnd);
};

void ShowNotification(WindowInfo *win, const TCHAR *message, bool autoDismiss=true, bool highlight=false, NotificationGroup groupId=NG_RESPONSE_TO_ACTION);
bool RegisterNotificationsWndClass(HINSTANCE inst);

#endif
