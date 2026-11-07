/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DisplayMode.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Notifications.h"
#include "EmbeddedResources.h"
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

// in each app's SearchAndDDE.cpp; its header needs paint types this file has no use for
MainWindow* LastActiveWindow();

static Kind kNotifFontDownload = "fontDownload";

struct FontDownloadMsg {
    Str name; // owned
    i64 nDownloaded = 0;
    i64 total = 0;
    FontDownloadEvent ev = FontDownloadEvent::Progress;
};

// bottom-left progress bar while a built-in font downloads, then the result
static void ShowFontDownloadNotif(const FontDownloadMsg& m, bool repaintNow) {
    MainWindow* win = LastActiveWindow();
    if (!win) {
        return;
    }
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifFontDownload);
    NotificationCreateArgs args;
    args.win = win;
    args.groupId = kNotifFontDownload;
    args.corner = NotifCorner::BottomLeft;
    args.plainText = true;

    if (m.ev == FontDownloadEvent::Progress) {
        TempStr got = str::FormatSizeShortTemp(m.nDownloaded);
        TempStr msg = m.total > 0 ? fmt("Downloading font %s: %s of %s", m.name, got, str::FormatSizeShortTemp(m.total))
                                  : fmt("Downloading font %s: %s", m.name, got);
        if (!wnd) {
            args.msg = msg;
            args.timeoutMs = kNotifNoTimeout;
            wnd = ShowNotification(args);
        }
        int perc = m.total > 0 ? CalcPerc((int)(std::min(m.nDownloaded, m.total) / 1024), (int)(m.total / 1024)) : 0;
        if (wnd) {
            UpdateNotificationProgress(wnd, msg, perc);
        }
    } else {
        // a fresh notification: this one has no progress bar
        RemoveNotificationsForGroup(win, kNotifFontDownload);
        bool ok = m.ev == FontDownloadEvent::Done;
        args.msg = ok ? fmt("Downloaded font %s (%s)", m.name, str::FormatSizeShortTemp(m.total))
                      : fmt("Failed to download font %s", m.name);
        args.warning = !ok;
        args.timeoutMs = ok ? kNotifDefaultTimeOut : kNotif5SecsTimeOut;
        wnd = ShowNotification(args);
    }
    if (repaintNow && wnd) {
        RepaintNotificationNow(wnd);
    }
}

static void ShowPostedFontDownloadNotif(FontDownloadMsg* m) {
    ShowFontDownloadNotif(*m, false);
    str::Free(m->name);
    delete m;
}

// Fonts download on whatever thread mupdf asks for them. Elsewhere the update is
// posted to the UI thread. On the UI thread it can't be: the queue doesn't run
// until the download returns, so the notification is painted right away instead.
static void OnFontDownloadProgress(Str name, i64 nDownloaded, i64 total, FontDownloadEvent ev) {
    FontDownloadMsg m;
    m.name = name;
    m.nDownloaded = nDownloaded;
    m.total = total;
    m.ev = ev;
    if (uitask::IsMainUIThread()) {
        ShowFontDownloadNotif(m, true);
        return;
    }
    auto* posted = new FontDownloadMsg(m);
    posted->name = str::Dup(name);
    uitask::Post(MkFunc0(ShowPostedFontDownloadNotif, posted), "FontDownloadNotif");
}

void InstallFontDownloadNotifHook() {
    SetFontDownloadProgressFn(OnFontDownloadProgress);
}

// Notifications are drawn over the document and stay for a couple of seconds.
// A test that reads pixels has to wait them out (which is most of the runtime
// of e.g. tests/issue-1195.ts), so -dbg-control can switch them off.
bool gNotificationsEnabled = true;

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
