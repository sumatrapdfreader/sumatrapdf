/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;
struct WindowTab;

extern Kind kNotifCursorPos;
extern Kind kNotifActionResponse;
extern Kind kNotifPageInfo;
extern Kind kNotifAdHoc;

using NotificationWndRemoved = Func1<NotificationWnd*>;

constexpr const int kNotifDefaultTimeOut = 1000 * 3; // 3 seconds
constexpr const int kNotif5SecsTimeOut = 1000 * 5;
constexpr const int kNotifNoTimeout = 0;

// which corner of the canvas the notification is anchored to (multiple
// notifications in the same corner stack toward the opposite edge)
enum class NotifCorner {
    TopLeft, // default; how notifications were always positioned
    TopRight,
    BottomLeft,
    BottomRight,
};

// default distance (in unscaled px) from the canvas edges
constexpr const int kNotifDefaultMargin = 8;

struct NotificationCreateArgs {
    HWND hwndParent = nullptr;
    HFONT font = nullptr;
    Kind groupId = kNotifActionResponse;
    bool warning = false;
    bool noClose = false; // if true, no close button; must have timeoutMs > 0
    int timeoutMs = 0;    // if 0 => persists until closed manually
    int delayInMs = 0;    // if > 0 => create hidden, show after delay
    float shrinkLimit = 1.0f;
    NotifCorner corner = NotifCorner::TopLeft;
    int xMargin = kNotifDefaultMargin; // distance from the left/right edge
    int yMargin = kNotifDefaultMargin; // distance from the top/bottom edge
    Str msg;
    // if set, the notification is only shown while this tab is the active tab
    // (hidden when switching to another tab in the same window)
    WindowTab* tab = nullptr;
    NotificationWndRemoved onRemoved;
};

void NotificationUpdateMessage(NotificationWnd* wnd, Str msg, int timeoutInMS = 0, bool highlight = false);
TempStr NotificationGetMessageTemp(NotificationWnd* wnd);
void RemoveNotification(NotificationWnd*);
bool RemoveNotificationsForGroup(HWND, Kind);
void RemoveNotificationsForHwnd(HWND);
NotificationWnd* GetNotificationForGroup(HWND, Kind);
bool UpdateNotificationProgress(NotificationWnd*, Str msg, int perc);
void RelayoutNotifications(HWND hwnd);
// show notifications tied to activeTab (and untied ones), hide those tied to
// other tabs; call when the active tab changes
void ShowNotificationsForActiveTab(HWND hwndCanvas, WindowTab* activeTab);
// remove notifications tied to a tab (call when the tab is closed)
void RemoveNotificationsForTab(WindowTab* tab);

NotificationWnd* ShowNotification(const NotificationCreateArgs& args);
NotificationWnd* ShowTemporaryNotification(HWND hwnd, Str msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowWarningNotification(HWND hwndParent, Str msg, int timeoutMs);

void MaybeDelayedWarningNotification(Str msg);
void ShowMaybeDelayedNotifications(HWND hwndParent);

int CalcPerc(int current, int total);
