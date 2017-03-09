/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class NotificationWnd;

typedef std::function<void(NotificationWnd*)> NotificationWndRemovedCallback;

class NotificationWnd : public ProgressUpdateUI {
  public:
    static const int TIMEOUT_TIMER_ID = 1;

    HWND self = nullptr;
    bool hasProgress = false;
    bool hasCancel = false;

    HFONT font = nullptr;
    bool highlight = false;
    NotificationWndRemovedCallback wndRemovedCb;

    // only used for progress notifications
    bool isCanceled = false;
    int progress = 0;
    int progressWidth = 0;
    WCHAR* progressMsg = nullptr; // must contain two %d (for current and total)

    void CreatePopup(HWND parent, const WCHAR* message);
    void UpdateWindowPosition(const WCHAR* message, bool init = false);

    int groupId = 0; // for use by Notifications

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    // Note: in most cases use WindowInfo::ShowNotification()
    NotificationWnd(HWND parent, const WCHAR* message, int timeoutInMS = 0, bool highlight = false,
                    NotificationWndRemovedCallback cb = nullptr) {
        hasCancel = (0 == timeoutInMS);
        wndRemovedCb = cb;
        this->highlight = highlight;
        CreatePopup(parent, message);
        if (timeoutInMS)
            SetTimer(self, TIMEOUT_TIMER_ID, timeoutInMS, nullptr);
    }

    NotificationWnd(HWND parent, const WCHAR* message, const WCHAR* progressMsg,
                    NotificationWndRemovedCallback cb = nullptr) {
        hasProgress = true;
        hasCancel = true;
        wndRemovedCb = cb;
        this->progressMsg = str::Dup(progressMsg);
        CreatePopup(parent, message);
    }

    ~NotificationWnd() {
        DestroyWindow(self);
        DeleteObject(font);
        free(progressMsg);
    }

    HWND hwnd() { return self; }

    void UpdateMessage(const WCHAR* message, int timeoutInMS = 0, bool highlight = false);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // ProgressUpdateUI methods
    virtual void UpdateProgress(int current, int total);
    virtual bool WasCanceled();
};

class Notifications {
    Vec<NotificationWnd*> wnds;

    int GetWndX(NotificationWnd* wnd);
    void MoveBelow(NotificationWnd* fix, NotificationWnd* move);
    void Remove(NotificationWnd* wnd);

  public:
    ~Notifications() { DeleteVecMembers(wnds); }

    bool Contains(NotificationWnd* wnd) { return wnds.Contains(wnd); }

    // groupId is used to classify notifications and causes a notification
    // to replace any other notification of the same group
    void Add(NotificationWnd* wnd, int groupId = 0);
    NotificationWnd* GetForGroup(int groupId);
    void RemoveForGroup(int groupId);
    void Relayout();

    // NotificationWndCallback methods
    void RemoveNotification(NotificationWnd* wnd);
};

void RegisterNotificationsWndClass();
