/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;
struct WindowTab;
struct VirtCtrl;
struct VirtRichText;
struct ILayout;
struct PlatformFont;

extern Kind kNotifCursorPos;
extern Kind kNotifActionResponse;
extern Kind kNotifPageInfo;
extern Kind kNotifAdHoc;
extern Kind kNotifLazyLayout;

using NotificationWndRemoved = Func1<NotificationWnd*>;

constexpr const int kNotifDefaultTimeOut = 1000 * 3; // 3 seconds
constexpr const int kNotif5SecsTimeOut = 1000 * 5;
constexpr const int kNotifNoTimeout = 0;

// where on the canvas the notification is anchored. The corner variants stack
// multiple notifications toward the opposite edge. BottomBar is different: it
// spans the full canvas width along the bottom with centered text, for a status
// hint (e.g. the F7 keyboard-selection help).
// fixed underlying type so it can be forward-declared (e.g. in SumatraPDF.h)
enum class NotifCorner : int {
    TopLeft, // default; how notifications were always positioned
    TopRight,
    BottomLeft,
    BottomRight,
    BottomBar,
    Count, // number of positions; used to size per-position stacking state
};

// default distance (in unscaled px) from the canvas edges
constexpr const int kNotifDefaultMargin = 8;

struct NotificationCreateArgs {
    HWND hwndParent = nullptr;
    PlatformFont* font = nullptr;
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
    // if true, `msg` is shown verbatim: the tip markup ([text](CmdFoo),
    // **bold**, (Key/..), (Kbd/..)) is not parsed. Required for any message
    // that embeds text from outside the app (file paths, document metadata,
    // server responses) - see GHSA-2wv2-qm2f-vmxh
    bool plainText = false;
    // when set, the message is this pre-built rich text instead of `msg` parsed
    // as tip markup, for messages mixing app-authored markup with outside text.
    // `msg` is still the window text. ownership passes to the notification
    VirtRichText* richMsg = nullptr;
    // when set, the notification shows this VirtCtrl tree instead of `msg`.
    // ownership passes to the notification
    ILayout* content = nullptr;
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
void ShowNotificationsForActiveTab(HWND hwndCanvas, WindowTab* activeTab);
void RemoveNotificationsForTab(WindowTab* tab);

NotificationWnd* ShowNotification(const NotificationCreateArgs& args);
NotificationWnd* ShowTemporaryNotification(HWND hwnd, Str msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowCustomNotification(HWND hwndParent, ILayout* content, int timeoutMs = kNotifNoTimeout);
NotificationWnd* ShowWarningNotification(HWND hwndParent, Str msg, int timeoutMs);

// same, for a message that isn't fully app-authored: shown verbatim, no markup
NotificationWnd* ShowPlainNotification(HWND hwnd, Str msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowPlainWarningNotification(HWND hwndParent, Str msg, int timeoutMs);

void MaybeDelayedWarningNotification(Str msg);
void ShowMaybeDelayedNotifications(HWND hwndParent);

int CalcPerc(int current, int total);
