/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;

extern Kind kNotifCursorPos;
extern Kind kNotifActionResponse;
extern Kind kNotifPageInfo;
extern Kind kNotifAdHoc;

using NotificationWndRemoved = Func1<NotificationWnd*>;

constexpr const int kNotifDefaultTimeOut = 1000 * 3; // 3 seconds
constexpr const int kNotif5SecsTimeOut = 1000 * 5;

struct NotificationCreateArgs {
    HWND hwndParent = nullptr;
    HFONT font = nullptr;
    Kind groupId = kNotifActionResponse;
    bool warning = false;
    int timeoutMs = 0; // if 0 => persists until closed manually
    float shrinkLimit = 1.0f;
    const char* msg = nullptr;
    NotificationWndRemoved onRemoved;
};

void NotificationUpdateMessage(NotificationWnd* wnd, const char* msg, int timeoutInMS = 0, bool highlight = false);
void RemoveNotification(NotificationWnd*);
bool RemoveNotificationsForGroup(HWND hwnd, Kind);
bool RemoveNotificationsForHwnd(HWND hwnd);
NotificationWnd* GetNotificationForGroup(HWND hwnd, Kind);
bool UpdateNotificationProgress(NotificationWnd*, const char* msg, int perc);
bool NotificationExists(NotificationWnd*);
void RelayoutNotifications(HWND hwnd);

NotificationWnd* ShowNotification(const NotificationCreateArgs& args);
NotificationWnd* ShowTemporaryNotification(HWND hwnd, const char* msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowWarningNotification(HWND hwndParent, const char* msg, int timeoutMs);
bool IsNotificationValid(NotificationWnd*);

int CalcPerc(int current, int total);
