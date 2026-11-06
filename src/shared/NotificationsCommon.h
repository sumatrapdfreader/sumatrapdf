/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
int CalcPerc(int current, int total);
void ShowLazyLayoutNotif(DisplayModel* dm, Str msg);
void ShowChapterLayoutNotif(DisplayModel* dm, Str msg, bool finished);
// implemented by each app
void RepaintNotificationNow(NotificationWnd* wnd);

// --- shared by NotificationsCommon.cpp and each app's Notifications.cpp ---

extern bool gNotificationsEnabled;
