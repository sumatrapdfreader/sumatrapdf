/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Notifications.h"
#include "NotificationsCommon.h"

// The progress notifications of lazy and per-chapter e-book layout, which
// orig and ng show the same way.

// returns 0% - 100%
int CalcPerc(int current, int total) {
    ReportIf(total <= 0 || current < 0);
    ReportIf(total < current);
    if (total <= 0) {
        total = 1;
    }
    int perc = limitValue(100 * current / total, 0, 100);
    return perc;
}

// the window half of orig's NotifyMediaBoxRelayout (DisplayModel.cpp)
void ShowLazyLayoutNotif(DisplayModel* dm, Str msg) {
    for (MainWindow* win : gWindows) {
        if (win->AsFixed() != dm) {
            continue;
        }
        NotificationCreateArgs args;
        args.win = win;
        args.groupId = kNotifLazyLayout;
        args.timeoutMs = kNotif5SecsTimeOut;
        args.corner = NotifCorner::BottomLeft;
        args.msg = msg;
        ShowNotification(args);
        return;
    }
}

// the window half of orig's ShowChapterLayoutProgress (DisplayModel.cpp)
void ShowChapterLayoutNotif(DisplayModel* dm, Str msg, bool finished) {
    MainWindow* found = nullptr;
    WindowTab* tab = nullptr;
    for (MainWindow* win : gWindows) {
        for (WindowTab* t : win->Tabs()) {
            if (t->AsFixed() == dm) {
                found = win;
                tab = t;
                break;
            }
        }
        if (found) {
            break;
        }
    }
    if (!found) {
        return;
    }
    int timeout = finished ? kNotif5SecsTimeOut : kNotifNoTimeout;
    NotificationWnd* wnd = GetNotificationForGroup(found, kNotifChapterLayout);
    if (wnd) {
        NotificationUpdateMessage(wnd, msg, timeout);
        return;
    }
    NotificationCreateArgs args;
    args.win = found;
    args.groupId = kNotifChapterLayout;
    args.timeoutMs = timeout;
    args.corner = NotifCorner::BottomLeft;
    args.msg = msg;
    args.plainText = true;
    args.tab = tab;
    ShowNotification(args);
}

void InstallLayoutNotifHooks() {
    gShowChapterLayoutNotifFn = ShowChapterLayoutNotif;
    gShowLazyLayoutNotifFn = ShowLazyLayoutNotif;
}

// Notifications are drawn over the document and stay for a couple of seconds.
// A test that reads pixels has to wait them out (which is most of the runtime
// of e.g. tests/issue-1195.ts), so -dbg-control can switch them off.
bool gNotificationsEnabled = true;

bool AreNotificationsEnabled() {
    return gNotificationsEnabled;
}

// notification groups both apps use
Kind kNotifZoomOrView = "zoomOrView";
Kind kNotifCursorPos = "cursorPosHelper";
Kind kNotifActionResponse = "responseToAction";
Kind kNotifPageInfo = "pageInfoHelper";
// can have multiple of those
Kind kNotifAdHoc = "notifAdHoc";
// debug-only: continuous layout re-done because newly visible pages got measured
Kind kNotifLazyLayout = "notifLazyLayout";
// debug-only: background chapter layout of an ebook
Kind kNotifChapterLayout = "chapterLayout";
