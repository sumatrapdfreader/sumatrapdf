/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Notifications.cpp is a win32 child window per notification,
// owner-drawn with the tip markup, laid out in the canvas corners. What is
// left here is the model: which notifications exist, in which group, for which
// tab, and for how long. The gpui shell draws them (src/gui/AppShell.cpp).

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TipMarkup.h"
#include "Notifications.h"

Kind kNotifCursorPos = "cursorPosHelper";
Kind kNotifActionResponse = "responseToAction";
Kind kNotifPageInfo = "pageInfoHelper";
Kind kNotifAdHoc = "notifAdHoc";
Kind kNotifLazyLayout = "notifLazyLayout";
Kind kNotifChapterLayout = "chapterLayout";
Kind kNotifZoomOrView = "zoomOrView";

static Vec<NotificationWnd*> gNotifications;
static Vec<Str> gDelayedNotifications;
static bool gNotificationsEnabled = true;
static u32 gNextNotifKey = 1;

void SetNotifWindow(NotificationCreateArgs& args, MainWindow* win) {
    args.win = win;
}

const Vec<NotificationWnd*>& GetNotifications() {
    return gNotifications;
}

bool AreNotificationsEnabled() {
    return gNotificationsEnabled;
}

void SetNotificationsEnabled(bool enabled) {
    gNotificationsEnabled = enabled;
}

static void FreeNotification(NotificationWnd* wnd) {
    str::Free(wnd->msg);
    if (wnd->spans) {
        TipSpansFree(*wnd->spans);
        delete wnd->spans;
    }
    delete wnd;
}

void RemoveNotification(NotificationWnd* wnd) {
    if (!wnd) {
        return;
    }
    int idx = VecFind(gNotifications, wnd);
    if (idx < 0) {
        return;
    }
    VecRemoveAt(gNotifications, idx);
    FreeNotification(wnd);
}

// orig: onClosed replaces the default removal and has to remove itself
void CloseNotification(NotificationWnd* wnd, NotifCloseReason reason) {
    if (!wnd) {
        return;
    }
    if (wnd->onClosed.IsValid()) {
        NotificationClosedEvent ev;
        ev.wnd = wnd;
        ev.reason = reason;
        wnd->onClosed.Call(&ev);
        return;
    }
    RemoveNotification(wnd);
}

void ExpireNotifications(MainWindow* win, int elapsedMs) {
    for (int i = gNotifications.len - 1; i >= 0; i--) {
        NotificationWnd* wnd = gNotifications[i];
        if (wnd->win != win || wnd->timeLeftMs <= 0) {
            continue;
        }
        wnd->timeLeftMs -= elapsedMs;
        if (wnd->timeLeftMs > 0) {
            continue;
        }
        wnd->timeLeftMs = 0;
        CloseNotification(wnd, NotifCloseReason::Timeout);
    }
}

NotificationWnd* GetNotificationForGroup(MainWindow* win, Kind kind) {
    for (NotificationWnd* wnd : gNotifications) {
        if (wnd->win == win && wnd->groupId == kind) {
            return wnd;
        }
    }
    return nullptr;
}

NotificationWnd* GetNotificationByKey(u32 key) {
    for (NotificationWnd* wnd : gNotifications) {
        if (wnd->key == key) {
            return wnd;
        }
    }
    return nullptr;
}

bool RemoveNotificationsForGroup(MainWindow* win, Kind kind) {
    bool removed = false;
    for (int i = gNotifications.len - 1; i >= 0; i--) {
        NotificationWnd* wnd = gNotifications[i];
        if (wnd->win != win || wnd->groupId != kind) {
            continue;
        }
        VecRemoveAt(gNotifications, i);
        FreeNotification(wnd);
        removed = true;
    }
    return removed;
}

void RemoveNotificationsForWindow(MainWindow* win) {
    for (int i = gNotifications.len - 1; i >= 0; i--) {
        NotificationWnd* wnd = gNotifications[i];
        if (wnd->win != win) {
            continue;
        }
        VecRemoveAt(gNotifications, i);
        FreeNotification(wnd);
    }
}

void RemoveNotificationsForTab(WindowTab* tab) {
    for (int i = gNotifications.len - 1; i >= 0; i--) {
        NotificationWnd* wnd = gNotifications[i];
        if (wnd->tab != tab) {
            continue;
        }
        VecRemoveAt(gNotifications, i);
        FreeNotification(wnd);
    }
}

NotificationWnd* ShowNotification(const NotificationCreateArgs& args) {
    if (!gNotificationsEnabled) {
        return nullptr;
    }
    // one notification per group per window, as orig does
    RemoveNotificationsForGroup(args.win, args.groupId);
    auto* wnd = new NotificationWnd();
    wnd->win = args.win;
    wnd->tab = args.tab;
    wnd->groupId = args.groupId;
    wnd->msg = str::Dup(args.msg);
    wnd->warning = args.warning;
    wnd->timeoutMs = args.timeoutMs;
    wnd->corner = args.corner;
    wnd->xMargin = args.xMargin;
    wnd->yMargin = args.yMargin;
    wnd->onClosed = args.onClosed;
    wnd->timeLeftMs = args.timeoutMs;
    wnd->key = gNextNotifKey++;
    if (args.richMsg) {
        wnd->spans = args.richMsg;
    } else if (!args.plainText && len(args.msg) > 0) {
        auto* spans = new Vec<TipSpan>();
        TipSpansParse(*spans, args.msg);
        if (TipSpansHaveRichContent(*spans)) {
            wnd->spans = spans;
        } else {
            TipSpansFree(*spans);
            delete spans;
        }
    }
    VecAppend(gNotifications, wnd);
    logf("notification: %s\n", args.msg);
    return wnd;
}

NotificationWnd* ShowTemporaryNotification(MainWindow* win, Str msg, int timeoutMs) {
    NotificationCreateArgs args;
    args.win = win;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    args.groupId = kNotifAdHoc;
    return ShowNotification(args);
}

NotificationWnd* ShowWarningNotification(MainWindow* win, Str msg, int timeoutMs) {
    NotificationCreateArgs args;
    args.win = win;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    args.warning = true;
    args.groupId = kNotifAdHoc;
    return ShowNotification(args);
}

NotificationWnd* ShowPlainNotification(MainWindow* win, Str msg, int timeoutMs) {
    NotificationCreateArgs args;
    args.win = win;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    args.groupId = kNotifAdHoc;
    args.plainText = true;
    return ShowNotification(args);
}

NotificationWnd* ShowPlainWarningNotification(MainWindow* win, Str msg, int timeoutMs) {
    NotificationCreateArgs args;
    args.win = win;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    args.warning = true;
    args.groupId = kNotifAdHoc;
    args.plainText = true;
    return ShowNotification(args);
}

void NotificationUpdateMessage(NotificationWnd* wnd, Str msg, int timeoutInMS) {
    if (!wnd) {
        return;
    }
    str::ReplaceWithCopy(&wnd->msg, msg);
    wnd->timeoutMs = timeoutInMS;
    wnd->timeLeftMs = timeoutInMS;
    if (wnd->spans) {
        TipSpansFree(*wnd->spans);
        TipSpansParse(*wnd->spans, msg);
    }
}

bool UpdateNotificationProgress(NotificationWnd* wnd, Str msg, int perc) {
    if (VecFind(gNotifications, wnd) < 0) {
        return false;
    }
    ReportIf(perc < 0 || perc > 100);
    wnd->progressPerc = limitValue(perc, 0, 100);
    NotificationUpdateMessage(wnd, msg);
    return true;
}

TempStr NotificationGetMessageTemp(NotificationWnd* wnd) {
    return wnd ? str::DupTemp(wnd->msg) : TempStr{};
}

// a warning raised before there is a window to show it in waits here
void MaybeDelayedWarningNotification(Str msg) {
    log(msg);
    MainWindow* win = gWindows.len > 0 ? gWindows[0] : nullptr;
    if (win) {
        ShowWarningNotification(win, msg, kNotifNoTimeout);
        return;
    }
    VecAppend(gDelayedNotifications, str::Dup(msg));
}

void ShowMaybeDelayedNotifications(MainWindow* win) {
    for (Str s : gDelayedNotifications) {
        ShowWarningNotification(win, s, kNotifNoTimeout);
        str::Free(s);
    }
    VecReset(gDelayedNotifications);
}

// the window half of orig's ShowChapterLayoutProgress (DisplayModel.cpp)
static void ShowChapterLayoutNotif(DisplayModel* dm, Str msg, bool finished) {
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

// the window half of orig's NotifyMediaBoxRelayout (DisplayModel.cpp)
static void ShowLazyLayoutNotif(DisplayModel* dm, Str msg) {
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

void InstallLayoutNotifHooks() {
    gShowChapterLayoutNotifFn = ShowChapterLayoutNotif;
    gShowLazyLayoutNotifFn = ShowLazyLayoutNotif;
}

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
