/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct TipSpan;

extern Kind kNotifCursorPos;
extern Kind kNotifActionResponse;
extern Kind kNotifPageInfo;
extern Kind kNotifAdHoc;
extern Kind kNotifLazyLayout;
extern Kind kNotifChapterLayout;
extern Kind kNotifZoomOrView;

constexpr const int kNotifDefaultTimeOut = 1000 * 3; // 3 seconds
constexpr const int kNotif5SecsTimeOut = 1000 * 5;
constexpr const int kNotifNoTimeout = 0;

// default distance (in unscaled px) from the canvas edges
constexpr const int kNotifDefaultMargin = 8;

enum class NotifCloseReason {
    User,    // close button
    Timeout, // timeoutMs elapsed
    Program, // CloseNotification()
};

struct NotificationWnd;

struct NotificationClosedEvent {
    NotificationWnd* wnd = nullptr;
    NotifCloseReason reason = NotifCloseReason::Program;
};

using NotificationClosed = Func1<NotificationClosedEvent*>;

// where on the canvas the notification is anchored. The corner variants stack
// multiple notifications toward the opposite edge. BottomBar spans the full
// canvas width along the bottom with centered text.
enum class NotifCorner : int {
    TopLeft, // default; how notifications were always positioned
    TopRight,
    BottomLeft,
    BottomRight,
    BottomBar,
    Count,
};

// ng: orig's NotificationWnd is a win32 window with owner-drawn rich text. Here
// it is the model of one notification; the gpui shell draws it in the canvas
// corner (src/gui/NotificationsUI.cpp).
struct NotificationWnd {
    MainWindow* win = nullptr;
    // if set, the notification only shows while this tab is the active one
    WindowTab* tab = nullptr;
    Kind groupId = nullptr;
    Str msg;
    // the parsed tip markup of msg, or what the caller built; null means msg
    // is drawn verbatim. owned
    Vec<TipSpan>* spans = nullptr;
    bool warning = false;
    int timeoutMs = 0;
    NotifCorner corner = NotifCorner::TopLeft;
    int xMargin = kNotifDefaultMargin;
    int yMargin = kNotifDefaultMargin;
    NotificationClosed onClosed;
    // ms left before the timeout closes it; the shell's tick counts it down
    int timeLeftMs = 0;
    // 0..100, or -1 when the notification has no progress bar
    int progressPerc = -1;
    // ng: identity for the shell's listeners, which can only carry an integer
    u32 key = 0;
};

struct NotificationCreateArgs {
    MainWindow* win = nullptr;
    Kind groupId = kNotifActionResponse;
    bool warning = false;
    int timeoutMs = 0; // if 0 => persists until closed manually
    NotifCorner corner = NotifCorner::TopLeft;
    int xMargin = kNotifDefaultMargin;
    int yMargin = kNotifDefaultMargin;
    Str msg;
    // if true, `msg` is shown verbatim: the tip markup ([text](CmdFoo),
    // **bold**, (Key/..), (Kbd/..)) is not parsed. Required for any message
    // that embeds text from outside the app (file paths, document metadata,
    // server responses) - see GHSA-2wv2-qm2f-vmxh
    bool plainText = false;
    // when set, the message is these pre-built spans instead of `msg` parsed as
    // tip markup, for messages mixing app-authored markup with outside text.
    // `msg` is still what NotificationGetMessageTemp() reports. ownership of
    // the strings passes to the notification
    Vec<TipSpan>* richMsg = nullptr;
    WindowTab* tab = nullptr;
    // called on close (button, timeout, CloseNotification()) instead of the
    // default removal; must call RemoveNotification(ev->wnd)
    NotificationClosed onClosed;
};

NotificationWnd* ShowNotification(const NotificationCreateArgs& args);
NotificationWnd* ShowTemporaryNotification(MainWindow* win, Str msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowWarningNotification(MainWindow* win, Str msg, int timeoutMs);
// same, for a message that isn't fully app-authored: shown verbatim, no markup
NotificationWnd* ShowPlainNotification(MainWindow* win, Str msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowPlainWarningNotification(MainWindow* win, Str msg, int timeoutMs);

void NotificationUpdateMessage(NotificationWnd* wnd, Str msg, int timeoutInMS = 0);
bool UpdateNotificationProgress(NotificationWnd* wnd, Str msg, int perc);
TempStr NotificationGetMessageTemp(NotificationWnd* wnd);
void RemoveNotification(NotificationWnd*);
void CloseNotification(NotificationWnd*, NotifCloseReason reason = NotifCloseReason::Program);
bool AreNotificationsEnabled();
void SetNotificationsEnabled(bool);
bool RemoveNotificationsForGroup(MainWindow*, Kind);
void RemoveNotificationsForWindow(MainWindow*);
void RemoveNotificationsForTab(WindowTab*);
NotificationWnd* GetNotificationForGroup(MainWindow*, Kind);
NotificationWnd* GetNotificationByKey(u32 key);
// closes this window's notifications whose timeout elapsed; the shell's tick
// calls it with the ms since the last one
void ExpireNotifications(MainWindow* win, int elapsedMs);
void InstallLayoutNotifHooks();

void MaybeDelayedWarningNotification(Str msg);
void ShowMaybeDelayedNotifications(MainWindow* win);

// the notifications the shell should draw for this window, in creation order
const Vec<NotificationWnd*>& GetNotifications();

int CalcPerc(int current, int total);
