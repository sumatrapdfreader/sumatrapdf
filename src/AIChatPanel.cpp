/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the AI chat sidebar: a single panel shared by all providers (Claude Code,
// Grok Build, OpenAI Codex). Everything backend-specific comes from
// AIChatProvider (see AIChatCommon.h); this file owns the UI and the
// process/stream plumbing.

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/WebView.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkMode_win.h"
#include "EmbeddedResources.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

// timer ids on hwndAiChatBox
constexpr UINT_PTR kTimerAutoSelectSession = 42;
constexpr UINT_PTR kTimerWebViewSize = 43;

// The chat page and its marked.min.js are both served to WebView2 from the
// provider's virtual host (https://<provider>/). NavigateToString/SetHtml does
// not reliably load in the current WebView2 runtime (the page stays blank),
// whereas Navigate() to a resource-served URL does - the same mechanism the
// in-app manual and CHM viewers use. marked.min.js comes from IDR_EMBEDDED_PAK.
struct AIChatWebResources {
    u8* marked = nullptr; // owned (malloc); from GetEmbeddedFileData
    int markedLen = 0;
    Str html; // owned; the chat page HTML
};
static AIChatWebResources gAIChatWebResources;

// path is host-relative, without a leading slash (e.g. "index.html")
static bool AIChatPathIs(Str path, Str name) {
    if (str::EqI(path, name)) {
        return true;
    }
    return path.len > 0 && path.s[0] == '/' && str::EqI(Str(path.s + 1, path.len - 1), name);
}

static bool AIChatGetResource(void* ctx, Str path, WebViewResourceResult* res) {
    auto* r = (AIChatWebResources*)ctx;
    if (!r || !res || len(path) == 0) {
        return false;
    }
    if (AIChatPathIs(path, StrL("index.html")) || str::Eq(path, StrL("/")) || str::Eq(path, StrL(""))) {
        if (len(r->html) == 0) {
            return false;
        }
        res->data = (const u8*)r->html.s;
        res->dataLen = (size_t)r->html.len;
        res->contentType = str::Dup(StrL("text/html; charset=utf-8"));
        res->ownsData = false;
        return true;
    }
    if (r->marked && r->markedLen > 0 && AIChatPathIs(path, StrL("marked.min.js"))) {
        res->data = r->marked;
        res->dataLen = (size_t)r->markedLen;
        res->contentType = str::Dup(StrL("text/javascript"));
        res->ownsData = false;
        return true;
    }
    return false;
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

// --- WebView helpers ---

// Execute JS on the WebView AND record it in the current tab's chat log
static void WebViewEval(MainWindow* win, Str js, bool record = true) {
    if (win->aiChatWebView && win->aiChatWebViewReady) {
        win->aiChatWebView->Eval(js);
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

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, AIChatTabState* st) {
    if (!st || len(st->chatLog) == 0) {
        return;
    }
    if (!win->aiChatWebView || !win->aiChatWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str log = ToStr(st->chatLog);
    Str rest = log;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) > 0) {
            win->aiChatWebView->Eval(str::DupTemp(line));
        }
    }
}

// --- Panel title ---

static void UpdateAIChatPanelTitle(MainWindow* win, int labelDx) {
    AIChatProvider* p = CurrentProvider(win);
    if (!win || !win->aiChatLabel || !p) {
        return;
    }
    Str docName = StrL("document");
    WindowTab* tab = win->CurrentTab();
    if (tab && !tab->IsAboutTab() && tab->filePath) {
        Str title = tab->GetTabTitle();
        if (len(title) > 0) {
            docName = title;
        }
    }

    PlatformFont* font = win->aiChatLabel->font;
    if (!font) {
        font = GetDefaultGuiFont(true, false);
    }
    if (labelDx <= 0 && win->hwndAiChatBox) {
        labelDx = HwndClientRect(win->hwndAiChatBox).dx;
    }
    int maxDx = AIChatLabelMaxTextDx(labelDx);
    TempStr prefix = str::JoinTemp(p->TitleTemp(), StrL(" with "));
    TempStr label = AIChatFitPanelTitleTemp(font, prefix, docName, maxDx);
    win->aiChatLabel->SetText(label);
}

// --- Layout ---

static void LayoutAIChatBox(MainWindow* win) {
    if (!win->aiChatLayout) {
        return;
    }
    Rect rc = HwndClientRect(win->hwndAiChatBox);
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }

    UpdateAIChatPanelTitle(win, rc.dx);
    LayoutTreeToSize(win->hwndAiChatBox, win->aiChatLayout, {rc.dx, rc.dy}, &win->aiChatRoot);

    // the webview is created lazily so it's not part of the layout; a flex
    // spacer reserves its area and we position it into the spacer's bounds
    if (win->aiChatWebView) {
        Rect wr = win->aiChatWebViewSlot->lastBounds;
        MoveWindow(win->aiChatWebView->hwnd, wr.x, wr.y, wr.dx, wr.dy, TRUE);
        // defer UpdateWebviewSize during rapid WM_SIZE to avoid WebView2 put_Bounds freeze
        KillTimer(win->hwndAiChatBox, kTimerWebViewSize);
        SetTimer(win->hwndAiChatBox, kTimerWebViewSize, 50, nullptr);
    }
}

// --- Session combo ---

static void PopulateSessionCombo(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!win->aiChatSessionCombo || !p) {
        return;
    }

    StrVec items;
    items.Append(StrL("+ New Session"));

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || len(tab->filePath) == 0 || !st) {
        win->aiChatSessionCombo->SetItems(items);
        CbSetCurrentSelection(win->aiChatSessionCombo, 0);
        return;
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    p->CollectSessions(dir, sessions);

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

    win->aiChatSessionCombo->SetItems(items);
    CbSetCurrentSelection(win->aiChatSessionCombo, selectedIdx);
    AIChatFreeSessions(sessions);
}

static void OnSessionComboChange(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!p) {
        return;
    }
    int sel = CbGetCurrentSelection(win->aiChatSessionCombo);

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || len(tab->filePath) == 0 || !st) {
        return;
    }

    if (sel == 0) {
        // "New Session" — clear current session
        AIChatLog(p->logger, StrL("session"), StrL("new"));
        str::ReplaceWithCopy(&st->sessionId, Str{});
        st->chatLog.Reset();
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    p->CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < len(sessions)) {
        AIChatLog(p->logger, StrL("session"), sessions[sessionIdx].sessionId);
        str::ReplaceWithCopy(&st->sessionId, sessions[sessionIdx].sessionId);
        st->chatLog.Reset();
        WebViewClearChat(win);
        p->LoadSessionHistory(win, st->sessionId, dir);
        // LoadSessionHistory writes to the webview which rebuilds chatLog
    }

    AIChatFreeSessions(sessions);
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!p || !st || len(tab->filePath) == 0 || st->sessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    p->CollectSessions(dir, sessions);

    if (len(sessions) > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        str::ReplaceWithCopy(&st->sessionId, sessions[0].sessionId);
        WebViewClearChat(win);
        p->LoadSessionHistory(win, st->sessionId, dir);
    }

    AIChatFreeSessions(sessions);
}

// --- Settings <-> UI ---

static void PopulateModelCombo(MainWindow* win, AIChatProvider* p) {
    StrVec models;
    p->BuildModelsList(models);
    StrVec items;
    for (int i = 0; i < len(models); i++) {
        items.Append(AIChatModelDisplayNameTemp(models[i], {}));
    }
    win->aiChatModelCombo->SetItems(items);
}

// Apply persisted settings to the UI controls
static void ApplyAIChatSettingsToUI(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!p) {
        return;
    }
    if (win->aiChatModelCombo) {
        PopulateModelCombo(win, p);
        StrVec models;
        p->BuildModelsList(models);
        Str model = AIChatResolveModel(models, p->GetModel(), p->defaultModel);
        int modelIdx = std::max(AIChatFindModelInList(models, model), 0);
        CbSetCurrentSelection(win->aiChatModelCombo, modelIdx);
    }
    if (win->aiChatOptionCombo) {
        int optionIdx = p->GetOption();
        if (optionIdx < 0 || optionIdx >= p->optionCount) {
            optionIdx = p->optionDefault;
        }
        CbSetCurrentSelection(win->aiChatOptionCombo, optionIdx);
    }
    if (win->aiChatCheckbox) {
        win->aiChatCheckbox->SetIsChecked(p->GetFlag());
    }
}

// Read current settings from UI controls and save
static void SyncAIChatSettingsFromUI(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!p) {
        return;
    }
    if (win->aiChatModelCombo) {
        int sel = CbGetCurrentSelection(win->aiChatModelCombo);
        StrVec models;
        p->BuildModelsList(models);
        if (sel >= 0 && sel < len(models)) {
            p->SetModel(models[sel]);
        }
    }
    if (win->aiChatOptionCombo) {
        p->SetOption(CbGetCurrentSelection(win->aiChatOptionCombo));
    }
    if (win->aiChatCheckbox) {
        p->SetFlag(win->aiChatCheckbox->IsChecked());
    }
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    ScheduleSaveSettings();
}

// --- Working state ---

static void UpdateAIChatPanelForCurrentTab(MainWindow* win) {
    if (!win || !win->hwndAiChatBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    bool supported = IsAIChatSupportedForTab(tab);
    bool working = supported && st && st->process != nullptr;
    bool enableInput = supported && !working;

    if (win->aiChatInput) {
        EnableWindow(win->aiChatInput->hwnd, enableInput);
        WStr cue = WStrL(L"Ask about this document...");
        if (!supported) {
            cue = WStrL(L"Not available for this file type");
        } else if (working) {
            cue = WStrL(L"Agent is working...");
        }
        SendMessageW(win->aiChatInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)cue.s);
    }
    if (win->aiChatSessionCombo) {
        EnableWindow(win->aiChatSessionCombo->hwnd, enableInput);
    }
    if (win->aiChatModelCombo) {
        EnableWindow(win->aiChatModelCombo->hwnd, enableInput);
    }
    if (win->aiChatOptionCombo) {
        EnableWindow(win->aiChatOptionCombo->hwnd, enableInput);
    }
    if (win->aiChatCheckbox) {
        EnableWindow(win->aiChatCheckbox->hwnd, enableInput);
    }
    if (win->aiChatStopBtn) {
        win->aiChatStopBtn->SetIsVisible(working);
        win->aiChatStopBtn->SetIsEnabled(working);
    }
    LayoutAIChatBox(win);
}

static void SetAIChatWorking(MainWindow* win, bool /*working*/) {
    UpdateAIChatPanelForCurrentTab(win);
}

static void StopAIChat(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (p && st && st->process) {
        AIChatLog(p->logger, StrL("stop"), st->sessionId ? st->sessionId : StrL("(no session)"));
        AIChatCloseProcess(&st->process, true);
        WebViewAddError(win, StrL("Stopped by user."));
        SetAIChatWorking(win, false);
    }
}

// --- Stream updates (posted from the reader thread) ---

struct AIChatUpdateData {
    HWND hwndFrame = nullptr;
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
        if (WaitForSingleObject(st->process, 0) == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            GetExitCodeProcess(st->process, &exitCode);
            AIChatLog(p->logger, StrL("exit"), fmt("%lu", exitCode));
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
    MainWindow* win = AIChatFindMainWindowByFrame(data->hwndFrame);
    int pid = data->providerId;
    AIChatProvider* p = GetAIChatProvider(pid);
    if (!IsMainWindowValidAndNotClosing(win) || !win->hwndAiChatBox || !p) {
        FreeAIChatUpdateData(data);
        return;
    }
    WindowTab* tab = FindAIChatUpdateTab(win, pid, data->sessionId);
    bool isActiveTab = tab && tab == win->CurrentTab() && win->aiChatProvider == pid;
    ApplyAIChatUpdate(win, p, data, GetTabState(tab, pid), isActiveTab);
    FreeAIChatUpdateData(data);
}

// post an update to be applied on the UI thread (implemented in AIChatPanel.cpp)
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
    data->hwndFrame = ctx->hwndFrame;
    data->providerId = ctx->providerId;
    data->sessionId = ctx->sessionId ? str::Dup(ctx->sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnAIChatUpdate, data));
}

// record a session id the provider assigned mid-stream
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId) {
    AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, sessionId);
    str::ReplaceWithCopy(&ctx->sessionId, sessionId);
}

// --- Reader thread ---

struct AIChatReadThreadCtx {
    HANDLE hReadPipe = nullptr;
    AIChatStreamCtx stream;
};

static void AIChatReadThread(AIChatReadThreadCtx* ctx) {
    HANDLE hPipe = ctx->hReadPipe;
    AIChatProvider* p = GetAIChatProvider(ctx->stream.providerId);

    // Most SSE/provider lines are well under 4KB; grow to heap for rare large lines.
    char lineScratch[4096]{};
    str::Builder lineBuf;
    str::BuilderUseExternalBuffer(lineBuf, Str(lineScratch, sizeofi(lineScratch)));
    constexpr int kMaxProviderLineSize = 1024 * 1024;
    bool lineTooLong = false;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
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
    }
    AIChatLog(p->logger, StrL("eof"), StrL("(stdout closed)"));

    CloseHandle(hPipe);
    AIChatPostUpdate(&ctx->stream, AIChatUpdateType::Finished, {});
    str::Free(ctx->stream.sessionId);
    delete ctx;
}

static void StartAIChatReadThread(AIChatReadThreadCtx* ctx) {
    AIChatReadThread(ctx);
}

// --- Headless chat runner (for -dbg-control tests) ---

static void AIChatReadAllPipe(HANDLE hPipe, str::Builder& out) {
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(hPipe, buf, sizeof(buf), &n, nullptr) && n > 0) {
        out.Append(Str(buf, (int)n));
    }
}

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
    AIChatReadAllPipe(launch.hReadPipe, raw);
    CloseHandle(launch.hReadPipe);
    launch.hReadPipe = nullptr;
    DWORD waitRes = WaitForSingleObject(launch.hProcess, 5 * 60 * 1000);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(launch.hProcess, 1);
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
    Str out = ToStr(raw);
    int off = 0;
    while (off < out.len) {
        int start = off;
        while (off < out.len && out.s[off] != '\n' && out.s[off] != '\r') {
            off++;
        }
        if (off > start) {
            TempStr line = str::DupTemp(Str(out.s + start, off - start));
            AIChatLog(p->logger, StrL("<<<"), line);
            p->ParseStreamLine(line, &ctx);
        }
        while (off < out.len && (out.s[off] == '\n' || out.s[off] == '\r')) {
            off++;
        }
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

// --- Sending a message ---

static void SendAIChatMessage(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!p || !win->aiChatInput) {
        return;
    }
    if (!IsAIChatSupportedForTab(win->CurrentTab())) {
        return;
    }
    HWND hwndInput = win->aiChatInput->hwnd;
    int inputLen = HwndGetTextLen(hwndInput);
    if (inputLen == 0) {
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

    TempWStr inputW = HwndGetTextWTemp(hwndInput);
    TempStr input = ToUtf8Temp(inputW);
    HwndSetText(hwndInput, StrL(""));

    WebViewAddUser(win, input);
    SetAIChatWorking(win, true);

    bool isNewSession = len(st->sessionId) == 0;
    if (isNewSession && p->generatesSessionId) {
        str::ReplaceWithCopy(&st->sessionId, AIChatGenerateSessionIdTemp());
    }

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);

    // sync and save settings from UI
    SyncAIChatSettingsFromUI(win);

    StrVec models;
    p->BuildModelsList(models);
    Str model = AIChatResolveModel(models, p->GetModel(), p->defaultModel);
    int optionIdx = p->GetOption();
    if (optionIdx < 0 || optionIdx >= p->optionCount) {
        optionIdx = p->optionDefault;
    }

    TempStr exePath = p->FindExecutableTemp();
    if (len(exePath) == 0) {
        AIChatLog(p->logger, StrL("error"), fmt("Cannot find %s executable", p->exeName));
        WebViewAddError(win, fmt("Cannot find %s. Is %s installed?", p->exeName, p->name));
        SetAIChatWorking(win, false);
        return;
    }

    AIChatLog(p->logger, StrL(">>> user"), input);
    AIChatLog(p->logger, StrL(">>> session"),
              fmt("%s (%s)", st->sessionId ? st->sessionId : kAIChatPendingSessionId(),
                  Str(isNewSession ? "new" : "resume")));
    AIChatLog(p->logger, StrL(">>> cwd"), dir);

    AIChatCmdArgs args;
    args.exePath = exePath;
    args.model = model;
    args.sessionId = st->sessionId;
    args.filePath = filePath;
    args.dir = dir;
    // Raw user text; providers quote with QuoteCmdLineArgTemp when building the
    // CreateProcessW command line (naive " -> \" is not enough on Windows).
    args.escapedInput = input;
    args.option = optionIdx;
    args.flag = p->GetFlag();
    args.isNewSession = isNewSession;
    TempStr cmdLine = p->BuildCmdLineTemp(args);

    AIChatLog(p->logger, StrL(">>> cmd"), cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        AIChatLog(p->logger, StrL("error"), fmt("Failed to launch %s process", p->exeName));
        WebViewAddError(win, fmt("Failed to launch %s. Is it installed and in PATH?", p->exeName));
        SetAIChatWorking(win, false);
        return;
    }

    st->process = launch.hProcess;
    AIChatLog(p->logger, StrL(">>> start"), fmt("pid %lu", launch.processId));

    auto* ctx = new AIChatReadThreadCtx();
    ctx->hReadPipe = launch.hReadPipe;
    ctx->stream.hwndFrame = win->hwndFrame;
    ctx->stream.providerId = win->aiChatProvider;
    ctx->stream.sessionId = str::Dup(st->sessionId ? st->sessionId : kAIChatPendingSessionId());
    RunAsync(MkFunc0(StartAIChatReadThread, ctx), StrL("AIChatReadThread"));
}

// --- WndProcs ---

static LRESULT CALLBACK WndProcAIChatInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*idSubclass*/,
                                           DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !IsShiftPressed()) {
        SendAIChatMessage(win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void CloseAIChatPanelFromLabel(MainWindow* win);

static LRESULT CALLBACK WndProcAIChatBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*idSubclass*/,
                                         DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    // the panel header (label + close button) is a virtual control tree, so
    // this window paints it (background included) and hands it its input
    if (VirtHostOnMessage(hwnd, win->aiChatRoot, msg, wp, lp, res, ThemeControlBackgroundColor())) {
        return res;
    }

    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wp;
            HdcFillRect(hdc, HwndClientRect(hwnd), win->brControlBgColor);
            return TRUE;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)win->brControlBgColor;
        }
        case WM_SIZE:
            LayoutAIChatBox(win);
            break;
        case WM_TIMER:
            if (wp == kTimerAutoSelectSession) {
                KillTimer(hwnd, kTimerAutoSelectSession);
                AutoSelectRecentSession(win);
                PopulateSessionCombo(win);
            } else if (wp == kTimerWebViewSize) {
                KillTimer(hwnd, kTimerWebViewSize);
                if (win->aiChatWebView) {
                    win->aiChatWebView->UpdateWebviewSize();
                }
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// --- Splitter ---
constexpr int kAIChatMinDx = 150;

static void OnAIChatSplitterMove(VirtSplitter::MoveEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->GetHwnd());
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = HwndClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    if (dx < kAIChatMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    if (ev->queryOnly) {
        return;
    }
    AIChatUpdateSidebarDx(win, dx, ev->finishedDragging);
    if (ev->finishedDragging) {
        ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
    }
}

// called from SumatraPDF.cpp for width change relayout
// reposition children and repaint after the container is moved/resized
void RelayoutAIChatPanel(MainWindow* win) {
    if (!win || !win->hwndAiChatBox || !win->uiState.aiChatVisible) {
        return;
    }
    LayoutAIChatBox(win);
    KillTimer(win->hwndAiChatBox, kTimerWebViewSize);
    if (win->aiChatWebView && win->aiChatWebViewReady) {
        win->aiChatWebView->UpdateWebviewSize();
    }
    RedrawWindow(win->hwndAiChatBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (win->aiChatSplitter) {
        win->aiChatSplitter->Invalidate();
    }
}

// --- Lazy WebView2 init ---

static void DeleteAIChatWebView(MainWindow* win) {
    delete win->aiChatWebView;
    win->aiChatWebView = nullptr;
    win->aiChatWebViewReady = false;
}

// WebView2 loads the chat HTML asynchronously, so appendText()/marked and the
// rest of the page's JS don't exist until the navigation finishes. Anything the
// app evals before then is lost (it only lands in st->chatLog). This fires when
// the page is actually loaded, so we (re)render the current tab's chat into it -
// replacing the old "wait 600ms and hope" timer.
static void OnAIChatWebViewNavigated(void* ctx, Str, bool) {
    MainWindow* win = (MainWindow*)ctx;
    if (!IsMainWindowValidAndNotClosing(win) || !win->hwndAiChatBox) {
        return;
    }
    if (win->aiChatWebView) {
        // the webview is created lazily, often before the panel has its final
        // size, leaving its window (and so the WebView2 controller) at 0x0. Now
        // that the page has loaded and the panel is laid out, re-run the layout
        // to move/size the webview into its slot and make its controller visible.
        win->aiChatWebView->SetControllerVisible(true);
        RelayoutAIChatPanel(win);
    }
    OnAIChatTabChanged(win);
}

static void EnsureWebViewReady(MainWindow* win) {
    if (win->aiChatWebViewReady) {
        return;
    }
    if (!HasWebView()) {
        return;
    }
    AIChatProvider* p = CurrentProvider(win);
    if (!p) {
        return;
    }
    auto* webView = new WebviewWnd();
    webView->events.ctx = win;
    webView->events.navigationCompleted = OnAIChatWebViewNavigated;
    TempStr localAppData = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA);
    // use unique data dir per process to avoid locking conflicts
    webView->dataDir =
        str::Dup(fmt("%s\\SumatraPDF\\%s_%d", localAppData, p->webViewDataDirPrefix, (int)GetCurrentProcessId()));
    int markedLen = 0;
    u8* markedData = GetEmbeddedFileData(StrL("marked.min.js"), &markedLen);
    if (!markedData || markedLen <= 0) {
        free(markedData);
        delete webView;
        return;
    }
    wstr::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = wstr::Dup(p->virtualHostW);
    // serve both the chat page and marked.min.js from the virtual host
    free(gAIChatWebResources.marked);
    gAIChatWebResources.marked = markedData;
    gAIChatWebResources.markedLen = markedLen;
    str::ReplaceWithCopy(&gAIChatWebResources.html, AIChatFormatChatHtmlTemp(p->virtualHost, BgColorForProvider(p)));
    webView->resourceProvider.ctx = &gAIChatWebResources;
    webView->resourceProvider.getResource = AIChatGetResource;

    Rect rc = HwndClientRect(win->hwndAiChatBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndAiChatBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr url = fmt("%sindex.html", p->virtualHost);
        webView->Navigate(url);
        // make the webview visible, like the manual browser does; without this
        // the embedded controller stays hidden (isVisible defaults to false) and
        // the loaded page never paints
        webView->SetIsVisible(true);
        win->aiChatWebView = webView;
        win->aiChatWebViewReady = true;
        RelayoutAIChatPanel(win);
    } else {
        delete webView;
    }
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
    if (!p) {
        return;
    }
    win->aiChatProvider = providerId;
    if (win->aiChatCheckbox) {
        HwndSetText(win->aiChatCheckbox->hwnd, p->checkboxLabel);
    }
    if (win->aiChatOptionCombo) {
        win->aiChatOptionCombo->SetItemsSeqStrings(p->optionItems);
    }
    ApplyAIChatSettingsToUI(win);
    UpdateAIChatPanelTitle(win, 0);
    DeleteAIChatWebView(win);
}

// --- Theme ---

// apply theme colors to the panel's native controls and, since the chat
// colors are baked into the WebView's html, recreate the WebView (the chat
// log is replayed into it once the new page has loaded)
void UpdateAIChatTheme(MainWindow* win) {
    if (!win || !win->hwndAiChatBox) {
        return;
    }
    Color bgCol = ThemeControlBackgroundColor();
    Color txtCol = ThemeWindowTextColor();
    if (win->aiChatInput) {
        win->aiChatInput->SetColors(txtCol, bgCol);
    }
    if (win->aiChatCheckbox) {
        win->aiChatCheckbox->SetColors(txtCol, bgCol);
    }
    // the panel is created after the frame-wide dark mode pass, so its
    // controls (e.g. the checkbox) need their own subclass + theme pass
    DarkModeApplyToChildControls(win->hwndAiChatBox);
    if (win->aiChatWebView) {
        DeleteAIChatWebView(win);
        if (win->uiState.aiChatVisible) {
            EnsureWebViewReady(win);
            // the new webview's navigationCompleted callback replays the chat
            // once its page has loaded
        }
    }
    RedrawWindow(win->hwndAiChatBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// --- Public API ---

void CreateAIChatPanel(MainWindow* win) {
    if (!IsAIChatAvailable()) {
        return;
    }
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gSettings->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndAiChatBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    // the splitter is part of the frame's content row and outlives the panel;
    // we only take it over while the panel exists
    if (win->aiChatSplitter) {
        win->aiChatSplitter->SetIsVisible(false);
        win->aiChatSplitter->onMove = MkFunc1Void(OnAIChatSplitterMove);
    }

    PlatformFont* font = GetAppFont();

    // label
    PlatformFont* labelFont = GetAppSidebarLabelFont();
    auto header = NewLabelWithClose(win->hwndAiChatBox, labelFont, MkFunc0(CloseAIChatPanelFromLabel, win));
    win->aiChatLabel = header.label;
    win->aiChatHeader = header.box;

    // session combo
    {
        DropDown::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.font = font;
        args.isRtl = IsUIRtl();
        win->aiChatSessionCombo = new DropDown();
        win->aiChatSessionCombo->Create(args);
        win->aiChatSessionCombo->onSelectionChanged = MkFunc0(OnSessionComboChange, win);
    }

    // webview deferred
    win->aiChatWebView = nullptr;
    win->aiChatWebViewReady = false;

    // model combo
    {
        DropDown::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.font = font;
        args.isRtl = IsUIRtl();
        win->aiChatModelCombo = new DropDown();
        win->aiChatModelCombo->Create(args);
    }

    // effort / sandbox combo
    {
        DropDown::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.font = font;
        args.isRtl = IsUIRtl();
        win->aiChatOptionCombo = new DropDown();
        win->aiChatOptionCombo->Create(args);
    }

    // skip-permissions / always-approve / skip-sandbox checkbox
    {
        Checkbox::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.text = StrL("Skip Permissions");
        args.font = font;
        args.isRtl = IsUIRtl();
        win->aiChatCheckbox = new Checkbox();
        win->aiChatCheckbox->Create(args);
    }

    // stop button (hidden by default, shown when agent is working)
    {
        auto* b = NewThemedButton(win->hwndAiChatBox, StrL("Stop"), font, false);
        b->onClick = MkFunc0(StopAIChat, win);
        b->SetIsVisible(false);
        win->aiChatStopBtn = b;
    }

    // input box
    {
        Edit::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.isMultiLine = true;
        args.idealSizeLines = 3;
        args.withBorder = true;
        args.cueText = StrL("Ask about this document...");
        args.font = font;
        win->aiChatInput = new Edit();
        win->aiChatInput->Create(args);
    }

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(win->aiChatInput->hwnd, WndProcAIChatInput, inputSubclassId, (DWORD_PTR)win);

    win->aiChatBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndAiChatBox, WndProcAIChatBox, win->aiChatBoxSubclassId, (DWORD_PTR)win);

    // layout: label, session combo, webview area (flex), input row, options row
    {
        auto* inputRow = new HBox();
        inputRow->alignCross = CrossAxisAlign::Stretch;
        inputRow->AddChild(win->aiChatInput, 1);
        inputRow->AddChild(win->aiChatStopBtn);

        auto* optionsRow = new HBox();
        optionsRow->alignCross = CrossAxisAlign::CrossCenter;
        optionsRow->AddChild(win->aiChatModelCombo, 1);
        optionsRow->AddChild(new Spacer(2, 0));
        optionsRow->AddChild(win->aiChatOptionCombo, 1);
        optionsRow->AddChild(new Spacer(8, 0));
        optionsRow->AddChild(win->aiChatCheckbox, 1);

        auto* vbox = new VBox();
        vbox->alignCross = CrossAxisAlign::Stretch;
        vbox->AddChild(win->aiChatHeader);
        vbox->AddChild(win->aiChatSessionCombo);
        win->aiChatWebViewSlot = new Spacer(0, 0);
        vbox->AddChild(win->aiChatWebViewSlot, 1);
        vbox->AddChild(inputRow);
        vbox->AddChild(new Spacer(0, 4));
        vbox->AddChild(optionsRow);
        win->aiChatLayout = vbox;
    }

    // initialize provider-specific parts (default: Claude)
    win->aiChatProvider = -1;
    SetPanelProvider(win, 0);

    AIChatApplySavedSidebarDx(win);
    UpdateAIChatTheme(win);
}

void UpdateAIChatDpi(MainWindow* win, int dpi) {
    if (!win || !win->hwndAiChatBox || dpi <= 0) {
        return;
    }
    PlatformFont* font = GetAppFontForDpi(dpi);
    PlatformFont* labelFont = GetAppSidebarLabelFontForDpi(dpi);
    win->aiChatLabel->font = labelFont;
    win->aiChatSessionCombo->SetFont(font);
    win->aiChatModelCombo->SetFont(font);
    win->aiChatOptionCombo->SetFont(font);
    win->aiChatCheckbox->SetFont(font);
    win->aiChatInput->SetFont(font);
    win->aiChatStopBtn->font = font;
    int padY = DpiScaleByDpi(dpi, 5);
    int padX = DpiScaleByDpi(dpi, 12);
    win->aiChatStopBtn->textPadding = Insets{padY, padX, padY, padX};
    if (win->aiChatHeader && win->aiChatHeader->ChildrenCount() > 1) {
        ApplyLabelWithCloseDpi(win->aiChatLabel, AsVirtCloseButton(win->aiChatHeader->LayoutChildAt(1)), dpi);
    }
    RelayoutAIChatPanel(win);
    HwndInvalidate(win->hwndAiChatBox, true);
}

// close the panel for the current tab (label's close button)
static void CloseAIChatPanelFromLabel(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    AIChatSyncPanelsToCurrentTab(win);
    ScheduleUiUpdate(win);
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
        logf("OnAIChatToggle: IsAIChatAvailable() returned false (HasWebView=false)\n");
        return;
    }
    if (!p->IsInstalled()) {
        logf("OnAIChatToggle: provider %s is not installed (exePath=%s)\n", p->name, p->FindExecutableTemp());
        AIChatNotInstalledDialogArgs args;
        args.windowTitle = p->TitleTemp();
        args.mainInstruction = p->NotInstalledInstructionTemp();
        args.docUri = p->docUri;
        AIChatShowNotInstalledDialog(args);
        return;
    }
    if (!win->hwndAiChatBox) {
        logf("OnAIChatToggle: win->hwndAiChatBox is null\n");
        return;
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
            logf("OnAIChatToggle: IsAIChatSupportedForTab returned false for tab (filePath=%s)\n", tab->filePath);
            return;
        }
        logf("OnAIChatToggle: opening panel for backend %d\n", (int)p->backend);
        AIChatSetTabPanelOpen(tab, p->backend);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->uiState.aiChatVisible) {
        SetPanelProvider(win, providerId);
        UpdateAIChatPanelTitle(win, 0);
        EnsureWebViewReady(win);
        UpdateAIChatPanelForCurrentTab(win);
        PopulateSessionCombo(win);
        EditSetFocus(win->aiChatInput);
        // if the webview was recreated (provider change), its navigationCompleted
        // callback replays the chat once the new page has loaded
        // defer auto-select so SetHtml has time to load the page
        SetTimer(win->hwndAiChatBox, kTimerAutoSelectSession, 500, nullptr);
    }
    ScheduleUiUpdate(win);
}

// call when switching tabs to update session context
void OnAIChatTabChanged(MainWindow* win) {
    if (!win || !win->hwndAiChatBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();

    // the tab we switched to may have a different provider's panel open
    AIChatBackend open = AIChatGetTabPanelOpen(tab);
    if (win->uiState.aiChatVisible && open != AIChatBackend::None && (int)open != win->aiChatProvider) {
        SetPanelProvider(win, (int)open);
        EnsureWebViewReady(win);
    }

    UpdateAIChatPanelTitle(win, 0);
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
        AIChatProvider* p = CurrentProvider(win);
        TempStr dir = path::GetDirTemp(tab->filePath);
        p->LoadSessionHistory(win, st->sessionId, dir);
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
    win->aiChatWebViewReady = false;

    if (win->hwndAiChatBox) {
        KillTimer(win->hwndAiChatBox, kTimerAutoSelectSession);
        KillTimer(win->hwndAiChatBox, kTimerWebViewSize);
        if (win->aiChatBoxSubclassId) {
            RemoveWindowSubclass(win->hwndAiChatBox, WndProcAIChatBox, win->aiChatBoxSubclassId);
            win->aiChatBoxSubclassId = 0;
        }
    }

    // save webview dataDir before deleting so we can clean up
    Str webViewDataDir;
    WebviewWnd* webView = win->aiChatWebView;
    win->aiChatWebView = nullptr;
    if (webView) {
        webViewDataDir = str::Dup(webView->dataDir);
    }
    delete webView;

    // deleting the layout deletes the controls in it
    delete win->aiChatLayout;
    win->aiChatLayout = nullptr;
    // the layout owns the virtual controls the root points at, so it goes first
    delete win->aiChatRoot;
    win->aiChatRoot = nullptr;
    win->aiChatHeader = nullptr;
    win->aiChatWebViewSlot = nullptr;
    win->aiChatLabel = nullptr;
    win->aiChatSessionCombo = nullptr;
    win->aiChatModelCombo = nullptr;
    win->aiChatOptionCombo = nullptr;
    win->aiChatCheckbox = nullptr;
    win->aiChatStopBtn = nullptr;
    win->aiChatInput = nullptr;

    if (win->aiChatSplitter) {
        // owned by the frame's content row, so just park it
        win->aiChatSplitter->SetIsVisible(false);
        win->aiChatSplitter->onMove = {};
    }

    if (win->hwndAiChatBox) {
        DestroyWindow(win->hwndAiChatBox);
        win->hwndAiChatBox = nullptr;
    }

    // clean up per-process WebView2 cache dir
    if (webViewDataDir) {
        dir::RemoveAll(webViewDataDir);
        str::Free(webViewDataDir);
    }
}
