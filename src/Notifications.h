/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;

extern Kind kNotifGroupCursorPos;
extern Kind kNotifGroupActionResponse;

using NotificationWndRemovedCallback = std::function<void(NotificationWnd*)>;

// 3 seconds
constexpr const int kNotifDefaultTimeOut = 1000 * 3;

struct NotificationCreateArgs {
    HWND hwndParent = nullptr;
    Kind groupId = kNotifGroupActionResponse;
    bool warning = false;
    int timeoutMs = 0; // if 0 => persists until closed manually
    const char* msg = nullptr;
    const char* progressMsg = nullptr;
    NotificationWndRemovedCallback onRemoved;
};

struct NotificationWnd : public ProgressUpdateUI {
    HWND parent = nullptr;
    HWND hwnd = nullptr;
    int timeoutInMS = kNotifDefaultTimeOut; // 0 means no timeout
    bool hasProgress = false;
    bool hasClose = false;

    HFONT font = nullptr;
    bool highlight = false;
    NotificationWndRemovedCallback wndRemovedCb = nullptr;

    // only used for progress notifications
    bool isCanceled = false;
    int progress = 0;
    int progressWidth = 0;
    char* progressMsg = nullptr; // must contain two %d (for current and total)

    bool Create(const char* msg, const char* progressMsg);

    Kind groupId = nullptr; // for use by Notifications

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    // Note: in most cases use WindowInfo::ShowNotification()
    explicit NotificationWnd(HWND parent, int timeoutInMS);

    ~NotificationWnd() override;

    void UpdateMessage(const char* msg, int timeoutInMS = 0, bool highlight = false);

    // ProgressUpdateUI methods
    void UpdateProgress(int current, int total) override;
    bool WasCanceled() override;
};

NotificationWnd* ShowNotification(NotificationCreateArgs& args);
void NotificationUpdateMessage(NotificationWnd* wnd, const char* msg, int timeoutInMS = 0, bool highlight = false);
void RemoveNotification(NotificationWnd*);
void RemoveNotificationsForGroup(HWND hwnd, Kind);
NotificationWnd* GetNotificationForGroup(HWND hwnd, Kind);
bool UpdateNotificationProgress(NotificationWnd*, int, int);
bool NotificationExists(NotificationWnd*);
void RelayoutNotifications(HWND hwnd);
