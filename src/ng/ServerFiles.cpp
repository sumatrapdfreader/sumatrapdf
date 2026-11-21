/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The account's files on SumatraPDF Web, for the home page's Server badge and
// its Upload to server / Delete from server. A file is on the server when a
// server file has its name, as on the website; a different size marks it
// changed (no hashing of local files). Listed when the home page is
// first shown, again after a change or a new key.
//
//   home page --> ServerFilesEnsureListed() --> StartOnlineList --> names --> redraw
//   menu --> Upload / Delete --> job --> notification --> list again
//   server-only entry --> OpenServerFile --> download --> open (joins history)
//
// A server file no history entry has the name of is shown on the home page
// through a stand-in FileState whose path is where it downloads to.
//
// Desktop authenticates with the saved API key; wasm with the session cookie
// of the site hosting it.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"

#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "FileHistory.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "Translations.h"
#include "SumatraPDFWebCommon.h"
#include "ServerFiles.h"
#if OS_WASM
#include "gui/WasmBridge.h"
#endif

struct ServerFilesState {
    StrVec names;
    Vec<i64> sizes;
    // names are the account's files
    bool listed = false;
    // a list was started for the current key
    bool tried = false;
    u32 listId = 0;
    u32 transferId = 0;
    Str transferName;
    MainWindow* transferWin = nullptr;
};

static ServerFilesState gSrv;

// stand-ins for server-only files, by name. Never freed: the home page caches
// thumbnails by FileState*.
static StrVec gStandInNames;
static Vec<FileState*> gStandIns;

static void ServerJobDone(OnlineJobResult* res);

static Str ServerToken() {
    if (OS_WASM || !gSettings) {
        return {};
    }
    return gSettings->onlineApiKey;
}

static bool CanUseServer() {
    if (!HasPermission(Perm::InternetAccess)) {
        return false;
    }
    return OS_WASM || (gSettings && IsOnlineApiKey(gSettings->onlineApiKey));
}

static void RedrawAllWindows() {
    for (MainWindow* win : gWindows) {
        win->RedrawAll(true);
    }
}

static void Notify(MainWindow* win, Str msg) {
    if (IsMainWindowValidAndNotClosing(win)) {
        ShowTemporaryNotification(win, msg);
    }
}

static void FinishList(OnlineJobResult* res) {
    gSrv.listId = 0;
    gSrv.names.Reset();
    VecReset(gSrv.sizes);
    gSrv.listed = false;
    DWORD lastStatus = 0;
    TempStr msg;
    if (OnlineJobFailed(res, &lastStatus, &msg)) {
        logf("ServerFiles: list failed: %s\n", msg);
        RedrawAllWindows();
        return;
    }
    Vec<OnlineListedFile> parsed;
    if (!ParseOnlineFiles(res->body, parsed)) {
        RedrawAllWindows();
        return;
    }
    for (OnlineListedFile& f : parsed) {
        gSrv.names.Append(f.name);
        VecAppend(gSrv.sizes, f.size);
    }
    FreeOnlineFiles(parsed);
    gSrv.listed = true;
    RedrawAllWindows();
}

static void StartList() {
    if (gSrv.listId != 0 || !CanUseServer()) {
        return;
    }
    TempStr err;
    if (!StartOnlineList(ServerToken(), &gSrv.listId, MkFunc1Void<OnlineJobResult*>(ServerJobDone), &err)) {
        gSrv.listId = 0;
    }
}

static void FinishTransfer(OnlineJobResult* res) {
    gSrv.transferId = 0;
    TempStr name = str::DupTemp(gSrv.transferName);
    str::FreePtr(&gSrv.transferName);
    MainWindow* win = gSrv.transferWin;
    gSrv.transferWin = nullptr;
    DWORD lastStatus = 0;
    TempStr msg;
    if (OnlineJobFailed(res, &lastStatus, &msg)) {
        Notify(win, msg);
        return;
    }
    if (res->job == OnlineJob::Download) {
#if OS_WASM
        WasmPersistSettings();
#endif
        if (IsMainWindowValidAndNotClosing(win)) {
            StartLoadDocument(win, res->cachePath);
        }
        return;
    }
    if (res->job == OnlineJob::Upload) {
        Notify(win, fmt(Tr("Uploaded %s.").s, name));
    } else {
        Notify(win, fmt(Tr("Deleted %s from the server.").s, name));
    }
    StartList();
}

static void ServerJobDone(OnlineJobResult* res) {
    if (res->id != 0 && res->id == gSrv.listId) {
        FinishList(res);
        return;
    }
    if (res->id != 0 && res->id == gSrv.transferId) {
        FinishTransfer(res);
    }
}

static void StartListAsync(void*) {
    StartList();
}

// Called while the home page is built: starts the list from the message loop,
// not the build (wasm runs the request inline).
void ServerFilesEnsureListed() {
    if (gSrv.tried || !CanUseServer()) {
        return;
    }
    gSrv.tried = true;
    uitask::Post(MkFunc0<void>(StartListAsync, nullptr), "ServerFilesList");
}

// The key changed: forget the names, list again on the next home page.
void ServerFilesReset() {
    gSrv.names.Reset();
    VecReset(gSrv.sizes);
    gSrv.listed = false;
    gSrv.tried = false;
    gSrv.listId = 0;
    RedrawAllWindows();
}

bool ServerFilesListed() {
    return gSrv.listed && CanUseServer();
}

// localSize < 0: unknown, taken as the same
ServerFileState GetServerFileState(Str path, i64 localSize) {
    if (!ServerFilesListed() || len(path) == 0) {
        return ServerFileState::None;
    }
    int idx = gSrv.names.Find(path::GetBaseNameTemp(path));
    if (idx < 0 || idx >= len(gSrv.sizes)) {
        return ServerFileState::None;
    }
    bool differs = localSize >= 0 && localSize != gSrv.sizes[idx];
    return differs ? ServerFileState::Differs : ServerFileState::Same;
}

static FileState* StandInFor(Str name) {
    int idx = gStandInNames.Find(name);
    if (idx >= 0) {
        return gStandIns[idx];
    }
    TempStr path = OnlineDownloadPathTemp(name);
    if (len(path) == 0) {
        return nullptr;
    }
    FileState* fs = NewFileState(path);
    gStandInNames.Append(name);
    VecAppend(gStandIns, fs);
    return fs;
}

// the account's files no history entry has the name of, in the server's order
void ServerOnlyFileStates(Vec<FileState*>& out) {
    if (!ServerFilesListed()) {
        return;
    }
    Vec<FileState*> history;
    FileHistoryGetRecentlyOpenedOrder(history);
    StrVec localNames;
    for (FileState* fs : history) {
        if (fs && len(fs->filePath) > 0) {
            localNames.Append(path::GetBaseNameTemp(fs->filePath));
        }
    }
    for (int i = 0; i < len(gSrv.names); i++) {
        if (localNames.Contains(gSrv.names[i])) {
            continue;
        }
        FileState* fs = StandInFor(gSrv.names[i]);
        if (fs) {
            VecAppend(out, fs);
        }
    }
}

bool IsServerOnlyFile(Str path) {
    if (!ServerFilesListed() || len(path) == 0 || FileHistoryFindByPath(path)) {
        return false;
    }
    Str name = path::GetBaseNameTemp(path);
    int idx = gStandInNames.Find(name);
    return idx >= 0 && str::Eq(gStandIns[idx]->filePath, path) && gSrv.names.Contains(name);
}

// the size of the server file with path's name; -1 if there is none
i64 ServerFileSize(Str path) {
    int idx = gSrv.names.Find(path::GetBaseNameTemp(path));
    return (idx >= 0 && idx < len(gSrv.sizes)) ? gSrv.sizes[idx] : -1;
}

static bool CanStartTransfer(MainWindow* win);
static void TransferStarted(MainWindow* win, Str name);

// Opens the downloaded copy, downloading it first unless it has the server's size.
void OpenServerFile(MainWindow* win, Str path) {
    Str name = path::GetBaseNameTemp(path);
    int idx = gSrv.names.Find(name);
    if (idx < 0 || idx >= len(gSrv.sizes)) {
        return;
    }
    if (file::GetSize(path) == gSrv.sizes[idx]) {
        StartLoadDocument(win, path);
        return;
    }
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr err;
    auto onDone = MkFunc1Void<OnlineJobResult*>(ServerJobDone);
    if (!StartOnlineDownload(ServerToken(), name, gSrv.sizes[idx], &gSrv.transferId, onDone, &err)) {
        gSrv.transferId = 0;
        Notify(win, err);
        return;
    }
    TransferStarted(win, name);
}

static bool CanStartTransfer(MainWindow* win) {
    if (!ServerFilesListed()) {
        return false;
    }
    if (gSrv.transferId != 0) {
        Notify(win, Tr("Wait for the current transfer to finish."));
        return false;
    }
    return true;
}

static void TransferStarted(MainWindow* win, Str name) {
    gSrv.transferWin = win;
    str::ReplaceWithCopy(&gSrv.transferName, name);
}

void UploadFileToServer(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr why = CheckOnlineUploadTemp(path);
    if (len(why) > 0) {
        Notify(win, why);
        return;
    }
    TempStr err;
    auto onDone = MkFunc1Void<OnlineJobResult*>(ServerJobDone);
    if (!StartOnlineUpload(ServerToken(), path, &gSrv.transferId, onDone, &err)) {
        gSrv.transferId = 0;
        Notify(win, err);
        return;
    }
    TransferStarted(win, path::GetBaseNameTemp(path));
}

void DeleteFileFromServer(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
        return;
    }
    Str name = path::GetBaseNameTemp(path);
    if (!gSrv.names.Contains(name)) {
        return;
    }
    TempStr err;
    auto onDone = MkFunc1Void<OnlineJobResult*>(ServerJobDone);
    if (!StartOnlineDelete(ServerToken(), name, &gSrv.transferId, onDone, &err)) {
        gSrv.transferId = 0;
        Notify(win, err);
        return;
    }
    TransferStarted(win, name);
}
