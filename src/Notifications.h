/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;

extern Kind NG_CURSOR_POS_HELPER;
extern Kind NG_RESPONSE_TO_ACTION;

using NotificationWndRemovedCallback = std::function<void(NotificationWnd*)>;

enum class NotificationOptions {
    WithTimeout = 0, // timeout after 3 seconds, no highlight
    Persist = (1 << 0),
    Highlight = (1 << 1),
    Warning = Persist | Highlight,
};

struct NotificationWnd : public ProgressUpdateUI {
    HWND parent = nullptr;
    HWND hwnd = nullptr;
    int timeoutInMS = 0; // 0 means no timeout
    bool hasProgress = false;
    bool hasClose = false;

    HFONT font = nullptr;
    bool highlight = false;
    NotificationWndRemovedCallback wndRemovedCb = nullptr;

    // only used for progress notifications
    bool isCanceled = false;
    int progress = 0;
    int progressWidth = 0;
    WCHAR* progressMsg = nullptr; // must contain two %d (for current and total)

    bool Create(const WCHAR* msg, const WCHAR* progressMsg);

    Kind groupId = nullptr; // for use by Notifications

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    // Note: in most cases use WindowInfo::ShowNotification()
    explicit NotificationWnd(HWND parent, int timeoutInMS);

    ~NotificationWnd() override;

    void UpdateMessage(const WCHAR* message, int timeoutInMS = 0, bool highlight = false);

    // ProgressUpdateUI methods
    void UpdateProgress(int current, int total) override;
    bool WasCanceled() override;
};

struct Notifications {
    Vec<NotificationWnd*> wnds;

    void MoveBelow(NotificationWnd* fix, NotificationWnd* move);
    void Remove(NotificationWnd* wnd);

    ~Notifications();
    bool Contains(NotificationWnd* wnd) const;

    NotificationWnd* Show(HWND hwnd, const WCHAR* msg, NotificationOptions opts = NotificationOptions::WithTimeout,
                          Kind groupId = NG_RESPONSE_TO_ACTION);
    NotificationWnd* Show(HWND hwnd, std::string_view, NotificationOptions opts = NotificationOptions::WithTimeout,
                          Kind groupId = NG_RESPONSE_TO_ACTION);

    // groupId is used to classify notifications and causes a notification
    // to replace any other notification of the same group
    void Add(NotificationWnd*, Kind);
    NotificationWnd* GetForGroup(Kind) const;
    void RemoveForGroup(Kind);
    void Relayout();

    // NotificationWndCallback methods
    void RemoveNotification(NotificationWnd* wnd);
};
