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
#include "ContentHash.h"
#include "ServerFiles.h"
#if OS_WASM
#include "gui/WasmBridge.h"
#endif

struct ServerFilesState {
    Vec<OnlineListedFile> files;
    // files contains a successful listing, which can be empty
    bool listed = false;
    // a list was started for the current key
    bool tried = false;
    // when the last list was started
    u64 listStartedMs = 0;
    // the disk cache was looked at for the current key
    bool cacheLoaded = false;
    bool refreshAfterList = false;
    u32 listId = 0;
    u32 transferId = 0;
    Str transferName;
    MainWindow* transferWin = nullptr;
};

static ServerFilesState gSrv;

struct ServerBatch {
    MainWindow* win;
    StrVec paths;
    Str account;
    ServerFileAction action;
    int next = 0;
    bool refresh = false;

    ~ServerBatch() { str::Free(account); }
};

static ServerBatch* gServerBatch;

// a list older than this is fetched again (another device may have changed it)
constexpr u64 kRelistMs = 5ULL * 60 * 1000;
static u64 gRelistMs = kRelistMs;

struct ServerStandIn {
    Str name;
    FileState* state;
};

// Never freed: the home page caches thumbnails by FileState*.
static Vec<ServerStandIn> gStandIns;

static int FindServerFile(Str name) {
    for (int i = 0; i < len(gSrv.files); i++) {
        if (str::Eq(gSrv.files[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static FileState* FindStandIn(Str name) {
    for (const ServerStandIn& entry : gStandIns) {
        if (str::Eq(entry.name, name)) {
            return entry.state;
        }
    }
    return nullptr;
}

static void ServerJobDone(OnlineJobResult* res);
static void ContinueServerBatch();

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

static void Notify(MainWindow* win, Str msg) {
    if (IsMainWindowValidAndNotClosing(win)) {
        ShowTemporaryNotification(win, msg);
    }
}

// Hashes history files that might be server files (same size), so they match
// by content. At most every kHashRescanMs: the home page asks on every build.
constexpr u64 kHashRescanMs = 2000;

enum class HashScan {
    IfDue,
    Now,
};

static void QueueHistoryHashes(HashScan scan) {
    static u64 lastMs = 0;
    u64 now = GetTickCount64();
    bool due = scan == HashScan::Now || lastMs == 0 || now - lastMs >= kHashRescanMs;
    if (!gSrv.listed || !due) {
        return;
    }
    lastMs = now;
    Vec<i64> sizes;
    for (const OnlineListedFile& f : gSrv.files) {
        VecAppend(sizes, f.size);
    }
    QueueContentHashes(sizes, MkFunc0Void(ServerFilesChanged));
}

static void ClearServerList() {
    FreeOnlineFiles(gSrv.files);
    gSrv.listed = false;
}

// The last list is kept on disk, so the badges show at startup and offline:
// "<origin> <user id>" (OnlineAccountTemp), then the /api/files JSON.
static TempStr ServerListCachePathTemp() {
    return GetPathInAppDataDirTemp(StrL("SumatraPDF-web-files.txt"));
}

// Makes body (the /api/files JSON) the server list. False if it doesn't parse.
static bool ApplyServerList(Str body) {
    Vec<OnlineListedFile> parsed;
    if (!ParseOnlineFiles(body, parsed)) {
        return false;
    }
    ClearServerList();
    // The cache takes ownership of the names allocated by the parser.
    gSrv.files = parsed;
    gSrv.listed = true;
    ForgetContentHashChecks();
    QueueHistoryHashes(HashScan::Now);
    return true;
}

static void SaveServerListCache(Str body) {
    TempStr account = OnlineAccountTemp();
    TempStr path = ServerListCachePathTemp();
    if (len(account) == 0 || len(path) == 0) {
        return;
    }
    file::WriteFileAtomic(path, str::JoinTemp(account, StrL("\n"), body));
}

// once per key: the cached list, if it is this account's
static void LoadServerListCache() {
    if (gSrv.cacheLoaded || gSrv.listed) {
        return;
    }
    gSrv.cacheLoaded = true;
    TempStr account = OnlineAccountTemp();
    TempStr path = ServerListCachePathTemp();
    if (len(account) == 0 || len(path) == 0) {
        return;
    }
    Str data = file::ReadFile(path);
    if (!data.s) {
        return;
    }
    AutoFree<char> dataFree(data.s);
    int nl = str::IndexOfChar(data, '\n');
    if (nl < 0 || !str::Eq(Str(data.s, nl), account)) {
        return;
    }
    if (ApplyServerList(Str(data.s + nl + 1, len(data) - nl - 1))) {
        logf("ServerFiles: showing the cached list until a fresh one arrives\n");
        // posted: this runs while the home page lays out
        uitask::Post(MkFunc0Void(ServerFilesChanged), "ServerFilesCached");
    }
}

static void FinishList(OnlineJobResult* res) {
    gSrv.listId = 0;
    DWORD lastStatus = 0;
    TempStr msg;
    if (OnlineJobFailed(res, &lastStatus, &msg)) {
        logf("ServerFiles: list failed: %s\n", msg);
        // a rejected key: what we had is not ours to show; otherwise (offline)
        // keep showing the last list
        if (lastStatus == kHttpStatusUnauthorized) {
            ClearServerList();
        }
        ServerFilesChanged();
        return;
    }
    if (ApplyServerList(res->body)) {
        SaveServerListCache(res->body);
    }
    ServerFilesChanged();
}

static void StartList() {
    if (gSrv.listId != 0 || !CanUseServer()) {
        return;
    }
    gSrv.listStartedMs = GetTickCount64();
    TempStr err;
    if (!StartOnlineList(ServerToken(), &gSrv.listId, MkFunc1Void<OnlineJobResult*>(ServerJobDone), &err)) {
        gSrv.listId = 0;
    }
}

static void RefreshServerList() {
    if (gSrv.listId != 0) {
        gSrv.refreshAfterList = true;
        return;
    }
    StartList();
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
    RememberOnlineJobHash(res);
    if (res->job == OnlineJob::Download) {
#if OS_WASM
        WasmPersistSettings();
#endif
        if (IsMainWindowValidAndNotClosing(win)) {
            OpenDownloadedServerFile(win, res->cachePath);
        }
        return;
    }
    if (res->job == OnlineJob::Upload) {
        Notify(win, fmt(Tr("Uploaded %s.").s, name));
    } else {
        Notify(win, fmt(Tr("Deleted %s from the server.").s, name));
    }
    if (gServerBatch) {
        gServerBatch->refresh = true;
    } else {
        RefreshServerList();
    }
}

static void ServerJobDone(OnlineJobResult* res) {
    if (res->id != 0 && res->id == gSrv.listId) {
        FinishList(res);
        if (gSrv.refreshAfterList) {
            gSrv.refreshAfterList = false;
            StartList();
        }
        return;
    }
    if (res->id != 0 && res->id == gSrv.transferId) {
        FinishTransfer(res);
        ContinueServerBatch();
    }
}

static void StartListAsync(void*) {
    StartList();
}

// Called while the home page is built (and, in orig, when a window is
// activated): shows the cached list, then fetches it, again once it's older
// than gRelistMs. The fetch starts from the message loop, not the build (wasm
// runs the request inline).
void ServerFilesEnsureListed() {
    SaveKnownContentHashes();
    if (!CanUseServer()) {
        return;
    }
    LoadServerListCache();
    QueueHistoryHashes(HashScan::IfDue);
    bool stale = gSrv.listId == 0 && GetTickCount64() - gSrv.listStartedMs >= gRelistMs;
    if (gSrv.tried && !stale) {
        return;
    }
    gSrv.tried = true;
    gSrv.listStartedMs = GetTickCount64();
    uitask::Post(MkFunc0<void>(StartListAsync, nullptr), "ServerFilesList");
}

// test hook: list again after ms instead of kRelistMs (0 restores it)
void SetServerRelistMsForTesting(int ms) {
    gRelistMs = ms > 0 ? (u64)ms : kRelistMs;
}

// The key changed: forget the names, list again on the next home page.
void ServerFilesReset() {
    ClearServerList();
    gSrv.tried = false;
    gSrv.cacheLoaded = false;
    gSrv.refreshAfterList = false;
    gSrv.listId = 0;
    ServerFilesChanged();
}

bool ServerFilesListed() {
    return gSrv.listed && CanUseServer();
}

// The server file path corresponds to, -1 if none:
//   - its content's hash is known: the server file with that sha1 (preferring
//     its name), else the one with its name, which then Differs
//   - not known yet: the one with its name; a different size Differs
// localSize < 0: unknown, taken as the same.
static int MatchServerFile(Str path, i64 localSize, ServerFileState* stOut) {
    *stOut = ServerFileState::None;
    if (!ServerFilesListed() || len(path) == 0) {
        return -1;
    }
    Str name = path::GetBaseNameTemp(path);
    int byName = FindServerFile(name);

    ContentHash h;
    bool known = KnownContentHash(path, h);
    if (known) {
        int byContent = -1;
        for (int i = 0; i < len(gSrv.files); i++) {
            const OnlineListedFile& f = gSrv.files[i];
            if (!f.hasSha1 || memcmp(f.sha1, h.sha1, kSha1Len) != 0) {
                continue;
            }
            if (byContent < 0 || i == byName) {
                byContent = i;
            }
        }
        if (byContent >= 0) {
            *stOut = ServerFileState::Same;
            return byContent;
        }
    }
    if (byName < 0) {
        return -1;
    }
    const OnlineListedFile& f = gSrv.files[byName];
    bool differs = localSize >= 0 && localSize != f.size;
    if (known && f.hasSha1) {
        differs = true;
    }
    *stOut = differs ? ServerFileState::Differs : ServerFileState::Same;
    return byName;
}

ServerFileState GetServerFileState(Str path, i64 localSize) {
    ServerFileState st;
    MatchServerFile(path, localSize, &st);
    return st;
}

// the name of the server file path corresponds to (it may differ from path's
// own when the content matched), empty if none
TempStr ServerNameOfTemp(Str path) {
    ServerFileState st;
    int idx = MatchServerFile(path, -1, &st);
    return idx >= 0 ? str::DupTemp(gSrv.files[idx].name) : TempStr{};
}

static FileState* StandInFor(Str name) {
    FileState* fs = FindStandIn(name);
    if (fs) {
        return fs;
    }
    TempStr path = OnlineDownloadPathTemp(name);
    if (len(path) == 0) {
        return nullptr;
    }
    fs = NewFileState(path);
    VecAppend(gStandIns, ServerStandIn{str::Dup(name), fs});
    return fs;
}

// The tooltip line for a file that is on the server under another name, empty
// otherwise: "On SumatraPDF Web as notes.pdf".
TempStr ServerNameNoteTemp(Str path) {
    TempStr name = ServerNameOfTemp(path);
    if (len(name) == 0 || str::Eq(name, path::GetBaseNameTemp(path))) {
        return {};
    }
    return fmt(Tr("On SumatraPDF Web as %s").s, name);
}

// the account's files no history file corresponds to (by content, else by
// name), in the server's order
void ServerOnlyFileStates(Vec<FileState*>& out) {
    if (!ServerFilesListed()) {
        return;
    }
    Vec<FileState*> history;
    FileHistoryGetRecentlyOpenedOrder(history);
    Vec<bool> covered;
    for (int i = 0; i < len(gSrv.files); i++) {
        VecAppend(covered, false);
    }
    for (FileState* fs : history) {
        if (!fs || len(fs->filePath) == 0) {
            continue;
        }
        ServerFileState st;
        int idx = MatchServerFile(fs->filePath, -1, &st);
        if (idx >= 0) {
            covered[idx] = true;
        }
    }
    for (int i = 0; i < len(gSrv.files); i++) {
        if (covered[i]) {
            continue;
        }
        FileState* fs = StandInFor(gSrv.files[i].name);
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
    FileState* fs = FindStandIn(name);
    return fs && str::Eq(fs->filePath, path) && FindServerFile(name) >= 0;
}

// the size of the server file with path's name; -1 if there is none
i64 ServerFileSize(Str path) {
    int idx = FindServerFile(path::GetBaseNameTemp(path));
    return idx >= 0 ? gSrv.files[idx].size : -1;
}

static bool CanStartTransfer(MainWindow* win);
static void TransferStarted(MainWindow* win, Str name);

// Opens the downloaded copy, downloading it first unless it has the server's size.
void OpenServerFile(MainWindow* win, Str path) {
    Str name = path::GetBaseNameTemp(path);
    int idx = FindServerFile(name);
    if (idx < 0) {
        return;
    }
    i64 size = gSrv.files[idx].size;
    if (file::GetSize(path) == size) {
        OpenDownloadedServerFile(win, path);
        return;
    }
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr err;
    auto onDone = MkFunc1Void<OnlineJobResult*>(ServerJobDone);
    if (!StartOnlineDownload(ServerToken(), name, size, &gSrv.transferId, onDone, &err)) {
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

static void StartUpload(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
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

// Upload / Delete waiting for the user's Yes
struct ServerAsk {
    MainWindow* win = nullptr;
    Str path;
};

static ServerAsk* NewServerAsk(MainWindow* win, Str path) {
    auto* ask = new ServerAsk();
    ask->win = win;
    ask->path = str::Dup(path);
    return ask;
}

static void UploadAnswered(ServerAsk* ask, bool yes) {
    MainWindow* win = ask->win;
    Str path = ask->path;
    AutoFree<char> pathFree(path.s);
    delete ask;
    if (!yes || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    StartUpload(win, path);
}

// The server keeps one file per name: uploading another file of that name
// (e.g. from a different folder) replaces it, so ask first (#6577).
void UploadFileToServer(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr why = CheckOnlineUploadTemp(path);
    if (len(why) > 0) {
        Notify(win, why);
        return;
    }
    Str name = path::GetBaseNameTemp(path);
    if (FindServerFile(name) < 0) {
        StartUpload(win, path);
        return;
    }
    auto* ask = NewServerAsk(win, path);
    TempStr question = fmt(Tr("Replace %s on SumatraPDF Web?").s, name);
    AskServerYesNo(win, question, Tr("Upload to server"), MkFunc1(UploadAnswered, ask));
}

static void StartDelete(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr name = ServerNameOfTemp(path);
    if (len(name) == 0) {
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

static void DeleteAnswered(ServerAsk* ask, bool yes) {
    MainWindow* win = ask->win;
    Str path = ask->path;
    AutoFree<char> pathFree(path.s);
    delete ask;
    if (!yes || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    StartDelete(win, path);
}

// A server delete can't be undone (a local one goes to the trash), so ask
// first. It deletes the server file path corresponds to, whatever its name.
void DeleteFileFromServer(MainWindow* win, Str path) {
    if (!CanStartTransfer(win)) {
        return;
    }
    TempStr name = ServerNameOfTemp(path);
    if (len(name) == 0) {
        return;
    }
    auto* ask = NewServerAsk(win, path);
    TempStr question = fmt(Tr("Delete %s from SumatraPDF Web?").s, name);
    AskServerYesNo(win, question, Tr("Delete from server"), MkFunc1(DeleteAnswered, ask));
}

static void ContinueServerBatch() {
    while (gServerBatch && gSrv.transferId == 0) {
        auto* b = gServerBatch;
        bool sameAccount = str::Eq(b->account, OnlineAccountTemp());
        if (b->next == len(b->paths) || !IsMainWindowValidAndNotClosing(b->win) || !ServerFilesListed() ||
            !sameAccount) {
            bool refresh = b->refresh && sameAccount;
            gServerBatch = nullptr;
            delete b;
            if (refresh) {
                RefreshServerList();
            }
            return;
        }
        Str path = b->paths[b->next++];
        switch (b->action) {
            case ServerFileAction::Open:
                OpenServerFile(b->win, path);
                break;
            case ServerFileAction::Upload: {
                TempStr why = CheckOnlineUploadTemp(path);
                if (len(why) == 0) {
                    StartUpload(b->win, path);
                } else {
                    Notify(b->win, why);
                }
                break;
            }
            case ServerFileAction::Delete:
                StartDelete(b->win, path);
                break;
        }
    }
}

static void StartServerBatch(ServerBatch* b, bool confirmed) {
    if (!confirmed || !IsMainWindowValidAndNotClosing(b->win) || !str::Eq(b->account, OnlineAccountTemp()) ||
        !CanStartTransfer(b->win) || gServerBatch) {
        delete b;
        return;
    }
    gServerBatch = b;
    ContinueServerBatch();
}

void RunServerFilesBatch(MainWindow* win, const StrVec& paths, ServerFileAction action) {
    if (!IsMainWindowValidAndNotClosing(win) || paths.IsEmpty() || !CanStartTransfer(win) || gServerBatch) {
        return;
    }
    auto* b = new ServerBatch{win, paths, str::Dup(OnlineAccountTemp()), action};
    if (action == ServerFileAction::Open) {
        StartServerBatch(b, true);
        return;
    }
    TempStr question;
    Str title;
    if (action == ServerFileAction::Upload) {
        question = fmt(Tr("Upload %d files to SumatraPDF Web? Existing files with the same name will be replaced.").s,
                       len(paths));
        title = Tr("Upload to server");
    } else {
        question = fmt(Tr("Delete %d files from SumatraPDF Web?").s, len(paths));
        title = Tr("Delete from server");
    }
    AskServerYesNo(win, question, title, MkFunc1(StartServerBatch, b));
}
