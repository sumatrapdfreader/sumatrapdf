/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct NotificationWnd;

extern Kind kNotifGroupCursorPos;
extern Kind kNotifGroupActionResponse;

using NotificationWndRemovedCallback = std::function<void(NotificationWnd*)>;

constexpr const int kNotifDefaultTimeOut = 1000 * 3; // 3 seconds
constexpr const int kNotif5SecsTimeOut = 1000 * 5;

struct NotificationCreateArgs {
    HWND hwndParent = nullptr;
    HFONT font = nullptr;
    Kind groupId = kNotifGroupActionResponse;
    bool warning = false;
    int timeoutMs = 0; // if 0 => persists until closed manually
    const char* msg = nullptr;
    const char* progressMsg = nullptr;
    NotificationWndRemovedCallback onRemoved;
};

void NotificationUpdateMessage(NotificationWnd* wnd, const char* msg, int timeoutInMS = 0, bool highlight = false);
void RemoveNotification(NotificationWnd*);
void RemoveNotificationsForGroup(HWND hwnd, Kind);
NotificationWnd* GetNotificationForGroup(HWND hwnd, Kind);
bool UpdateNotificationProgress(NotificationWnd*, int, int);
bool NotificationExists(NotificationWnd*);
void RelayoutNotifications(HWND hwnd);

NotificationWnd* ShowNotification(const NotificationCreateArgs& args);
NotificationWnd* ShowTemporaryNotification(HWND hwnd, const char* msg, int timeoutMs = kNotifDefaultTimeOut);
NotificationWnd* ShowWarningNotification(HWND hwndParent, const char* msg, int timeoutMs);
