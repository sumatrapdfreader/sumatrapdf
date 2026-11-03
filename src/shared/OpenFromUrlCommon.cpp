/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Http.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Translations.h"
#if OS_WASM
#include "gui/WasmBridge.h"
#endif
#include "OpenFromUrlCommon.h"

#include "SumatraLog.h"

// Open from URL... (#6477): the dialog asks for the URL and the folder, the file
// is downloaded there on a thread and opened. wasm: the browser fetches it and
// it's stored like a dropped file.
//
//   dialog --> StartOpenFromUrl --> thread: HttpGetToFile --> UI: OpenDownloadedFile
//                                   (wasm: WasmFetchUrl, fetch() or the host's)

static Kind kNotifOpenFromUrl = "notifOpenFromUrl";
// how often the download's progress is shown
constexpr u64 kProgressIntervalMs = 300;

// "example.com/a.pdf" -> "https://example.com/a.pdf"; {} unless http(s)
TempStr NormalizeDownloadUrlTemp(Str s) {
    TempStr url = str::DupTemp(s);
    str::TrimWSInPlace(url, str::TrimOpt::Both);
    if (len(url) == 0) {
        return {};
    }
    if (str::IndexOf(url, StrL("://")) < 0) {
        url = str::JoinTemp(StrL("https://"), url);
    }
    if (!str::StartsWithI(url, StrL("http://")) && !str::StartsWithI(url, StrL("https://"))) {
        return {};
    }
    return url;
}

// "https://a.com/files/my%20doc.pdf?x=1" -> "my doc.pdf"
TempStr UrlFileNameTemp(Str url) {
    int start = str::IndexOf(url, StrL("://"));
    start = start < 0 ? 0 : start + 3;
    Str rest = Str(url.s + start, len(url) - start);
    int end = len(rest);
    for (int i = 0; i < len(rest); i++) {
        if (rest.s[i] == '?' || rest.s[i] == '#') {
            end = i;
            break;
        }
    }
    rest = Str(rest.s, end);
    int slash = str::IndexOfChar(rest, '/');
    TempStr name;
    if (slash >= 0) {
        Str urlPath = Str(rest.s + slash, len(rest) - slash);
        int last = str::LastIndexOfChar(urlPath, '/');
        name = url::DecodeTemp(Str(urlPath.s + last + 1, len(urlPath) - last - 1));
    }
    // characters a file name can't have on Windows
    for (int i = 0; i < len(name); i++) {
        char c = name.s[i];
        if ((u8)c < 32 || str::IndexOfChar(StrL("\\/:*?\"<>|"), c) >= 0) {
            name.s[i] = '_';
        }
    }
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    if (len(name) == 0 || str::Eq(name, StrL(".")) || str::Eq(name, StrL(".."))) {
        return str::DupTemp(StrL("download"));
    }
    return name;
}

// dir/name, or dir/"name (2).ext" ... when that is taken
TempStr UniqueFilePathTemp(Str dir, Str name) {
    TempStr path = path::JoinTemp(dir, name);
    TempStr ext = path::GetExtTemp(name);
    Str base = Str(name.s, len(name) - len(ext));
    for (int n = 2; FileSystemEntryExists(path); n++) {
        path = path::JoinTemp(dir, fmt("%s (%d)%s", base, n, ext));
    }
    return path;
}

// DownloadDirectory when it's a folder, else the Downloads folder
TempStr DownloadDirTemp() {
    Str dir = gSettings->downloadDirectory;
    if (len(dir) > 0 && dir::Exists(dir)) {
        return str::DupTemp(dir);
    }
    return GetDownloadsDirTemp();
}

static void ShowDownloadFailed(MainWindow* win, Str url) {
    RemoveNotificationsForGroup(win, kNotifOpenFromUrl);
    ShowPlainWarningNotification(win, fmt("%s %s", Tr("Failed to download"), url), kNotif5SecsTimeOut);
}

// the window the download started from, or another if it was closed
static MainWindow* DownloadWindow(MainWindow* win) {
    if (IsMainWindowValidAndNotClosing(win)) {
        return win;
    }
    for (MainWindow* w : gWindows) {
        if (IsMainWindowValidAndNotClosing(w)) {
            return w;
        }
    }
    return nullptr;
}

#if OS_WASM
struct WasmFetchData {
    MainWindow* win = nullptr;
    Str url;
};

static void OnWasmUrlFetched(WasmFetchData* d, Str path) {
    MainWindow* win = DownloadWindow(d->win);
    if (win) {
        if (len(path) == 0) {
            ShowDownloadFailed(win, d->url);
        } else {
            RemoveNotificationsForGroup(win, kNotifOpenFromUrl);
            OpenDownloadedFile(win, path);
        }
    }
    str::Free(d->url);
    delete d;
}
#else
struct UrlDownload {
    MainWindow* win = nullptr;
    Str url;
    Str path;
    bool ok = false;
    u64 lastProgressMs = 0; // download thread only
};

struct UrlDownloadProgress {
    UrlDownload* d = nullptr;
    i64 nDownloaded = 0;
};

static void ShowUrlDownloadProgress(UrlDownloadProgress* p) {
    MainWindow* win = p->d->win;
    NotificationWnd* wnd =
        IsMainWindowValidAndNotClosing(win) ? GetNotificationForGroup(win, kNotifOpenFromUrl) : nullptr;
    if (wnd) {
        TempStr name = path::GetBaseNameTemp(p->d->path);
        NotificationUpdateMessage(
            wnd, fmt("%s %s: %s", Tr("Downloading"), name, str::FormatSizeShortTemp(p->nDownloaded, nullptr)));
    }
    delete p;
}

static void OnUrlDownloadProgress(UrlDownload* d, HttpProgress* progress) {
    u64 now = GetTickCount64();
    if (now - d->lastProgressMs < kProgressIntervalMs) {
        return;
    }
    d->lastProgressMs = now;
    auto* p = new UrlDownloadProgress{d, progress->nDownloaded};
    uitask::Post(MkFunc0<UrlDownloadProgress>(ShowUrlDownloadProgress, p), "UrlDownloadProgress");
}

static void UrlDownloadDone(UrlDownload* d) {
    MainWindow* win = DownloadWindow(d->win);
    logf("UrlDownloadDone: ok=%d '%s' -> '%s'\n", d->ok ? 1 : 0, d->url, d->path);
    if (win && !d->ok) {
        ShowDownloadFailed(win, d->url);
    } else if (win) {
        RemoveNotificationsForGroup(win, kNotifOpenFromUrl);
        OpenDownloadedFile(win, d->path);
    }
    str::Free(d->url);
    str::Free(d->path);
    delete d;
}

// a URL like ".../download?id=3" names the file "download": add the extension
// of what was downloaded, so it opens with the right engine
static void AddMissingExt(UrlDownload* d) {
    FileType kind = GuessFileTypeFromFile(d->path);
    TempStr ext = GetExtForFileTypeTemp(kind);
    if (len(ext) == 0 || GuessFileTypeFromName(d->path) == kind) {
        return;
    }
    TempStr name = str::JoinTemp(path::GetBaseNameTemp(d->path), ext);
    TempStr newPath = UniqueFilePathTemp(path::GetDirTemp(d->path), name);
    if (file::Rename(newPath, d->path)) {
        str::ReplaceWithCopy(&d->path, newPath);
    }
}

static void UrlDownloadThread(UrlDownload* d) {
    auto onProgress = MkFunc1<UrlDownload, HttpProgress*>(OnUrlDownloadProgress, d);
    d->ok = HttpGetToFile(d->url, d->path, onProgress);
    if (d->ok) {
        AddMissingExt(d);
    }
    uitask::Post(MkFunc0<UrlDownload>(UrlDownloadDone, d), "UrlDownloadDone");
}
#endif

static void ShowDownloadingNotification(MainWindow* win, Str name) {
    RemoveNotificationsForGroup(win, kNotifOpenFromUrl);
    NotificationCreateArgs args;
    SetNotifWindow(args, win);
    args.groupId = kNotifOpenFromUrl;
    args.plainText = true;
    args.msg = fmt("%s %s", Tr("Downloading"), name);
    ShowNotification(args);
}

// the dialog's OK: false (and a warning) when the URL or the folder won't do, so
// the dialog stays open. Remembers the folder in DownloadDirectory
bool StartOpenFromUrl(MainWindow* win, Str urlIn, Str dirIn) {
    TempStr url = NormalizeDownloadUrlTemp(urlIn);
    if (len(url) == 0) {
        ShowWarningNotification(win, Tr("Enter a http:// or https:// URL"), kNotif5SecsTimeOut);
        return false;
    }
    TempStr name = UrlFileNameTemp(url);
#if OS_WASM
    (void)dirIn;
    ShowDownloadingNotification(win, name);
    auto* d = new WasmFetchData{win, str::Dup(url)};
    WasmFetchUrl(url, name, MkFunc1<WasmFetchData, Str>(OnWasmUrlFetched, d));
    return true;
#else
    TempStr dir = str::DupTemp(dirIn);
    str::TrimWSInPlace(dir, str::TrimOpt::Both);
    if (len(dir) == 0 || !dir::Exists(dir)) {
        ShowWarningNotification(win, Tr("The folder doesn't exist"), kNotif5SecsTimeOut);
        return false;
    }
    TempStr path = UniqueFilePathTemp(dir, name);
    // reserves the name, so another download doesn't pick it too
    if (!file::WriteFile(path, Str())) {
        ShowWarningNotification(win, Tr("Can't write to the folder"), kNotif5SecsTimeOut);
        return false;
    }
    if (!str::Eq(dir, gSettings->downloadDirectory)) {
        str::ReplaceWithCopy(&gSettings->downloadDirectory, dir);
        ScheduleSaveSettings();
    }
    ShowDownloadingNotification(win, path::GetBaseNameTemp(path));
    auto* d = new UrlDownload();
    d->win = win;
    d->url = str::Dup(url);
    d->path = str::Dup(path);
    RunAsync(MkFunc0<UrlDownload>(UrlDownloadThread, d), StrL("OpenFromUrl"));
    return true;
#endif
}
