/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"
#include "EmbeddedResources.h"
#include "BrowserViewCommon.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "AIChatPanelCommon.h"
#include "AIChatAPI.h"

// path is host-relative, without a leading slash (e.g. "index.html")
bool AIChatPathIs(Str path, Str name) {
    if (str::EqI(path, name)) {
        return true;
    }
    return path.len > 0 && path.s[0] == '/' && str::EqI(Str(path.s + 1, path.len - 1), name);
}

// providerId is an AIChatBackend value
AIChatProvider* GetAIChatProvider(int providerId) {
    switch ((AIChatBackend)providerId) {
        case AIChatBackend::Claude:
            return GetClaudeCodeProvider();
        case AIChatBackend::Grok:
            return GetGrokBuildProvider();
        case AIChatBackend::Codex:
            return GetCodexBuildProvider();
        case AIChatBackend::AntiGravity:
            return GetAntiGravityProvider();
        case AIChatBackend::CustomAPI:
            return GetCustomAIProvider();
        default:
            return nullptr;
    }
}

AIChatProvider* CurrentProvider(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    return GetAIChatProvider(win->aiChatProvider);
}

AIChatTabState* GetTabState(WindowTab* tab, int providerId) {
    if (!tab || providerId < 0 || providerId >= kAIChatProviderCount) {
        return nullptr;
    }
    return &tab->aiChat[providerId];
}

Str kAIChatPendingSessionId() {
    return StrL("pending");
}

Str BgColorForProvider(AIChatProvider* p) {
    Str bg = p->GetBgColor();
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

void WebViewAppendText(MainWindow* win, Str text) {
    TempStr js = fmt("appendText('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddUser(MainWindow* win, Str text) {
    TempStr js = fmt("addUser('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddTool(MainWindow* win, Str text) {
    TempStr js = fmt("addTool('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddError(MainWindow* win, Str text) {
    AIChatProvider* p = CurrentProvider(win);
    if (p) {
        AIChatLog(p->logger, StrL("error"), text);
    }
    TempStr js = fmt("addError('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewFlushBlock(MainWindow* win) {
    WebViewEval(win, StrL("flushBlock()"));
}

void WebViewClearChat(MainWindow* win) {
    WebViewEval(win, StrL("clearChat()"), false); // don't record clear
}

void WebViewShowUnsupportedFileType(MainWindow* win) {
    WebViewClearChat(win);
    AIChatProvider* p = CurrentProvider(win);
    TempStr msg = fmt("%s is only available for PDF files.", p ? p->name : StrL("AI chat"));
    TempStr js = fmt("addError('%s')", JsEscapeTemp(msg));
    WebViewEval(win, js, false);
}

// history replay helpers used by providers
// used by providers to replay session history into the chat
void AIChatHistoryAddUser(MainWindow* win, Str text) {
    WebViewAddUser(win, text);
}

void AIChatHistoryAppendText(MainWindow* win, Str text) {
    WebViewAppendText(win, text);
}

void AIChatHistoryAddTool(MainWindow* win, Str text) {
    WebViewAddTool(win, text);
}

void AIChatHistoryFlushBlock(MainWindow* win) {
    WebViewFlushBlock(win);
}

void SetAIChatWorking(MainWindow* win, bool /*working*/) {
    UpdateAIChatPanelForCurrentTab(win);
}

// Each turn gets a unique id, so a turn's updates reach its tab only. Routing
// by session id mixed up two new sessions (both "pending" until the provider
// names them) and kept delivering a stopped turn's output.
int AIChatNewTurnId() {
    static int gLastTurnId = 0;
    return ++gLastTurnId;
}

// the tab running turn turnId; null for a finished or stopped turn
WindowTab* FindAIChatUpdateTab(MainWindow* win, int pid, int turnId) {
    if (turnId == 0) {
        return nullptr;
    }
    for (WindowTab* t : win->Tabs()) {
        AIChatTabState* st = GetTabState(t, pid);
        if (st && st->turnId == turnId) {
            return t;
        }
    }
    return nullptr;
}

// record a session id the provider assigned mid-stream
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId) {
    AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, sessionId);
    str::ReplaceWithCopy(&ctx->sessionId, sessionId);
}

TempStr AIChatTestResultTemp(int backend, Str filePath, Str message, int* exitCode) {
    AIChatDebugReset();
    Str text;
    Str err;
    bool ok = RunAIChatSync((AIChatBackend)backend, filePath, message, text, err);
    str::Builder res;
    if (ok) {
        res.Append(StrL("OK\n"));
        res.Append(text);
    } else {
        res.Append(StrL("FAIL: "));
        res.Append(err);
        res.Append(StrL("\n--- debug log ---\n"));
        res.Append(AIChatDebugGetTemp());
    }
    if (exitCode) {
        *exitCode = ok ? 0 : 1;
    }
    str::Free(text);
    str::Free(err);
    return str::DupTemp(ToStr(res));
}

// Inject a canned (user, assistant) turn into the chat webview, taking the exact
// same WebView* path a real turn does (addUser + appendText + flushBlock), so the
// rendering can be debugged fast without a live provider round-trip. Opens the
// grok panel first if it isn't already showing. For the -dbg-control replay test.
TempStr AIChatTestReplayResultTemp(Str userMsg, Str response, int* exitCode) {
    if (len(gWindows) == 0) {
        if (exitCode) {
            *exitCode = 2;
        }
        return str::DupTemp(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    AIChatDebugReset();
    bool grokOpen = win->uiState.aiChatVisible && win->aiChatProvider == (int)AIChatBackend::Grok;
    if (!grokOpen) {
        OnAIChatToggle(win, (int)AIChatBackend::Grok);
    }
    WebViewAddUser(win, userMsg);
    WebViewAppendText(win, response);
    WebViewFlushBlock(win);
    if (exitCode) {
        *exitCode = 0;
    }
    return str::DupTemp(StrL("OK replayed"));
}

static bool AIChatTabHasRunningProcess(WindowTab* tab) {
    if (!tab) {
        return false;
    }
    for (const AIChatTabState& chat : tab->aiChat) {
        if (chat.process) {
            return true;
        }
    }
    return false;
}

static void FreeSessionList(MainWindow* win);

void ShutdownAIChatForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (!tab) {
            continue;
        }
        for (AIChatTabState& chat : tab->aiChat) {
            AIChatCancelAPI(&chat);
            AIChatCloseProcess(&chat.process, true);
        }
    }
    AIChatWaitForTabProcessesToFinish(win, AIChatTabHasRunningProcess);
    FreeSessionList(win);
}

// --- Session list ---

// the sessions a window's combo shows; UI thread only
struct AIChatSessionList {
    MainWindow* win = nullptr; // key, never dereferenced
    int gen = 0;               // the latest scan started for win
    int shownGen = 0;          // the scan the combo shows
    // what ids belong to: tab is a key, never dereferenced
    WindowTab* tab = nullptr;
    int providerId = -1;
    Str dir;                            // owned
    WindowTab* autoSelectTab = nullptr; // key: a pending scan auto-selects for it
    StrVec ids;                         // combo item i + 1 is session ids[i]
    StrVec items;                       // the combo's items, for tests
    int sel = 0;
    ~AIChatSessionList() { str::Free(dir); }
};

static Vec<AIChatSessionList*> gAIChatSessionLists;
static int gAIChatLastScanGen = 0;

// a scan's input and result: the worker gets copies, never UI state
struct AIChatSessionScan {
    MainWindow* win = nullptr; // key, never dereferenced before validation
    WindowTab* tab = nullptr;  // key
    int providerId = -1;
    int gen = 0;
    AIChatScanMode mode = AIChatScanMode::Populate;
    Str dir; // owned
    Vec<AIChatSessionInfo> sessions;
};

static AIChatSessionList* FindSessionList(MainWindow* win) {
    for (AIChatSessionList* list : gAIChatSessionLists) {
        if (list->win == win) {
            return list;
        }
    }
    return nullptr;
}

static AIChatSessionList* GetSessionList(MainWindow* win) {
    AIChatSessionList* list = FindSessionList(win);
    if (!list) {
        list = new AIChatSessionList();
        list->win = win;
        VecAppend(gAIChatSessionLists, list);
    }
    return list;
}

static void FreeSessionList(MainWindow* win) {
    AIChatSessionList* list = FindSessionList(win);
    if (list) {
        VecRemove(gAIChatSessionLists, list);
        delete list;
    }
}

static bool SessionListIsFor(AIChatSessionList* list, WindowTab* tab, int providerId, Str dir) {
    return list->tab == tab && list->providerId == providerId && str::Eq(list->dir, dir);
}

static void SetSessionListKey(AIChatSessionList* list, WindowTab* tab, int providerId, Str dir) {
    list->tab = tab;
    list->providerId = providerId;
    str::ReplaceWithCopy(&list->dir, dir);
}

// "+ New Session", the sessions, then the tab's session if not among them
static void FillSessionCombo(MainWindow* win, AIChatSessionList* list, AIChatTabState* st,
                             Vec<AIChatSessionInfo>& sessions) {
    list->ids.Reset();
    list->items.Reset();
    StrVec& items = list->items;
    items.Append(StrL("+ New Session"));
    int selectedIdx = 0;
    for (int i = 0; i < len(sessions); i++) {
        Str display = sessions[i].display;
        if (len(display) == 0) {
            display = StrL("(no description)");
        }
        items.Append(ShortenStringUtf8Temp(display, 50));
        list->ids.Append(sessions[i].sessionId);
        if (st && st->sessionId && str::Eq(st->sessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
        }
    }
    if (st && st->sessionId && selectedIdx == 0) {
        items.Append(StrL("(current session)"));
        selectedIdx = len(sessions) + 1;
    }
    list->sel = selectedIdx;
    AIChatSetSessionItems(win, items, selectedIdx);
}

static void ApplySessionScan(MainWindow* win, AIChatSessionList* list, AIChatSessionScan* scan) {
    AIChatTabState* st = GetTabState(scan->tab, scan->providerId);
    if (scan->mode == AIChatScanMode::AutoSelect && st && len(st->sessionId) == 0 && len(scan->sessions) > 0) {
        // sorted by timestamp desc, so [0] is the most recent
        str::ReplaceWithCopy(&st->sessionId, scan->sessions[0].sessionId);
        WebViewClearChat(win);
        CurrentProvider(win)->LoadSessionHistory(win, st->sessionId, scan->dir);
    }
    SetSessionListKey(list, scan->tab, scan->providerId, scan->dir);
    list->autoSelectTab = nullptr;
    list->shownGen = scan->gen;
    FillSessionCombo(win, list, st, scan->sessions);
}

// UI thread; the window, tab or provider may have changed since the scan started
static void AIChatSessionsScanned(AIChatSessionScan* scan) {
    MainWindow* win = scan->win;
    AIChatSessionList* list = FindSessionList(win);
    bool isLatest = list && list->gen == scan->gen;
    if (isLatest && IsMainWindowValidAndNotClosing(win) && AIChatPanelShown(win) && win->CurrentTab() == scan->tab &&
        win->aiChatProvider == scan->providerId && str::Eq(path::GetDirTemp(scan->tab->filePath), scan->dir)) {
        ApplySessionScan(win, list, scan);
    }
    AIChatFreeSessions(scan->sessions);
    str::Free(scan->dir);
    delete scan;
}

static void AIChatSessionsThread(AIChatSessionScan* scan) {
    AIChatProvider* provider = GetAIChatProvider(scan->providerId);
    provider->CollectSessions(scan->dir, scan->sessions);
    uitask::Post(MkFunc0(AIChatSessionsScanned, scan), "AIChatSessionsScanned");
}

// Lists the current tab's sessions on a thread (reading session dirs can take
// seconds), then fills the combo. AutoSelect also picks the most recent
// session for a tab that has none. A newer scan drops the older one's result.
void ScanAIChatSessions(MainWindow* win, AIChatScanMode mode) {
    AIChatProvider* provider = CurrentProvider(win);
    if (!provider) {
        return;
    }
    AIChatSessionList* list = GetSessionList(win);
    list->gen = ++gAIChatLastScanGen;
    WindowTab* tab = win->CurrentTab();
    int providerId = win->aiChatProvider;
    AIChatTabState* st = GetTabState(tab, providerId);
    bool hasFile = st && len(tab->filePath) > 0;
    TempStr dir = hasFile ? path::GetDirTemp(tab->filePath) : TempStr();

    // a newer scan for the same tab keeps a pending auto-select
    if (list->autoSelectTab == tab && list->providerId == providerId) {
        mode = AIChatScanMode::AutoSelect;
    }
    list->autoSelectTab = mode == AIChatScanMode::AutoSelect ? tab : nullptr;

    // until the scan is back, don't offer another tab's sessions
    Vec<AIChatSessionInfo> none;
    if (!hasFile || !SessionListIsFor(list, tab, providerId, dir)) {
        SetSessionListKey(list, tab, providerId, dir);
        FillSessionCombo(win, list, st, none);
    }
    if (!hasFile) {
        list->autoSelectTab = nullptr;
        list->shownGen = list->gen;
        return;
    }

    auto* scan = new AIChatSessionScan();
    scan->win = win;
    scan->tab = tab;
    scan->providerId = providerId;
    scan->gen = list->gen;
    scan->mode = mode;
    scan->dir = str::Dup(dir);
    RunAsync(MkFunc0(AIChatSessionsThread, scan), StrL("AIChatSessions"));
}

// the session combo item sel stands for in win's current tab; empty for
// "+ New Session" and "(current session)"
Str AIChatSessionIdAt(MainWindow* win, int sel) {
    AIChatSessionList* list = FindSessionList(win);
    WindowTab* tab = win->CurrentTab();
    if (!list || !tab || len(tab->filePath) == 0) {
        return {};
    }
    if (!SessionListIsFor(list, tab, win->aiChatProvider, path::GetDirTemp(tab->filePath))) {
        return {};
    }
    int i = sel - 1;
    if (i < 0 || i >= len(list->ids)) {
        return {};
    }
    return list->ids[i];
}

// "pending=<0|1> sel=<n>", then the session combo's items, a line each
TempStr AIChatSessionsTestTemp(int* exitCode) {
    *exitCode = 1;
    if (len(gWindows) == 0) {
        return str::DupTemp(StrL("no window"));
    }
    AIChatSessionList* list = FindSessionList(gWindows[0]);
    if (!list) {
        return str::DupTemp(StrL("no session list"));
    }
    str::Builder res;
    res.Append(fmt("pending=%d sel=%d\n", list->gen != list->shownGen ? 1 : 0, list->sel));
    for (int i = 0; i < len(list->items); i++) {
        res.Append(list->items[i]);
        res.AppendChar('\n');
    }
    *exitCode = 0;
    return ToStrTemp(res);
}

void StopAIChat(MainWindow* win) {
    AIChatProvider* provider = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (provider && st && (st->process || st->apiRequest)) {
        AIChatLog(provider->logger, StrL("stop"), st->sessionId ? st->sessionId : StrL("(no session)"));
        AIChatCancelAPI(st);
        AIChatCloseProcess(&st->process, true);
        st->turnId = 0;
        WebViewAddError(win, StrL("Stopped by user."));
        SetAIChatWorking(win, false);
    }
}

struct AIChatUpdateData {
    // the window the update is for: orig finds it by frame, ng holds it
    HWND hwndFrame = nullptr;
    MainWindow* win = nullptr;
    int providerId = 0;
    Str text;
    Str sessionId;
    int turnId = 0; // to identify which tab this belongs to
    AIChatUpdateType updateType = AIChatUpdateType::Text;
};

static void FreeAIChatUpdateData(AIChatUpdateData* data) {
    str::Free(data->text);
    str::Free(data->sessionId);
    delete data;
}

// isActiveTab: the panel currently shows this provider and this tab, so the
// WebView reflects the update; otherwise it's only logged / recorded
static void ApplyAIChatUpdate(MainWindow* win, AIChatProvider* p, AIChatUpdateData* data, AIChatTabState* st,
                              bool isActiveTab) {
    if (st && st->apiRequest && !isActiveTab) {
        if (data->updateType == AIChatUpdateType::Text)
            st->chatLog.Append(fmt("appendText('%s')\n", JsEscapeTemp(data->text)));
        else if (data->updateType == AIChatUpdateType::Error)
            st->chatLog.Append(fmt("addError('%s')\n", JsEscapeTemp(data->text)));
        else if (data->updateType == AIChatUpdateType::Finished)
            st->chatLog.Append(StrL("flushBlock()\n"));
    }
    switch (data->updateType) {
        case AIChatUpdateType::Text:
            if (st && st->apiRequest) st->apiReply.Append(data->text);
            if (data->text) {
                AIChatLog(p->logger, StrL("<<< text"), data->text);
            }
            if (isActiveTab) {
                WebViewAppendText(win, data->text);
            }
            break;
        case AIChatUpdateType::Tool:
            if (data->text) {
                AIChatLog(p->logger, StrL("<<< tool"), data->text);
            }
            if (isActiveTab) {
                WebViewAddTool(win, data->text);
            }
            break;
        case AIChatUpdateType::Error:
            if (st && st->apiRequest) st->apiFailed = true;
            if (isActiveTab) {
                WebViewAddError(win, data->text);
            } else if (data->text) {
                AIChatLog(p->logger, StrL("error"), data->text);
            }
            break;
        case AIChatUpdateType::Flush:
            if (isActiveTab) {
                WebViewFlushBlock(win);
            }
            break;
        case AIChatUpdateType::SessionId:
            if (data->text) {
                AIChatLog(p->logger, StrL("<<< session"), data->text);
            }
            if (st && data->text) {
                str::ReplaceWithCopy(&st->sessionId, data->text);
            }
            break;
        case AIChatUpdateType::Finished:
            AIChatFinishAPI(st);
            OnAIChatFinished(win, p, st, isActiveTab);
            if (st) {
                st->turnId = 0;
            }
            break;
    }
}

static void OnAIChatUpdate(AIChatUpdateData* data) {
    MainWindow* win = AIChatUpdateWindow(data->hwndFrame, data->win);
    int pid = data->providerId;
    AIChatProvider* p = GetAIChatProvider(pid);
    if (!IsMainWindowValidAndNotClosing(win) || !AIChatPanelShown(win) || !p) {
        FreeAIChatUpdateData(data);
        return;
    }
    WindowTab* tab = FindAIChatUpdateTab(win, pid, data->turnId);
    bool isActiveTab = IsCurrentTab(win, tab) && win->aiChatProvider == pid;
    ApplyAIChatUpdate(win, p, data, GetTabState(tab, pid), isActiveTab);
    FreeAIChatUpdateData(data);
    AIChatUpdateApplied(win);
}

// per stream, not global: a chat streaming on its reader thread while a
// one-shot request parses must keep its own output
void AIChatPostUpdate(AIChatStreamCtx* ctx, AIChatUpdateType type, Str text) {
    if (AIChatCaptureSink* capture = ctx->capture) {
        if (type == AIChatUpdateType::Text) {
            capture->text.Append(text ? text : Str(""));
        } else if (type == AIChatUpdateType::Error) {
            capture->err.Append(text ? text : Str(""));
        } else if (type == AIChatUpdateType::Finished) {
            capture->finished = true;
        }
        return;
    }
    auto* data = new AIChatUpdateData();
    data->hwndFrame = ctx->hwndFrame;
    data->win = ctx->win;
    data->providerId = ctx->providerId;
    data->sessionId = ctx->sessionId ? str::Dup(ctx->sessionId) : Str{};
    data->turnId = ctx->turnId;
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnAIChatUpdate, data), "AIChatUpdate");
}
