/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the AI chat sidebar: a single panel shared by all providers (Claude Code,
// Grok Build, OpenAI Codex). Everything backend-specific comes from
// AIChatProvider (see AIChatCommon.h); this file owns the UI and the
// process/stream plumbing.

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "resource.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

#define IDC_AICHAT_LABEL_WITH_CLOSE 1110

// timer ids on hwndAiChatBox
constexpr UINT_PTR kTimerAutoSelectSession = 42;
constexpr UINT_PTR kTimerWebViewSize = 43;
constexpr UINT_PTR kTimerReplayChat = 44;

static LoadedDataResource gAIChatMarkedJs;

AIChatProvider* GetAIChatProvider(int providerId) {
    switch (providerId) {
        case 0:
            return GetClaudeCodeProvider();
        case 1:
            return GetGrokBuildProvider();
        case 2:
            return GetCodexBuildProvider();
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
        AIChatLog(p->logger, "error", text);
    }
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewFlushBlock(MainWindow* win) {
    WebViewEval(win, "flushBlock()");
}

static void WebViewClearChat(MainWindow* win) {
    WebViewEval(win, "clearChat()", false); // don't record clear
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
    if (!st || st->chatLog.IsEmpty()) {
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
    Str docName = "document";
    WindowTab* tab = win->CurrentTab();
    if (tab && !tab->IsAboutTab() && tab->filePath) {
        Str title = tab->GetTabTitle();
        if (len(title) > 0) {
            docName = title;
        }
    }

    HWND labelHwnd = win->aiChatLabel->hwnd;
    HFONT font = win->aiChatLabel->font;
    if (!font) {
        font = GetDefaultGuiFont(true, false);
    }
    if (labelDx <= 0 && win->hwndAiChatBox) {
        labelDx = ClientRect(win->hwndAiChatBox).dx;
    }
    int maxDx = AIChatLabelMaxTextDx(labelHwnd, labelDx);
    TempStr prefix = str::JoinTemp(p->TitleTemp(), StrL(" with "));
    TempStr label = AIChatFitPanelTitleTemp(labelHwnd, font, prefix, docName, maxDx);
    win->aiChatLabel->SetLabel(label);
}

// --- Layout ---

static void LayoutAIChatBox(MainWindow* win) {
    if (!win->aiChatLayout) {
        return;
    }
    Rect rc = ClientRect(win->hwndAiChatBox);
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }

    UpdateAIChatPanelTitle(win, rc.dx);
    LayoutToSize(win->aiChatLayout, {rc.dx, rc.dy});

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
    items.Append("+ New Session");

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || !tab->filePath || !st) {
        win->aiChatSessionCombo->SetItems(items);
        win->aiChatSessionCombo->SetCurrentSelection(0);
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
            display = "(no description)";
        }
        items.Append(ShortenStringUtf8Temp(display, 50));
        if (st->sessionId && str::Eq(st->sessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (st->sessionId && !foundCurrent) {
        items.Append("(current session)");
        selectedIdx = len(sessions) + 1;
    }

    win->aiChatSessionCombo->SetItems(items);
    win->aiChatSessionCombo->SetCurrentSelection(selectedIdx);
    AIChatFreeSessions(sessions);
}

static void OnSessionComboChange(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    if (!p) {
        return;
    }
    int sel = win->aiChatSessionCombo->GetCurrentSelection();

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || !tab->filePath || !st) {
        return;
    }

    if (sel == 0) {
        // "New Session" — clear current session
        AIChatLog(p->logger, "session", "new");
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
        AIChatLog(p->logger, "session", sessions[sessionIdx].sessionId);
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
    if (!p || !st || !tab->filePath || st->sessionId) {
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
        items.Append(AIChatModelDisplayNameTemp(models[i], nullptr));
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
        int modelIdx = AIChatFindModelInList(models, model);
        if (modelIdx < 0) {
            modelIdx = 0;
        }
        win->aiChatModelCombo->SetCurrentSelection(modelIdx);
    }
    if (win->aiChatOptionCombo) {
        int optionIdx = p->GetOption();
        if (optionIdx < 0 || optionIdx >= p->optionCount) {
            optionIdx = p->optionDefault;
        }
        win->aiChatOptionCombo->SetCurrentSelection(optionIdx);
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
        int sel = win->aiChatModelCombo->GetCurrentSelection();
        StrVec models;
        p->BuildModelsList(models);
        if (sel >= 0 && sel < len(models)) {
            p->SetModel(models[sel]);
        }
    }
    if (win->aiChatOptionCombo) {
        p->SetOption(win->aiChatOptionCombo->GetCurrentSelection());
    }
    if (win->aiChatCheckbox) {
        p->SetFlag(win->aiChatCheckbox->IsChecked());
    }
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    SaveSettings();
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
        win->aiChatStopBtn->SetVisibility(working ? Visibility::Visible : Visibility::Collapse);
        EnableWindow(win->aiChatStopBtn->hwnd, working);
    }
    LayoutAIChatBox(win);
}

static void SetAIChatWorking(MainWindow* win, bool) {
    UpdateAIChatPanelForCurrentTab(win);
}

static void StopAIChat(MainWindow* win) {
    AIChatProvider* p = CurrentProvider(win);
    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (p && st && st->process) {
        AIChatLog(p->logger, "stop", st->sessionId ? st->sessionId : StrL("(no session)"));
        AIChatCloseProcess(&st->process, true);
        WebViewAddError(win, "Stopped by user.");
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

static void OnAIChatUpdate(AIChatUpdateData* data) {
    MainWindow* win = AIChatFindMainWindowByFrame(data->hwndFrame);
    int pid = data->providerId;
    AIChatProvider* p = GetAIChatProvider(pid);
    if (!win || !IsMainWindowValid(win) || !win->hwndAiChatBox || !p) {
        str::Free(data->text);
        str::Free(data->sessionId);
        delete data;
        return;
    }
    {
        // find the tab this update belongs to; prefer tabs with a running process
        WindowTab* tab = nullptr;
        for (WindowTab* t : win->Tabs()) {
            AIChatTabState* st = GetTabState(t, pid);
            if (!st || !st->process) {
                continue;
            }
            if (data->sessionId && st->sessionId && str::Eq(st->sessionId, data->sessionId)) {
                tab = t;
                break;
            }
            if (data->sessionId && str::Eq(data->sessionId, kAIChatPendingSessionId()) && !st->sessionId) {
                tab = t;
                break;
            }
        }
        if (!tab) {
            for (WindowTab* t : win->Tabs()) {
                AIChatTabState* st = GetTabState(t, pid);
                if (st && st->sessionId && data->sessionId && str::Eq(st->sessionId, data->sessionId)) {
                    tab = t;
                    break;
                }
            }
        }
        // only update the WebView if the panel currently shows this
        // provider and this tab
        bool isActiveTab = tab && tab == win->CurrentTab() && win->aiChatProvider == pid;
        AIChatTabState* st = GetTabState(tab, pid);

        switch (data->updateType) {
            case AIChatUpdateType::Text:
                if (data->text) {
                    AIChatLog(p->logger, "<<< text", data->text);
                }
                if (isActiveTab) {
                    WebViewAppendText(win, data->text);
                }
                break;
            case AIChatUpdateType::Tool:
                if (data->text) {
                    AIChatLog(p->logger, "<<< tool", data->text);
                }
                if (isActiveTab) {
                    WebViewAddTool(win, data->text);
                }
                break;
            case AIChatUpdateType::Error:
                if (isActiveTab) {
                    WebViewAddError(win, data->text);
                } else if (data->text) {
                    AIChatLog(p->logger, "error", data->text);
                }
                break;
            case AIChatUpdateType::Flush:
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                }
                break;
            case AIChatUpdateType::SessionId:
                if (data->text) {
                    AIChatLog(p->logger, "<<< session", data->text);
                }
                if (st && data->text) {
                    str::ReplaceWithCopy(&st->sessionId, data->text);
                }
                break;
            case AIChatUpdateType::Finished:
                if (st && st->process) {
                    if (WaitForSingleObject(st->process, 0) == WAIT_OBJECT_0) {
                        DWORD exitCode = 0;
                        GetExitCodeProcess(st->process, &exitCode);
                        AIChatLog(p->logger, "exit", fmt("%lu", exitCode));
                    }
                    AIChatCloseProcess(&st->process, p->terminateOnFinish);
                }
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                    SetAIChatWorking(win, false);
                    PopulateSessionCombo(win);
                }
                break;
        }
    }
    str::Free(data->text);
    str::Free(data->sessionId);
    delete data;
}

void AIChatPostUpdate(AIChatStreamCtx* ctx, AIChatUpdateType type, Str text) {
    auto data = new AIChatUpdateData();
    data->hwndFrame = ctx->hwndFrame;
    data->providerId = ctx->providerId;
    data->sessionId = ctx->sessionId ? str::Dup(ctx->sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnAIChatUpdate, data));
}

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

    str::Builder lineBuf;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                Str line = ToStr(lineBuf);
                if (line) {
                    AIChatLog(p->logger, "<<<", line);
                }
                p->ParseStreamLine(line, &ctx->stream);
                lineBuf.Reset();
            } else if (buf[i] != '\r') {
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    Str rem = ToStr(lineBuf);
    if (rem) {
        AIChatLog(p->logger, "<<<", rem);
    }
    AIChatLog(p->logger, "eof", "(stdout closed)");

    CloseHandle(hPipe);
    AIChatPostUpdate(&ctx->stream, AIChatUpdateType::Finished, {});
    str::Free(ctx->stream.sessionId);
    delete ctx;
}

static void StartAIChatReadThread(AIChatReadThreadCtx* ctx) {
    AIChatReadThread(ctx);
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
    int inputLen = GetWindowTextLengthW(hwndInput);
    if (inputLen == 0) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    AIChatTabState* st = GetTabState(tab, win->aiChatProvider);
    if (!tab || !tab->filePath || !st) {
        return;
    }
    if (st->process) {
        return; // this tab already has a running request
    }

    TempWStr inputW = HwndGetTextWTemp(hwndInput);
    TempStr input = ToUtf8Temp(inputW);
    HwndSetText(hwndInput, "");

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
    if (!exePath) {
        AIChatLog(p->logger, "error", fmt("Cannot find %s executable", p->exeName));
        WebViewAddError(win, fmt("Cannot find %s. Is %s installed?", p->exeName, p->name));
        SetAIChatWorking(win, false);
        return;
    }

    AIChatLog(p->logger, ">>> user", input);
    AIChatLog(p->logger, ">>> session",
              fmt("%s (%s)", st->sessionId ? st->sessionId : kAIChatPendingSessionId(),
                  Str(isNewSession ? "new" : "resume")));
    AIChatLog(p->logger, ">>> cwd", dir);

    AIChatCmdArgs args;
    args.exePath = exePath;
    args.model = model;
    args.sessionId = st->sessionId;
    args.filePath = filePath;
    args.dir = dir;
    args.escapedInput = str::ReplaceTemp(input, StrL("\""), StrL("\\\""));
    args.option = optionIdx;
    args.flag = p->GetFlag();
    args.isNewSession = isNewSession;
    TempStr cmdLine = p->BuildCmdLineTemp(args);

    AIChatLog(p->logger, ">>> cmd", cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        AIChatLog(p->logger, "error", fmt("Failed to launch %s process", p->exeName));
        WebViewAddError(win, fmt("Failed to launch %s. Is it installed and in PATH?", p->exeName));
        SetAIChatWorking(win, false);
        return;
    }

    st->process = launch.hProcess;
    AIChatLog(p->logger, ">>> start", fmt("pid %lu", launch.processId));

    auto ctx = new AIChatReadThreadCtx();
    ctx->hReadPipe = launch.hReadPipe;
    ctx->stream.hwndFrame = win->hwndFrame;
    ctx->stream.providerId = win->aiChatProvider;
    ctx->stream.sessionId = str::Dup(st->sessionId ? st->sessionId : kAIChatPendingSessionId());
    RunAsync(MkFunc0(StartAIChatReadThread, ctx), "AIChatReadThread");
}

// --- WndProcs ---

static LRESULT CALLBACK WndProcAIChatInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !IsShiftPressed()) {
        SendAIChatMessage(win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void CloseAIChatPanelFromLabel(MainWindow* win);

static LRESULT CALLBACK WndProcAIChatBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, win->brControlBgColor);
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
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_AICHAT_LABEL_WITH_CLOSE) {
                CloseAIChatPanelFromLabel(win);
            }
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
            } else if (wp == kTimerReplayChat) {
                // WebView recreated (theme or provider change): restore the chat
                KillTimer(hwnd, kTimerReplayChat);
                OnAIChatTabChanged(win);
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// --- Splitter ---
constexpr int kAIChatMinDx = 150;

static void OnAIChatSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = ClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    if (dx < kAIChatMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    AIChatUpdateSidebarDx(win, dx, ev->finishedDragging);
    if (ev->finishedDragging) {
        ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
    }
}

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
    if (win->aiChatSplitter && win->aiChatSplitter->hwnd) {
        InvalidateRect(win->aiChatSplitter->hwnd, nullptr, TRUE);
    }
}

// --- Lazy WebView2 init ---

static void DeleteAIChatWebView(MainWindow* win) {
    delete win->aiChatWebView;
    win->aiChatWebView = nullptr;
    win->aiChatWebViewReady = false;
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
    auto webView = new WebviewWnd();
    TempStr localAppData = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA);
    // use unique data dir per process to avoid locking conflicts
    webView->dataDir =
        str::Dup(fmt("%s\\SumatraPDF\\%s_%d", localAppData, p->webViewDataDirPrefix, (int)GetCurrentProcessId()));
    if (!LockDataResource(IDR_CLAUDE_MARKED_JS, &gAIChatMarkedJs)) {
        delete webView;
        return;
    }
    wstr::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = wstr::Dup(p->virtualHostW);
    webView->resourceProvider.ctx = &gAIChatMarkedJs;
    webView->resourceProvider.getResource = AIChatGetMarkedJsResource;

    Rect rc = ClientRect(win->hwndAiChatBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndAiChatBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr chatHtml = AIChatFormatChatHtmlTemp(p->virtualHost, BgColorForProvider(p));
        webView->SetHtml(chatHtml);
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
    COLORREF bgCol = ThemeControlBackgroundColor();
    COLORREF txtCol = ThemeWindowTextColor();
    if (win->aiChatLabel) {
        win->aiChatLabel->SetColors(txtCol, bgCol);
    }
    if (win->aiChatInput) {
        win->aiChatInput->SetColors(txtCol, bgCol);
    }
    // the panel is created after the frame-wide dark mode pass, so its
    // controls (e.g. the checkbox) need their own subclass + theme pass
    if (UseDarkModeLib() && !IsCurrentThemeDefault()) {
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndAiChatBox);
    }
    if (win->aiChatWebView) {
        DeleteAIChatWebView(win);
        if (win->uiState.aiChatVisible) {
            EnsureWebViewReady(win);
            // replay the chat once the new page has had time to load
            SetTimer(win->hwndAiChatBox, kTimerReplayChat, 600, nullptr);
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
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndAiChatBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    // splitter (non-live: only resize on mouse release)
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->aiChatSplitter = new Splitter();
        win->aiChatSplitter->onMove = MkFunc1Void(OnAIChatSplitterMove);
        win->aiChatSplitter->Create(args);
    }

    HFONT font = GetDefaultGuiFont();

    // label
    auto label = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.cmdId = IDC_AICHAT_LABEL_WITH_CLOSE;
        args.isRtl = IsUIRtl();
        args.font = GetDefaultGuiFont(true, false);
        label->Create(args);
    }
    win->aiChatLabel = label;
    label->SetPaddingXY(2, 2);

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
        args.text = "Skip Permissions";
        args.isRtl = IsUIRtl();
        win->aiChatCheckbox = new Checkbox();
        win->aiChatCheckbox->Create(args);
    }

    // stop button (hidden by default, shown when agent is working)
    {
        Button::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.text = "Stop";
        args.font = font;
        args.isRtl = IsUIRtl();
        win->aiChatStopBtn = new Button();
        win->aiChatStopBtn->Create(args);
        win->aiChatStopBtn->onClick = MkFunc0(StopAIChat, win);
        win->aiChatStopBtn->SetVisibility(Visibility::Collapse);
    }

    // input box
    {
        Edit::CreateArgs args;
        args.parent = win->hwndAiChatBox;
        args.isMultiLine = true;
        args.idealSizeLines = 3;
        args.withBorder = true;
        args.cueText = "Ask about this document...";
        win->aiChatInput = new Edit();
        win->aiChatInput->Create(args);
    }

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(win->aiChatInput->hwnd, WndProcAIChatInput, inputSubclassId, (DWORD_PTR)win);

    win->aiChatBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndAiChatBox, WndProcAIChatBox, win->aiChatBoxSubclassId, (DWORD_PTR)win);

    // layout: label, session combo, webview area (flex), input row, options row
    {
        auto inputRow = new HBox();
        inputRow->alignCross = CrossAxisAlign::Stretch;
        inputRow->AddChild(win->aiChatInput, 1);
        inputRow->AddChild(win->aiChatStopBtn);

        auto optionsRow = new HBox();
        optionsRow->alignCross = CrossAxisAlign::CrossCenter;
        optionsRow->AddChild(win->aiChatModelCombo, 1);
        optionsRow->AddChild(new Spacer(2, 0));
        optionsRow->AddChild(win->aiChatOptionCombo, 1);
        optionsRow->AddChild(new Spacer(8, 0));
        optionsRow->AddChild(win->aiChatCheckbox, 1);

        auto vbox = new VBox();
        vbox->alignCross = CrossAxisAlign::Stretch;
        vbox->AddChild(win->aiChatLabel);
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

void OnAIChatToggle(MainWindow* win, int providerId) {
    AIChatProvider* p = GetAIChatProvider(providerId);
    if (!p || !IsAIChatAvailable()) {
        return;
    }
    if (!p->IsInstalled()) {
        AIChatNotInstalledDialogArgs args;
        args.windowTitle = p->TitleTemp();
        args.mainInstruction = p->NotInstalledInstructionTemp();
        args.docUri = p->docUri;
        AIChatShowNotInstalledDialog(args);
        return;
    }
    if (!win->hwndAiChatBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    if (AIChatGetTabPanelOpen(tab) == p->backend) {
        AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    } else {
        if (!IsAIChatSupportedForTab(tab)) {
            return;
        }
        AIChatSetTabPanelOpen(tab, p->backend);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->uiState.aiChatVisible) {
        bool providerChanged = win->aiChatProvider != providerId;
        SetPanelProvider(win, providerId);
        UpdateAIChatPanelTitle(win, 0);
        EnsureWebViewReady(win);
        UpdateAIChatPanelForCurrentTab(win);
        PopulateSessionCombo(win);
        if (win->aiChatInput) {
            HwndSetFocus(win->aiChatInput->hwnd);
        }
        if (providerChanged) {
            // the webview was recreated: replay the chat once it has loaded
            SetTimer(win->hwndAiChatBox, kTimerReplayChat, 600, nullptr);
        }
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
    if (!st->chatLog.IsEmpty()) {
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
    for (int i = 0; i < kAIChatProviderCount; i++) {
        if (tab->aiChat[i].process) {
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
        for (int i = 0; i < kAIChatProviderCount; i++) {
            AIChatCloseProcess(&tab->aiChat[i].process, true);
        }
    }
    AIChatWaitForTabProcessesToFinish(win, AIChatTabHasRunningProcess);
}

void DestroyAIChatPanel(MainWindow* win) {
    win->aiChatWebViewReady = false;

    if (win->hwndAiChatBox) {
        KillTimer(win->hwndAiChatBox, kTimerAutoSelectSession);
        KillTimer(win->hwndAiChatBox, kTimerWebViewSize);
        KillTimer(win->hwndAiChatBox, kTimerReplayChat);
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
    win->aiChatWebViewSlot = nullptr;
    win->aiChatLabel = nullptr;
    win->aiChatSessionCombo = nullptr;
    win->aiChatModelCombo = nullptr;
    win->aiChatOptionCombo = nullptr;
    win->aiChatCheckbox = nullptr;
    win->aiChatStopBtn = nullptr;
    win->aiChatInput = nullptr;

    delete win->aiChatSplitter;
    win->aiChatSplitter = nullptr;

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
