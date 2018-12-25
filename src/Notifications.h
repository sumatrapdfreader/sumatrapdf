/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class NotificationWnd;

typedef std::function<void(NotificationWnd*)> NotificationWndRemovedCallback;

// this is a unique id for notification group that allows decoupling it
// from the rest of the code.
typedef const char* NotificationGroupId;

class NotificationWnd : public ProgressUpdateUI {
  public:
    HWND hwnd = nullptr;
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

    NotificationGroupId groupId = nullptr; // for use by Notifications

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    // Note: in most cases use WindowInfo::ShowNotification()
    NotificationWnd(HWND parent, const WCHAR* message, int timeoutInMS = 0, bool highlight = false,
                    const NotificationWndRemovedCallback& cb = nullptr);
    NotificationWnd(HWND parent, const WCHAR* message, const WCHAR* progressMsg,
                    const NotificationWndRemovedCallback& cb = nullptr);

    virtual ~NotificationWnd();

    void UpdateMessage(const WCHAR* message, int timeoutInMS = 0, bool highlight = false);

    // ProgressUpdateUI methods
    void UpdateProgress(int current, int total) override;
    bool WasCanceled() override;
};

class Notifications {
    std::vector<NotificationWnd*> wnds;

    int GetWndX(NotificationWnd* wnd);
    void MoveBelow(NotificationWnd* fix, NotificationWnd* move);
    void Remove(NotificationWnd* wnd);

  public:
    ~Notifications() { DeleteVecMembers(wnds); }

    bool Contains(NotificationWnd* wnd) const { return vectorContains(this->wnds, wnd); }

    // groupId is used to classify notifications and causes a notification
    // to replace any other notification of the same group
    void Add(NotificationWnd*, NotificationGroupId);
    NotificationWnd* GetForGroup(NotificationGroupId) const;
    void RemoveForGroup(NotificationGroupId);
    void Relayout();

    // NotificationWndCallback methods
    void RemoveNotification(NotificationWnd* wnd);
};
