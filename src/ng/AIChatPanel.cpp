/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the AI chat sidebar: a single panel shared by all providers (Claude Code,
// Grok Build, OpenAI Codex, Antigravity). Everything backend-specific comes
// from AIChatProvider (see AIChatCommon.h); this file owns the UI and the
// process/stream plumbing.
//
// ng: orig's panel is a WS_CHILD static window holding win32 combos, an edit
// and a WebView2, laid out by hand and relaid out on every WM_SIZE. Here it is
// a gpui column the shell builds every frame, its chat page is a BrowserView
// (wry) and the win32 timers are the shell's tick. The rows, their order and
// the behaviour are orig's.

#include "gui/GpuiBridge.h"

#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"
#include "EmbeddedResources.h"
#include "gui/AppShell.h"
#include "gui/BrowserView.h"
#include "gui/DialogWidgets.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

#include "SumatraLog.h"

constexpr int kAIChatMinDx = 150;
// orig defers the "pick the most recent session" pass with a 500 ms timer so
// the page has loaded by then
constexpr int kAutoSelectDelayMs = 500;

// marked.min.js, unpacked once (orig reads it out of IDR_EMBEDDED_PAK)
static u8* gMarkedJs = nullptr;
static int gMarkedJsLen = 0;

static void EnsureMarkedLoaded() {
    if (gMarkedJs) {
        return;
    }
    gMarkedJs = GetEmbeddedFileData(StrL("marked.min.js"), &gMarkedJsLen);
}

// providerId is an AIChatBackend value (0=Claude, 1=Grok, 2=Codex, 3=AntiGravity)
AIChatProvider* GetAIChatProvider(int providerId) {
    if (providerId == 0) {
        return GetClaudeCodeProvider();
    }
    if (providerId == 1) {
        return GetGrokBuildProvider();
    }
    if (providerId == 2) {
        return GetCodexBuildProvider();
    }
    if (providerId == 3) {
        return GetAntiGravityProvider();
    }
    return nullptr;
}

// --- the panel --------------------------------------------------------------

struct AIChatPanel;

// serves the chat page and marked.min.js from the provider's virtual host
struct AIChatResources : BrowserViewCallback {
    AIChatPanel* panel = nullptr;

    bool OnBeforeNavigate(Str url, bool newWindow) override;
    void OnDocumentComplete(Str url) override;
    Str GetDataForUrl(Str url) override;
    void OnLButtonDown() override {}
    void DownloadData(Str, Str) override {}
};

struct AIChatPanel {
    MainWindow* win = nullptr;
    AIChatResources res;
    BrowserView* view = nullptr;
    // the chat page has finished loading and accepts js
    bool viewReady = false;
    Str html; // owned; the chat page for the current provider

    DialogSelect sessionSel;
    DialogSelect modelSel;
    DialogSelect optionSel;
    gp::InputState* input = nullptr;
    bool flagChecked = false;
    // ms left of orig's kTimerAutoSelectSession; < 0 means not armed
    int autoSelectMs = -1;
    // the selects are polled once a frame (see DialogSelect::PollChanged)
    bool initialized = false;
};

static Vec<AIChatPanel*> gPanels;

static AIChatPanel* PanelOf(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    for (AIChatPanel* p : gPanels) {
        if (p->win == win) {
            return p;
        }
    }
    return nullptr;
}

static AIChatProvider* CurrentProvider(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    return GetAIChatProvider(win->aiChatProvider);
}

static AIChatTabState* GetTabState(WindowTab* tab, int providerId) {
    if (!tab || providerId < 0 || providerId >= kAIChatProviderCount) {
        return nullptr;
    }
    return &tab->aiChat[providerId];
}

static Str kAIChatPendingSessionId() {
    return StrL("pending");
}

static Str BgColorForProvider(AIChatProvider* p) {
    Str bg = p->GetBgColor();
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

// --- the served resources ---------------------------------------------------

// path is host-relative, without a leading slash (e.g. "index.html")
static bool AIChatPathIs(Str path, Str name) {
    if (str::EqI(path, name)) {
        return true;
    }
    return path.len > 0 && path.s[0] == '/' && str::EqI(Str(path.s + 1, path.len - 1), name);
}

Str AIChatResources::GetDataForUrl(Str url) {
    if (!panel) {
        return {};
    }
    if (AIChatPathIs(url, StrL("index.html")) || len(url) == 0) {
        return panel->html;
    }
    if (AIChatPathIs(url, StrL("marked.min.js"))) {
        EnsureMarkedLoaded();
        if (gMarkedJs && gMarkedJsLen > 0) {
            return Str((char*)gMarkedJs, gMarkedJsLen);
        }
    }
    return {};
}

bool AIChatResources::OnBeforeNavigate(Str url, bool newWindow) {
    if (newWindow) {
        // a link in an answer opens in the user's browser, not in the panel
        gp::OpenUrl(ToGpui(url));
        return false;
    }
    return true;
}

// orig's OnAIChatWebViewNavigated: the page is loaded, so the chat can be
// (re)rendered into it
void AIChatResources::OnDocumentComplete(Str) {
    if (!panel) {
        return;
    }
    panel->viewReady = true;
    logf("AIChat: chat page loaded\n%s", AIChatPanelStateTemp(panel->win));
    OnAIChatTabChanged(panel->win);
}

// --- WebView helpers ---

// Execute JS on the WebView AND record it in the current tab's chat log
static void WebViewEval(MainWindow* win, Str js, bool record = true) {
    AIChatPanel* p = PanelOf(win);
    if (p && p->view && p->viewReady) {
        BrowserViewEval(p->view, js);
    }
    if (record) {
        AIChatTabState* st = GetTabState(win->CurrentTab(), win->aiChatProvider);
        if (st) {
            st->chatLog.Append(js);
            st->chatLog.AppendChar('\n');
        }
    }
}

static void WebViewAppendText(MainWindow* win, Str text) {
    TempStr js = fmt("appendText('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddUser(MainWindow* win, Str text) {
    TempStr js = fmt("addUser('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddTool(MainWindow* win, Str text) {
    TempStr js = fmt("addTool('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddError(MainWindow* win, Str text) {
    AIChatProvider* p = CurrentProvider(win);
    if (p) {
        AIChatLog(p->logger, StrL("error"), text);
    }
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewFlushBlock(MainWindow* win) {
    WebViewEval(win, StrL("flushBlock()"));
}

static void WebViewClearChat(MainWindow* win) {
    WebViewEval(win, StrL("clearChat()"), false); // don't record clear
}

static void WebViewShowUnsupportedFileType(MainWindow* win) {
    WebViewClearChat(win);
    AIChatProvider* p = CurrentProvider(win);
    TempStr msg = fmt("%s is only available for PDF and image files.", p ? p->name : StrL("AI chat"));
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(msg));
    WebViewEval(win, js, false);
}

// history replay helpers used by providers
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

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, AIChatTabState* st) {
    AIChatPanel* p = PanelOf(win);
    if (!st || len(st->chatLog) == 0 || !p || !p->view || !p->viewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str rest = ToStr(st->chatLog);
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) > 0) {
            BrowserViewEval(p->view, str::DupTemp(line));
        }
    }
}

// --- the chat page ----------------------------------------------------------

static void MakeChatPage(AIChatPanel* p, AIChatProvider* provider) {
    str::ReplaceWithCopy(&p->html, AIChatFormatChatHtmlTemp(provider->virtualHost, BgColorForProvider(provider)));
}

// the browser view is per provider (its virtual host and its chat colors are),
// so a provider or a theme change throws the old one away
static void DeleteAIChatWebView(AIChatPanel* p) {
    if (p->view) {
        BrowserViewDelete(p->view);
        p->view = nullptr;
    }
    p->viewReady = false;
}

static void EnsureWebViewReady(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    AIChatProvider* provider = CurrentProvider(win);
    if (!p || !provider || p->view) {
        return;
    }
    if (!BrowserViewAvailable()) {
        return;
    }
    EnsureMarkedLoaded();
    MakeChatPage(p, provider);
    p->res.panel = p;
    p->view = BrowserViewCreate(win, &p->res, provider->virtualHost);
    if (!p->view) {
        return;
    }
    BrowserViewSetVisible(p->view, true);
    TempStr url = fmt("%sindex.html", provider->virtualHost);
    BrowserViewNavigate(p->view, url);
    logf("AIChat: webview for '%s'\n", provider->virtualHost);
}

// --- Session combo ---

static void PopulateSessionCombo(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    AIChatProvider* provider = CurrentProvider(win);
    if (!p || !provider) {
        return;
    }

    StrVec items;
    items.Append(StrL("+ New Session"));

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || len(tab->filePath) == 0 || !st) {
        p->sessionSel.SetItems(items, 0);
        p->sessionSel.SetSel(win->gpuiWin ? win->gpuiWin->app : nullptr, 0);
        return;
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    provider->CollectSessions(dir, sessions);

    int selectedIdx = 0;
    bool foundCurrent = false;
    for (int i = 0; i < len(sessions); i++) {
        Str display = sessions[i].display;
        if (len(display) == 0) {
            display = StrL("(no description)");
        }
        items.Append(ShortenStringUtf8Temp(display, 50));
        if (st->sessionId && str::Eq(st->sessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (st->sessionId && !foundCurrent) {
        items.Append(StrL("(current session)"));
        selectedIdx = len(sessions) + 1;
    }

    p->sessionSel.SetItems(items, selectedIdx);
    p->sessionSel.SetSel(win->gpuiWin ? win->gpuiWin->app : nullptr, selectedIdx);
    AIChatFreeSessions(sessions);
}

static void OnSessionComboChange(MainWindow* win, int sel) {
    AIChatProvider* provider = CurrentProvider(win);
    if (!provider) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || len(tab->filePath) == 0 || !st) {
        return;
    }

    if (sel == 0) {
        // "New Session" - clear current session
        AIChatLog(provider->logger, StrL("session"), StrL("new"));
        str::ReplaceWithCopy(&st->sessionId, Str{});
        st->chatLog.Reset();
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    provider->CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < len(sessions)) {
        AIChatLog(provider->logger, StrL("session"), sessions[sessionIdx].sessionId);
        str::ReplaceWithCopy(&st->sessionId, sessions[sessionIdx].sessionId);
        st->chatLog.Reset();
        WebViewClearChat(win);
        provider->LoadSessionHistory(win, st->sessionId, dir);
        // LoadSessionHistory writes to the webview which rebuilds chatLog
    }

    AIChatFreeSessions(sessions);
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    AIChatProvider* provider = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!provider || !st || len(tab->filePath) == 0 || st->sessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    provider->CollectSessions(dir, sessions);

    if (len(sessions) > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        str::ReplaceWithCopy(&st->sessionId, sessions[0].sessionId);
        WebViewClearChat(win);
        provider->LoadSessionHistory(win, st->sessionId, dir);
    }

    AIChatFreeSessions(sessions);
}

// --- Settings <-> UI ---

// Apply persisted settings to the UI controls
static void ApplyAIChatSettingsToUI(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    AIChatProvider* provider = CurrentProvider(win);
    if (!p || !provider) {
        return;
    }
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;

    StrVec models;
    provider->BuildModelsList(models);
    StrVec items;
    for (int i = 0; i < len(models); i++) {
        items.Append(AIChatModelDisplayNameTemp(models[i], {}));
    }
    Str model = AIChatResolveModel(models, provider->GetModel(), provider->defaultModel);
    int modelIdx = std::max(AIChatFindModelInList(models, model), 0);
    p->modelSel.SetItems(items, modelIdx);
    p->modelSel.SetSel(app, modelIdx);

    StrVec options;
    for (int i = 0; i < provider->optionCount; i++) {
        options.Append(SeqStrByIndex(provider->optionItems, i));
    }
    int optionIdx = provider->GetOption();
    if (optionIdx < 0 || optionIdx >= provider->optionCount) {
        optionIdx = provider->optionDefault;
    }
    p->optionSel.SetItems(options, optionIdx);
    p->optionSel.SetSel(app, optionIdx);

    p->flagChecked = provider->GetFlag();
}

// Read current settings from UI controls and save
static void SyncAIChatSettingsFromUI(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    AIChatProvider* provider = CurrentProvider(win);
    if (!p || !provider) {
        return;
    }
    StrVec models;
    provider->BuildModelsList(models);
    int sel = p->modelSel.sel;
    if (sel >= 0 && sel < len(models)) {
        provider->SetModel(models[sel]);
    }
    if (p->optionSel.sel >= 0) {
        provider->SetOption(p->optionSel.sel);
    }
    provider->SetFlag(p->flagChecked);
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    ScheduleSaveSettings();
}

// --- Working state ---

static bool AIChatIsWorking(MainWindow* win) {
    AIChatTabState* st = GetTabState(win->CurrentTab(), win->aiChatProvider);
    return st && st->process != nullptr;
}

static void UpdateAIChatPanelForCurrentTab(MainWindow* win) {
    AppShellInvalidate(win);
}

static void SetAIChatWorking(MainWindow* win, bool /*working*/) {
    UpdateAIChatPanelForCurrentTab(win);
}

static void StopAIChat(MainWindow* win) {
    AIChatProvider* provider = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (provider && st && st->process) {
        AIChatLog(provider->logger, StrL("stop"), st->sessionId ? st->sessionId : StrL("(no session)"));
        AIChatCloseProcess(&st->process, true);
        WebViewAddError(win, StrL("Stopped by user."));
        SetAIChatWorking(win, false);
    }
}

// --- Stream updates (posted from the reader thread) ---

struct AIChatUpdateData {
    MainWindow* win = nullptr;
    int providerId = 0;
    Str text;
    Str sessionId; // to identify which tab this belongs to
    AIChatUpdateType updateType = AIChatUpdateType::Text;
};

static void FreeAIChatUpdateData(AIChatUpdateData* data) {
    str::Free(data->text);
    str::Free(data->sessionId);
    delete data;
}

// the tab an update belongs to; prefer tabs with a running process
static WindowTab* FindAIChatUpdateTab(MainWindow* win, int pid, Str sessionId) {
    for (WindowTab* t : win->Tabs()) {
        AIChatTabState* st = GetTabState(t, pid);
        if (!st || !st->process) {
            continue;
        }
        if (sessionId && st->sessionId && str::Eq(st->sessionId, sessionId)) {
            return t;
        }
        if (sessionId && str::Eq(sessionId, kAIChatPendingSessionId()) && len(st->sessionId) == 0) {
            return t;
        }
    }
    for (WindowTab* t : win->Tabs()) {
        AIChatTabState* st = GetTabState(t, pid);
        if (st && st->sessionId && sessionId && str::Eq(st->sessionId, sessionId)) {
            return t;
        }
    }
    return nullptr;
}

static void OnAIChatFinished(MainWindow* win, AIChatProvider* p, AIChatTabState* st, bool isActiveTab) {
    if (st && st->process) {
        if (AIChatWaitForProcess(st->process, 0)) {
            AIChatLog(p->logger, StrL("exit"), StrL("(process ended)"));
        }
        AIChatCloseProcess(&st->process, p->terminateOnFinish);
    }
    if (isActiveTab) {
        WebViewFlushBlock(win);
        SetAIChatWorking(win, false);
        PopulateSessionCombo(win);
    }
}

// isActiveTab: the panel currently shows this provider and this tab, so the
// WebView reflects the update; otherwise it's only logged / recorded
static void ApplyAIChatUpdate(MainWindow* win, AIChatProvider* p, AIChatUpdateData* data, AIChatTabState* st,
                              bool isActiveTab) {
    switch (data->updateType) {
        case AIChatUpdateType::Text:
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
            OnAIChatFinished(win, p, st, isActiveTab);
            break;
    }
}

static void OnAIChatUpdate(AIChatUpdateData* data) {
    MainWindow* win = data->win;
    int pid = data->providerId;
    AIChatProvider* p = GetAIChatProvider(pid);
    if (!IsMainWindowValidAndNotClosing(win) || !PanelOf(win) || !p) {
        FreeAIChatUpdateData(data);
        return;
    }
    WindowTab* tab = FindAIChatUpdateTab(win, pid, data->sessionId);
    bool isActiveTab = tab && tab == win->CurrentTab() && win->aiChatProvider == pid;
    ApplyAIChatUpdate(win, p, data, GetTabState(tab, pid), isActiveTab);
    FreeAIChatUpdateData(data);
    AppShellInvalidate(win);
}

// When set (only during a headless RunAIChatSync), provider updates are
// collected here instead of being posted to a webview: there's no window, and
// the message loop isn't pumping while the test blocks on the pipe.
struct AIChatCaptureSink {
    str::Builder text;
    str::Builder err;
    bool finished = false;
};
static AIChatCaptureSink* gAIChatCapture = nullptr;

void AIChatPostUpdate(AIChatStreamCtx* ctx, AIChatUpdateType type, Str text) {
    if (gAIChatCapture) {
        if (type == AIChatUpdateType::Text) {
            gAIChatCapture->text.Append(text ? text : Str(""));
        } else if (type == AIChatUpdateType::Error) {
            gAIChatCapture->err.Append(text ? text : Str(""));
        } else if (type == AIChatUpdateType::Finished) {
            gAIChatCapture->finished = true;
        }
        return;
    }
    auto* data = new AIChatUpdateData();
    data->win = ctx->win;
    data->providerId = ctx->providerId;
    data->sessionId = ctx->sessionId ? str::Dup(ctx->sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnAIChatUpdate, data), "AIChatUpdate");
}

// record a session id the provider assigned mid-stream
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId) {
    AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, sessionId);
    str::ReplaceWithCopy(&ctx->sessionId, sessionId);
}

// --- Reader thread ---

struct AIChatReadThreadCtx {
    void* hReadPipe = nullptr;
    AIChatStreamCtx stream;
};

static void AIChatReadThread(AIChatReadThreadCtx* ctx) {
    AIChatProvider* p = GetAIChatProvider(ctx->stream.providerId);

    str::Builder lineBuf;
    str::BuilderReserve(lineBuf, 4096);
    constexpr int kMaxProviderLineSize = 1024 * 1024;
    bool lineTooLong = false;
    char buf[4096];
    int bytesRead;

    while ((bytesRead = AIChatReadPipeChunk(ctx->hReadPipe, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                if (lineTooLong) {
                    AIChatPostUpdate(&ctx->stream, AIChatUpdateType::Error, StrL("Provider output line was too long"));
                } else {
                    Str line = ToStr(lineBuf);
                    if (line) {
                        AIChatLog(p->logger, StrL("<<<"), line);
                    }
                    p->ParseStreamLine(line, &ctx->stream);
                }
                lineBuf.Reset();
                lineTooLong = false;
            } else if (buf[i] != '\r' && !lineTooLong) {
                if (len(lineBuf) >= kMaxProviderLineSize) {
                    lineTooLong = true;
                    lineBuf.Reset();
                    continue;
                }
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    Str rem = lineTooLong ? Str{} : ToStr(lineBuf);
    if (rem) {
        AIChatLog(p->logger, StrL("<<<"), rem);
        p->ParseStreamLine(rem, &ctx->stream);
    }
    AIChatLog(p->logger, StrL("eof"), StrL("(stdout closed)"));

    AIChatCloseReadPipe(ctx->hReadPipe);
    AIChatPostUpdate(&ctx->stream, AIChatUpdateType::Finished, {});
    str::Free(ctx->stream.sessionId);
    delete ctx;
}

// --- Headless chat runner (for -dbg-control tests) ---

// Runs one chat turn synchronously with no UI: builds the command line, launches
// the process, reads its whole stdout, then feeds each line through the real
// provider parser while capturing the emitted text/errors. Same provider code
// the panel uses, so it exercises the real path.
static bool RunAIChatSync(AIChatBackend backend, Str filePath, Str message, Str& outText, Str& outErr) {
    AIChatProvider* p = GetAIChatProvider((int)backend);
    if (!p) {
        outErr = str::Dup(StrL("unknown backend"));
        return false;
    }
    TempStr exePath = p->FindExecutableTemp();
    if (len(exePath) == 0) {
        outErr = str::Dup(fmt("%s is not installed (not found in PATH)", p->exeName));
        return false;
    }

    StrVec models;
    p->BuildModelsList(models);
    Str model = AIChatResolveModel(models, p->GetModel(), p->defaultModel);
    int optionIdx = p->GetOption();
    if (optionIdx < 0 || optionIdx >= p->optionCount) {
        optionIdx = p->optionDefault;
    }
    TempStr sessionId = p->generatesSessionId ? AIChatGenerateSessionIdTemp() : Str{};
    TempStr dir = len(filePath) > 0 ? path::GetDirTemp(filePath) : str::DupTemp(StrL("."));
    if (len(dir) == 0) {
        dir = str::DupTemp(StrL("."));
    }

    AIChatCmdArgs args;
    args.exePath = exePath;
    args.model = model;
    args.sessionId = sessionId;
    args.filePath = filePath;
    args.dir = dir;
    args.escapedInput = message;
    args.option = optionIdx;
    args.flag = p->GetFlag();
    args.isNewSession = true;
    TempStr cmdLine = p->BuildCmdLineTemp(args);

    AIChatLog(p->logger, StrL(">>> test-user"), message);
    AIChatLog(p->logger, StrL(">>> test-file"), filePath);
    AIChatLog(p->logger, StrL(">>> test-cwd"), dir);
    AIChatLog(p->logger, StrL(">>> cmd"), cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        outErr = str::Dup(fmt("failed to launch %s", p->exeName));
        AIChatLog(p->logger, StrL("<<< error"), outErr);
        return false;
    }

    str::Builder raw;
    str::BuilderReserve(raw, 4096);
    AIChatReadPipeToEnd(launch.hReadPipe, raw);
    launch.hReadPipe = nullptr;
    if (!AIChatWaitForProcess(launch.hProcess, 5 * 60 * 1000)) {
        AIChatTerminateProcess(launch.hProcess);
        AIChatCloseProcess(&launch.hProcess, false);
        outErr = str::Dup(StrL("chat timed out"));
        AIChatLog(p->logger, StrL("<<< error"), outErr);
        return false;
    }
    AIChatCloseProcess(&launch.hProcess, false);

    // parse the collected output through the real provider parser, capturing the
    // text it emits instead of posting to a (nonexistent) webview
    AIChatCaptureSink sink;
    gAIChatCapture = &sink;
    AIChatStreamCtx ctx;
    ctx.providerId = (int)backend;
    Str rest = ToStr(raw);
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) == 0) {
            continue;
        }
        TempStr l = str::DupTemp(line);
        AIChatLog(p->logger, StrL("<<<"), l);
        p->ParseStreamLine(l, &ctx);
    }
    gAIChatCapture = nullptr;
    str::Free(ctx.sessionId);

    Str err = ToStr(sink.err);
    if (len(err) > 0) {
        outErr = str::Dup(err);
        return false;
    }
    Str txt = ToStr(sink.text);
    str::TrimWSInPlace(txt, str::TrimOpt::Both);
    if (len(txt) == 0) {
        outErr = str::Dup(StrL("response contained no text"));
        return false;
    }
    outText = str::Dup(txt);
    return true;
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
// same path a real turn does (addUser + appendText + flushBlock), so the
// rendering can be debugged fast without a live provider round-trip. Opens the
// grok panel first if it isn't already showing.
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

// --- Sending a message ---

static void SendAIChatMessage(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    AIChatProvider* provider = CurrentProvider(win);
    if (!p || !provider || !p->input) {
        return;
    }
    if (!IsAIChatSupportedForTab(win->CurrentTab())) {
        return;
    }
    TempStr input = str::DupTemp(FromGpui(gp::InputValue(p->input)));
    str::TrimWSInPlace(input, str::TrimOpt::Both);
    if (len(input) == 0) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || len(tab->filePath) == 0 || !st) {
        return;
    }
    if (st->process) {
        return; // this tab already has a running request
    }
    gp::InputSetValue(p->input, GStrL(""));

    WebViewAddUser(win, input);
    SetAIChatWorking(win, true);

    bool isNewSession = len(st->sessionId) == 0;
    if (isNewSession && provider->generatesSessionId) {
        str::ReplaceWithCopy(&st->sessionId, AIChatGenerateSessionIdTemp());
    }

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);

    // sync and save settings from UI
    SyncAIChatSettingsFromUI(win);

    StrVec models;
    provider->BuildModelsList(models);
    Str model = AIChatResolveModel(models, provider->GetModel(), provider->defaultModel);
    int optionIdx = provider->GetOption();
    if (optionIdx < 0 || optionIdx >= provider->optionCount) {
        optionIdx = provider->optionDefault;
    }

    TempStr exePath = provider->FindExecutableTemp();
    if (len(exePath) == 0) {
        AIChatLog(provider->logger, StrL("error"), fmt("Cannot find %s executable", provider->exeName));
        WebViewAddError(win, fmt("Cannot find %s. Is %s installed?", provider->exeName, provider->name));
        SetAIChatWorking(win, false);
        return;
    }

    AIChatLog(provider->logger, StrL(">>> user"), input);
    AIChatLog(provider->logger, StrL(">>> session"),
              fmt("%s (%s)", st->sessionId ? st->sessionId : kAIChatPendingSessionId(),
                  Str(isNewSession ? "new" : "resume")));
    AIChatLog(provider->logger, StrL(">>> cwd"), dir);

    AIChatCmdArgs args;
    args.exePath = exePath;
    args.model = model;
    args.sessionId = st->sessionId;
    args.filePath = filePath;
    args.dir = dir;
    // Raw user text; providers quote with QuoteCmdLineArgTemp when building the
    // command line (naive " -> \" is not enough on Windows).
    args.escapedInput = input;
    args.option = optionIdx;
    args.flag = provider->GetFlag();
    args.isNewSession = isNewSession;
    TempStr cmdLine = provider->BuildCmdLineTemp(args);

    AIChatLog(provider->logger, StrL(">>> cmd"), cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        AIChatLog(provider->logger, StrL("error"), fmt("Failed to launch %s process", provider->exeName));
        WebViewAddError(win, fmt("Failed to launch %s. Is it installed and in PATH?", provider->exeName));
        SetAIChatWorking(win, false);
        return;
    }

    st->process = launch.hProcess;
    AIChatLog(provider->logger, StrL(">>> start"), fmt("pid %d", (int)launch.processId));

    auto* ctx = new AIChatReadThreadCtx();
    ctx->hReadPipe = launch.hReadPipe;
    ctx->stream.win = win;
    ctx->stream.providerId = win->aiChatProvider;
    ctx->stream.sessionId = str::Dup(st->sessionId ? st->sessionId : kAIChatPendingSessionId());
    RunAsync(MkFunc0(AIChatReadThread, ctx), StrL("AIChatReadThread"));
}

// --- Provider switching ---

// reconfigure the panel's provider-specific parts: title, option combo
// items, checkbox label, model list and the webview (its chat colors and
// virtual host are provider-specific, so it's recreated on demand)
static void SetPanelProvider(MainWindow* win, int providerId) {
    if (win->aiChatProvider == providerId) {
        return;
    }
    AIChatProvider* p = GetAIChatProvider(providerId);
    AIChatPanel* panel = PanelOf(win);
    if (!p || !panel) {
        return;
    }
    win->aiChatProvider = providerId;
    ApplyAIChatSettingsToUI(win);
    DeleteAIChatWebView(panel);
}

// --- Theme ---

// the chat colors are baked into the page html, so a theme change throws the
// webview away; the chat log is replayed into the new one once it has loaded
void UpdateAIChatTheme(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    if (!p || !p->view) {
        return;
    }
    DeleteAIChatWebView(p);
    if (win->uiState.aiChatVisible) {
        EnsureWebViewReady(win);
    }
    AppShellInvalidate(win);
}

// --- Public API ---

void CreateAIChatPanel(MainWindow* win) {
    if (PanelOf(win)) {
        return;
    }
    auto* p = new AIChatPanel();
    p->win = win;
    p->res.panel = p;
    VecAppend(gPanels, p);

    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    p->sessionSel.Init(app);
    p->modelSel.Init(app);
    p->optionSel.Init(app);
    p->input = new gp::InputState();
    p->input->kind = gp::InputKind::Textarea;
    p->input->submitOnEnter = true;
    p->input->focus = gp::FocusHandleNew(app);

    // initialize provider-specific parts (default: Claude)
    win->aiChatProvider = -1;
    SetPanelProvider(win, 0);

    AIChatApplySavedSidebarDx(win);
}

// close the panel for the current tab (the header's close button)
static void CloseAIChatPanelFromLabel(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    AIChatSyncPanelsToCurrentTab(win);
    AppShellInvalidate(win);
}

// command entry point: toggle the panel for the given provider
void OnAIChatToggle(MainWindow* win, int providerId) {
    logf("OnAIChatToggle: providerId=%d\n", providerId);
    AIChatProvider* p = GetAIChatProvider(providerId);
    if (!p) {
        logf("OnAIChatToggle: GetAIChatProvider(%d) returned null\n", providerId);
        return;
    }
    if (!IsAIChatAvailable()) {
        logf("OnAIChatToggle: IsAIChatAvailable() returned false (no webview)\n");
        return;
    }
    if (!p->IsInstalled()) {
        logf("OnAIChatToggle: provider %s is not installed\n", p->name);
        AIChatNotInstalledDialogArgs args;
        args.windowTitle = p->TitleTemp();
        args.mainInstruction = p->NotInstalledInstructionTemp();
        args.docUri = p->docUri;
        AIChatShowNotInstalledDialog(win, args);
        return;
    }
    if (!PanelOf(win)) {
        CreateAIChatPanel(win);
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        logf("OnAIChatToggle: win->CurrentTab() returned null\n");
        return;
    }
    if (AIChatGetTabPanelOpen(tab) == p->backend) {
        logf("OnAIChatToggle: closing panel for backend %d\n", (int)p->backend);
        AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    } else {
        if (!IsAIChatSupportedForTab(tab)) {
            logf("OnAIChatToggle: not supported for tab (filePath=%s)\n", tab->filePath);
            return;
        }
        logf("OnAIChatToggle: opening panel for backend %d\n", (int)p->backend);
        AIChatSetTabPanelOpen(tab, p->backend);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->uiState.aiChatVisible) {
        SetPanelProvider(win, providerId);
        EnsureWebViewReady(win);
        UpdateAIChatPanelForCurrentTab(win);
        PopulateSessionCombo(win);
        // defer auto-select so the page has time to load
        AIChatPanel* panel = PanelOf(win);
        panel->autoSelectMs = kAutoSelectDelayMs;
    }
    logf("OnAIChatToggle:\n%s", AIChatPanelStateTemp(win));
    AppShellInvalidate(win);
}

// call when switching tabs to update session context
void OnAIChatTabChanged(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    if (!p) {
        return;
    }
    WindowTab* tab = win->CurrentTab();

    // the tab we switched to may have a different provider's panel open
    AIChatBackend open = AIChatGetTabPanelOpen(tab);
    if (win->uiState.aiChatVisible && open != AIChatBackend::None && (int)open != win->aiChatProvider) {
        SetPanelProvider(win, (int)open);
        EnsureWebViewReady(win);
    }

    bool supported = IsAIChatSupportedForTab(tab);
    UpdateAIChatPanelForCurrentTab(win);

    if (!win->uiState.aiChatVisible) {
        return;
    }

    if (!supported) {
        WebViewShowUnsupportedFileType(win);
        return;
    }

    PopulateSessionCombo(win);
    WebViewClearChat(win);

    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!st) {
        return;
    }

    // update working state for this tab
    SetAIChatWorking(win, st->process != nullptr);

    // if tab has in-memory chat log, replay it (fast, includes current session)
    if (len(st->chatLog) > 0) {
        ReplayChatLog(win, st);
    } else if (tab->filePath && st->sessionId) {
        // fallback: load from disk
        AIChatProvider* provider = CurrentProvider(win);
        TempStr dir = path::GetDirTemp(tab->filePath);
        provider->LoadSessionHistory(win, st->sessionId, dir);
    }
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

void ShutdownAIChatForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (!tab) {
            continue;
        }
        for (AIChatTabState& chat : tab->aiChat) {
            AIChatCloseProcess(&chat.process, true);
        }
    }
    AIChatWaitForTabProcessesToFinish(win, AIChatTabHasRunningProcess);
}

void DestroyAIChatPanel(MainWindow* win) {
    AIChatPanel* p = PanelOf(win);
    if (!p) {
        return;
    }
    VecRemove(gPanels, p);
    DeleteAIChatWebView(p);
    p->sessionSel.Free();
    p->modelSel.Free();
    p->optionSel.Free();
    delete p->input;
    str::Free(p->html);
    delete p;
}

// --- the gpui column --------------------------------------------------------

struct AIChatView {
    MainWindow* win = nullptr;

    static void OnClose(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(AIChatView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnStop(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnFlag(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnSplitterDrag(AIChatView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnSplitterUp(AIChatView* self, gp::Ctx* cx, const gp::MouseUpEvent*);
};

static gp::Entity<AIChatView> gAIChatView;

void AIChatView::OnClose(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseAIChatPanelFromLabel(self->win);
    gp::Notify(cx);
}

void AIChatView::OnInput(AIChatView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    SendAIChatMessage(self->win);
    gp::Notify(cx);
}

void AIChatView::OnStop(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    StopAIChat(self->win);
    gp::Notify(cx);
}

void AIChatView::OnFlag(AIChatView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    AIChatPanel* p = PanelOf(self->win);
    AIChatProvider* provider = CurrentProvider(self->win);
    if (p && provider) {
        p->flagChecked = !p->flagChecked;
        provider->SetFlag(p->flagChecked);
        ScheduleSaveSettings();
    }
    gp::Notify(cx);
}

// orig's OnAIChatSplitterMove: the panel ends where the cursor is, never
// narrower than kAIChatMinDx and never wider than half the frame
void AIChatView::OnSplitterDrag(AIChatView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    int dx = win->frameRc.dx - (int)ev->event.x;
    int maxDx = std::max(kAIChatMinDx, win->frameRc.dx / 2);
    dx = limitValue(dx, kAIChatMinDx, maxDx);
    if (dx == win->aiChatDx) {
        return;
    }
    AIChatUpdateSidebarDx(win, dx, false);
    gp::Notify(cx);
}

void AIChatView::OnSplitterUp(AIChatView* self, gp::Ctx* cx, const gp::MouseUpEvent*) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    logf("AIChat: panel width %d\n", win->aiChatDx);
    AIChatUpdateSidebarDx(win, win->aiChatDx, true);
    gp::Notify(cx);
}

// orig's UpdateAIChatPanelTitle: "<provider> chat with <document>"
static TempStr PanelTitleTemp(MainWindow* win, AIChatProvider* p) {
    Str docName = StrL("document");
    WindowTab* tab = win->CurrentTab();
    if (tab && !tab->IsAboutTab() && tab->filePath) {
        Str title = tab->GetTabTitle();
        if (len(title) > 0) {
            docName = title;
        }
    }
    return fmt("%s with %s", p->TitleTemp(), docName);
}

int AIChatPanelDx(MainWindow* win) {
    if (!win || !win->uiState.aiChatVisible || !PanelOf(win)) {
        return 0;
    }
    int dx = win->aiChatDx > 0 ? win->aiChatDx : DpiScale(360);
    return dx + kSplitterDx;
}

// the deferred auto-select orig runs off kTimerAutoSelectSession
void AIChatTick(MainWindow* win, int elapsedMs) {
    AIChatPanel* p = PanelOf(win);
    if (!p || p->autoSelectMs < 0) {
        return;
    }
    p->autoSelectMs -= elapsedMs;
    if (p->autoSelectMs > 0) {
        return;
    }
    p->autoSelectMs = -1;
    AutoSelectRecentSession(win);
    PopulateSessionCombo(win);
    AppShellInvalidate(win);
}

gp::El* AIChatPanelBuild(MainWindow* win, gp::Ctx* cx) {
    AIChatPanel* p = PanelOf(win);
    if (!p || !win->uiState.aiChatVisible) {
        return nullptr;
    }
    AIChatProvider* provider = CurrentProvider(win);
    if (!provider) {
        return nullptr;
    }
    if (!gAIChatView.IsValid()) {
        gAIChatView = gp::EntityNewState<AIChatView>(cx->app);
    }
    auto* view = (AIChatView*)gp::EntityGet(cx->app, gAIChatView.id);
    view->win = win;

    if (!p->initialized) {
        p->initialized = true;
        ApplyAIChatSettingsToUI(win);
    }
    // the drop-downs report a change through an entity subscription; polling
    // them here is what the port's dialogs do too
    if (p->sessionSel.PollChanged(cx->app)) {
        OnSessionComboChange(win, p->sessionSel.sel);
    }
    if (p->modelSel.PollChanged(cx->app) || p->optionSel.PollChanged(cx->app)) {
        SyncAIChatSettingsFromUI(win);
    }

    const gp::Theme& th = gp::ThemeNow(cx->app);
    bool supported = IsAIChatSupportedForTab(win->CurrentTab());
    bool working = AIChatIsWorking(win);
    bool enableInput = supported && !working;
    p->input->onChange = gp::ListenTo(gAIChatView, &AIChatView::OnInput);
    gp::Str cue = GStrL("Ask about this document...");
    if (!supported) {
        cue = GStrL("Not available for this file type");
    } else if (working) {
        cue = GStrL("Agent is working...");
    }
    gp::InputSetPlaceholder(p->input, cue);

    gp::El* col = gp::Div(cx->a)->FlexCol()->SizeFull()->Gap(4)->Pad(4)->Bg(th.tokens.background);

    // header: the title and a close button, as every other pane in the port
    gp::El* header = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(4);
    header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, PanelTitleTemp(win, provider)))
                      ->Font(13)
                      ->Bold()
                      ->Flex1()
                      ->MinW(0)
                      ->Truncate()
                      ->Fg(th.foreground));
    header->Child(gpc::Button::New(cx, GStrL("aichat-close"))
                      ->Label(GStrL("\xc3\x97"))
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gAIChatView, &AIChatView::OnClose))
                      ->IntoEl());
    col->Child(header);

    col->Child(p->sessionSel.Build(cx, StrL("aichat-session"), gp::kFill, !enableInput));

    // the chat page
    gp::El* chat = gp::Div(cx->a)->Flex1()->W(gp::kFill)->MinH(0);
    if (p->view) {
        chat->Child(BrowserViewBuild(p->view, cx));
    }
    col->Child(chat);

    // Enter sends, Shift+Enter inserts a line break; Stop appears while working
    gp::El* inputRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Gap(4);
    inputRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gpc::Textarea::New(cx, GStrL("aichat-input"), p->input)->Rows(3)->Disabled(!enableInput)->IntoEl()));
    gp::El* buttons = gp::Div(cx->a)->FlexCol()->Gap(4);
    if (working) {
        buttons->Child(gpc::Button::New(cx, GStrL("aichat-stop"))
                           ->Label(ToGpui(Tr("Stop")))
                           ->WithSize(gp::UiSize::Small)
                           ->OnClick(gp::ListenTo(gAIChatView, &AIChatView::OnStop))
                           ->IntoEl());
    }
    if (working) {
        inputRow->Child(buttons);
    }
    col->Child(inputRow);

    // the options row: model, effort / sandbox, the provider's flag
    gp::El* optionsRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(4);
    optionsRow->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(p->modelSel.Build(cx, StrL("aichat-model"), gp::kFill, !enableInput)));
    optionsRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        p->optionSel.Build(cx, StrL("aichat-option"), gp::kFill, !enableInput)));
    col->Child(optionsRow);
    col->Child(gpc::Checkbox::New(cx, GStrL("aichat-flag"))
                   ->Label(ToGpui(provider->checkboxLabel))
                   ->Checked(p->flagChecked)
                   ->Disabled(!enableInput)
                   ->OnClick(gp::ListenTo(gAIChatView, &AIChatView::OnFlag))
                   ->IntoEl());

    // the splitter goes on the left edge: the panel is on the right of the frame
    gp::El* row = gp::Div(cx->a)->FlexRow()->H(gp::kFill)->W((float)AIChatPanelDx(win))->Shrink0();
    row->Child(gp::Div(cx->a)
                   ->W((float)kSplitterDx)
                   ->H(gp::kFill)
                   ->Shrink0()
                   ->Bg(th.border)
                   ->Cursor(gp::CursorKind::ColResize)
                   ->PathClick(GStrL("aichat-splitter"))
                   ->OnDrag(GStrL("sumatra-aichat-splitter"))
                   ->OnDragMove(gp::ListenTo(gAIChatView, &AIChatView::OnSplitterDrag))
                   ->OnMouseUp(gp::ListenTo(gAIChatView, &AIChatView::OnSplitterUp))
                   ->OnMouseUpOut(gp::ListenTo(gAIChatView, &AIChatView::OnSplitterUp)));
    row->Child(gp::Div(cx->a)->FlexCol()->Flex1()->MinW(0)->H(gp::kFill)->Child(col));
    return row;
}

TempStr AIChatPanelStateTemp(MainWindow* win) {
    str::Builder out;
    AIChatPanel* p = PanelOf(win);
    if (!p) {
        out.Append(StrL("panel=none\n"));
        return ToStrTemp(out);
    }
    AIChatProvider* provider = CurrentProvider(win);
    out.Append(fmt("visible=%d provider=%d dx=%d\n", win->uiState.aiChatVisible ? 1 : 0, win->aiChatProvider,
                   AIChatPanelDx(win)));
    out.Append(fmt("host=%s ready=%d\n", provider ? provider->virtualHost : StrL("(none)"), p->viewReady ? 1 : 0));
    if (provider) {
        out.Append(fmt("title=%s\n", PanelTitleTemp(win, provider)));
        out.Append(fmt("checkbox=%s checked=%d\n", provider->checkboxLabel, p->flagChecked ? 1 : 0));
    }
    out.Append(
        fmt("session=%d model=%s option=%s\n", p->sessionSel.sel, p->modelSel.SelText(), p->optionSel.SelText()));
    out.Append(fmt("working=%d supported=%d\n", AIChatIsWorking(win) ? 1 : 0,
                   IsAIChatSupportedForTab(win->CurrentTab()) ? 1 : 0));
    return ToStrTemp(out);
}
