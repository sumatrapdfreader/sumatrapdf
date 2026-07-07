/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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
#include "resource.h"

#include "AIChatCommon.h"
#include "ClaudeCode.h"

bool IsClaudeCodeAvailable() {
    return IsAIChatAvailable();
}

static TempStr FindClaudeExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.local\\bin\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\Programs\\claude-code\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\npm\\claude.cmd", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"claude.exe"), WStr(L"claude"));
}

bool IsClaudeCodeInstalled() {
    return len(FindClaudeExecutableTemp()) > 0;
}

TempStr ClaudeCodeExecutablePathTemp() {
    return FindClaudeExecutableTemp();
}

static Mutex gClaudeCodeLogMutex;
static AIChatLogger gClaudeCodeLogger = {&gClaudeCodeLogMutex, "claude-code-log.txt", "claude-code"};

static void ClaudeCodeLog(Str direction, Str text) {
    AIChatLog(&gClaudeCodeLogger, direction, text);
}

static Str kClaudeCodeDocURI() {
    return StrL("/AI-Chat-with-document#claude-code");
}

static void ShowClaudeCodeNotInstalledDialog() {
    AIChatNotInstalledDialogArgs args;
    args.windowTitle = _TRA("Claude chat");
    args.mainInstruction = _TRA("Claude Code cli must be installed for this functionality");
    args.docUri = kClaudeCodeDocURI();
    AIChatShowNotInstalledDialog(args);
}

bool IsClaudeCodeSupportedForFile(Str filePath, Kind engineKind) {
    return IsAIChatSupportedForFile(filePath, engineKind);
}

bool IsClaudeCodeSupportedForTab(WindowTab* tab) {
    return IsAIChatSupportedForTab(tab);
}

#define IDC_CLAUDE_LABEL_WITH_CLOSE 1110
#define IDC_CLAUDE_SKIP_PERMS 1111
#define IDC_CLAUDE_SESSION_COMBO 1112
#define IDC_CLAUDE_MODEL_COMBO 1113
#define IDC_CLAUDE_EFFORT_COMBO 1114
#define IDC_CLAUDE_STOP_BTN 1115

static Str kClaudeVirtualHost() {
    return StrL("https://sumatrapdf.claude/");
}
constexpr const WCHAR* kClaudeVirtualHostW = L"https://sumatrapdf.claude/";

static LoadedDataResource gClaudeMarkedJs;

static Str ClaudeBgColor() {
    Str bg = gGlobalPrefs->claudeCode.bgColor;
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

static void BuildClaudeModelsList(StrVec& models) {
    models.Reset();
    AIChatAppendModelUnique(models, "sonnet");
    AIChatAppendModelUnique(models, "opus");
    AIChatAppendModelUnique(models, "haiku");
    Str extra = gGlobalPrefs->claudeCode.models;
    if (len(extra) > 0) {
        StrVec parts;
        Split(&parts, extra, ",", true);
        for (int i = 0; i < len(parts); i++) {
            AIChatAppendModelUnique(models, parts[i]);
        }
    }
}

static Str ResolveClaudeModel(const StrVec& models, Str model) {
    int idx = AIChatFindModelInList(models, model);
    if (idx >= 0) {
        return models[idx];
    }
    idx = AIChatFindModelInList(models, "opus");
    if (idx >= 0) {
        return models[idx];
    }
    return StrL("opus");
}

static void PopulateModelCombo(HWND combo) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    StrVec models;
    BuildClaudeModelsList(models);
    for (int i = 0; i < len(models); i++) {
        TempStr display = AIChatModelDisplayNameTemp(models[i], "Opus");
        WCHAR* displayW = CWStrTemp(display);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)displayW);
    }
}

// Apply persisted settings to the UI controls
static void ApplyClaudeSettingsToUI(MainWindow* win) {
    int effortIdx = gGlobalPrefs->claudeCode.effort;
    if (effortIdx < 0 || effortIdx > 3) {
        effortIdx = 1;
    }
    if (win->hwndClaudeModelCombo) {
        PopulateModelCombo(win->hwndClaudeModelCombo);
        StrVec models;
        BuildClaudeModelsList(models);
        Str model = ResolveClaudeModel(models, gGlobalPrefs->claudeCode.model);
        int modelIdx = AIChatFindModelInList(models, model);
        if (modelIdx < 0) {
            modelIdx = 0;
        }
        SendMessageW(win->hwndClaudeModelCombo, CB_SETCURSEL, modelIdx, 0);
    }
    if (win->hwndClaudeEffortCombo) {
        SendMessageW(win->hwndClaudeEffortCombo, CB_SETCURSEL, effortIdx, 0);
    }
    if (win->hwndClaudeSkipPermsCheck) {
        SendMessageW(win->hwndClaudeSkipPermsCheck, BM_SETCHECK,
                     gGlobalPrefs->claudeCode.skipPermissions ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

// Read current settings from UI controls and save
static void SyncClaudeSettingsFromUI(MainWindow* win) {
    if (win->hwndClaudeModelCombo) {
        int sel = (int)SendMessageW(win->hwndClaudeModelCombo, CB_GETCURSEL, 0, 0);
        StrVec models;
        BuildClaudeModelsList(models);
        if (sel >= 0 && sel < len(models)) {
            str::ReplaceWithCopy(&gGlobalPrefs->claudeCode.model, models[sel]);
        }
    }
    if (win->hwndClaudeEffortCombo) {
        gGlobalPrefs->claudeCode.effort = (int)SendMessageW(win->hwndClaudeEffortCombo, CB_GETCURSEL, 0, 0);
    }
    if (win->hwndClaudeSkipPermsCheck) {
        gGlobalPrefs->claudeCode.skipPermissions =
            (SendMessageW(win->hwndClaudeSkipPermsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    SaveSettings();
}

// Execute JS on the WebView AND record it in the current tab's chat log
static void LayoutClaudeBox(MainWindow* win);
static void AutoSelectRecentSession(MainWindow* win);
static void WebViewAddError(MainWindow* win, Str text); // forward decl
static void WebViewShowUnsupportedFileType(MainWindow* win);

static void UpdateClaudePanelForCurrentTab(MainWindow* win) {
    if (!win || !win->hwndClaudeBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool supported = IsClaudeCodeSupportedForTab(tab);
    bool working = supported && tab && tab->claudeProcess != nullptr;
    bool enableInput = supported && !working;

    if (win->claudeInput) {
        EnableWindow(win->claudeInput->hwnd, enableInput);
        WStr cue = WStrL(L"Ask about this document...");
        if (!supported) {
            cue = WStrL(L"Not available for this file type");
        } else if (working) {
            cue = WStrL(L"Agent is working...");
        }
        SendMessageW(win->claudeInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)cue.s);
    }
    if (win->hwndClaudeSessionCombo) {
        EnableWindow(win->hwndClaudeSessionCombo, enableInput);
    }
    if (win->hwndClaudeModelCombo) {
        EnableWindow(win->hwndClaudeModelCombo, enableInput);
    }
    if (win->hwndClaudeEffortCombo) {
        EnableWindow(win->hwndClaudeEffortCombo, enableInput);
    }
    if (win->hwndClaudeSkipPermsCheck) {
        EnableWindow(win->hwndClaudeSkipPermsCheck, enableInput);
    }
    if (win->hwndClaudeStopBtn) {
        ShowWindow(win->hwndClaudeStopBtn, working ? SW_SHOW : SW_HIDE);
        EnableWindow(win->hwndClaudeStopBtn, working);
    }
    LayoutClaudeBox(win);
}

static void SetClaudeWorking(MainWindow* win, bool /*working*/) {
    UpdateClaudePanelForCurrentTab(win);
}

static void CloseClaudeProcess(WindowTab* tab, bool terminateIfRunning) {
    if (!tab) {
        return;
    }
    AIChatCloseProcess(&tab->claudeProcess, terminateIfRunning);
}

static void StopClaude(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->claudeProcess) {
        ClaudeCodeLog("stop", tab->claudeSessionId ? tab->claudeSessionId : StrL("(no session)"));
        CloseClaudeProcess(tab, true);
        WebViewAddError(win, "Stopped by user.");
        SetClaudeWorking(win, false);
    }
}

static void WebViewEval(MainWindow* win, Str js, bool record = true) {
    if (win->claudeWebView && win->claudeWebViewReady) {
        win->claudeWebView->Eval(js);
    }
    if (record) {
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            tab->claudeChatLog.Append(js);
            tab->claudeChatLog.AppendChar('\n');
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
    Str msg = "Claude Code is only available for PDF and image files.";
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(msg));
    WebViewEval(win, js, false);
}

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, WindowTab* tab) {
    if (tab->claudeChatLog.IsEmpty()) {
        return;
    }
    if (!win->claudeWebView || !win->claudeWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str log = ToStr(tab->claudeChatLog);
    Str rest = log;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) > 0) {
            win->claudeWebView->Eval(str::DupTemp(line));
        }
    }
}

// --- Session history ---

// Compute the encoded project dir path that Claude uses:
// E:\foo_bar -> E--foo-bar (: removed, \ -> -, _ -> -)
static TempStr EncodeClaudeDirTemp(Str dir) {
    str::Builder buf;
    for (int i = 0; i < dir.len; i++) {
        char c = dir.s[i];
        if (c == ':' || c == '\\' || c == '/' || c == '_' || c == ' ') {
            buf.AppendChar('-');
        } else {
            buf.AppendChar(c);
        }
    }
    // remove trailing dash if present
    if (len(buf) > 0 && buf.LastChar() == '-') {
        buf.RemoveLast();
    }
    return ToStrTemp(buf);
}

// Extract user message text from a JSON line.
// Handles both "content":"string" and "content":[{"type":"text","text":"..."}] formats.
static TempStr ExtractUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"role\":\"user\""))) {
        return {};
    }
    // skip tool_result messages (they have role:user but contain tool output, not user text)
    if (str::Contains(line, StrL("\"tool_result\""))) {
        return {};
    }
    // try string format: "content":"text"
    if (str::Contains(line, StrL("\"content\":\""))) {
        TempStr content = AIChatJsonStrTemp(line, "content");
        if (!str::Contains(content, StrL("<command-"))) {
            return content;
        }
    }
    // try array format: "content":[{"type":"text","text":"..."}]
    if (str::Contains(line, StrL("\"content\":["))) {
        // find the text field inside the first text block
        bool hasText =
            str::Contains(line, StrL("\"type\":\"text\",\"text\":\"")) || str::Contains(line, StrL("\"text\":\""));
        if (hasText) {
            TempStr text = AIChatJsonStrTemp(line, "text");
            if (!str::Contains(text, StrL("<command-"))) {
                return text;
            }
        }
    }
    return {};
}

// Read the first user message from a session JSONL as description
static Str GetSessionDescription(Str sessionPath) {
    Str data = file::ReadFile(sessionPath);
    if (len(data) == 0) {
        return StrL("(empty)");
    }
    Str content = data;
    Str rest = content;
    Str result;
    Str line;

    while (!result && str::NextLine(rest, line, rest)) {
        if (len(line) == 0) {
            continue;
        }
        TempStr userText = ExtractUserTextTemp(str::DupTemp(line));
        if (userText) {
            result = str::Dup(userText);
        }
    }
    str::Free(data);
    return result ? result : StrL("(no description)");
}

// Scan ~/.claude/projects/<encoded-dir>/ for .jsonl session files
static void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr projectDir = fmt("%s\\.claude\\projects\\%s", userProfile, encodedDir);

    // scan for *.jsonl files in the project directory
    TempStr pattern = fmt("%s\\*.jsonl", projectDir);
    WIN32_FIND_DATAW fd;
    WCHAR* patternW = CWStrTemp(pattern);
    HANDLE hFind = FindFirstFileW(patternW, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        TempStr fileName = ToUtf8Temp(fd.cFileName);
        // session files are named <uuid>.jsonl
        int nameLen = len(fileName);
        if (nameLen < 42) { // uuid (36) + .jsonl (6) = 42
            continue;
        }
        // extract session ID (remove .jsonl extension)
        TempStr sessionId = str::DupTemp(Str(fileName.s, nameLen - 6));

        TempStr fullPath = fmt("%s\\%s", projectDir, fileName);
        Str desc = GetSessionDescription(fullPath);

        // use file modification time as timestamp
        ULARGE_INTEGER uli;
        uli.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        i64 ts = (i64)(uli.QuadPart / 10000); // to milliseconds

        AIChatSessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = ts;
        sessions.Append(si);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    AIChatSortSessionsByTimestampDesc(sessions);
}

static bool gPopulatingCombo = false; // guard against re-entrant CBN_SELCHANGE

// populate the session combo box for the current tab's directory
static void PopulateSessionCombo(MainWindow* win) {
    HWND combo = win->hwndClaudeSessionCombo;
    if (!combo) {
        return;
    }

    gPopulatingCombo = true; // prevent CBN_SELCHANGE during population

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"+ New Session");

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
        gPopulatingCombo = false;
        return;
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    int selectedIdx = 0;
    bool foundCurrent = false;
    for (int i = 0; i < len(sessions); i++) {
        Str display = sessions[i].display;
        if (len(display) == 0) {
            display = "(no description)";
        }
        TempStr label = ShortenStringUtf8Temp(display, 50);
        WCHAR* labelW = CWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);

        if (tab->claudeSessionId && str::Eq(tab->claudeSessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (tab->claudeSessionId && !foundCurrent) {
        Str label = "(current session)";
        WCHAR* labelW = CWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);
        selectedIdx = len(sessions) + 1;
    }

    SendMessageW(combo, CB_SETCURSEL, selectedIdx, 0);
    AIChatFreeSessions(sessions);

    gPopulatingCombo = false;
}

// Load conversation history from a session's JSONL file
static void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr sessionPath = fmt("%s\\.claude\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);

    if (!file::Exists(sessionPath)) {
        return;
    }

    Str data = file::ReadFile(sessionPath);
    if (len(data) == 0) {
        return;
    }

    Str content = data;
    Str rest = content;
    Str lineRaw;

    while (str::NextLine(rest, lineRaw, rest)) {
        if (len(lineRaw) > 0) {
            TempStr line = str::DupTemp(lineRaw);

            // Session JSONL format:
            // User messages: content can be string or array
            // Assistant messages: content is always array

            TempStr userText = ExtractUserTextTemp(line);
            if (userText) {
                WebViewAddUser(win, userText);
            } else if (str::Contains(line, StrL("\"role\":\"assistant\""))) {
                // assistant message
                if (str::Contains(line, StrL("\"type\":\"thinking\""))) {
                    // skip thinking blocks
                } else if (str::Contains(line, StrL("\"type\":\"text\""))) {
                    TempStr text = AIChatJsonStrTemp(line, "text");
                    if (len(text) > 0) {
                        WebViewAppendText(win, text);
                        WebViewFlushBlock(win);
                    }
                } else if (str::Contains(line, StrL("\"type\":\"tool_use\""))) {
                    TempStr toolName = AIChatJsonStrTemp(line, "name");
                    if (toolName) {
                        TempStr fp = AIChatJsonStrTemp(line, "file_path");
                        str::Builder desc;
                        desc.Append(fmt("Tool: %s", toolName));
                        if (fp) {
                            desc.Append(fmt(" (%s)", fp));
                        }
                        WebViewAddTool(win, ToStr(desc));
                    }
                }
            }
        }
    }

    str::Free(data);
}

// handle combo selection change
static void OnSessionComboChange(MainWindow* win) {
    if (gPopulatingCombo) {
        return; // ignore changes triggered by PopulateSessionCombo
    }

    HWND combo = win->hwndClaudeSessionCombo;
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }

    if (sel == 0) {
        // "New Session" — clear current session
        str::ReplaceWithCopy(&tab->claudeSessionId, Str{});
        tab->claudeChatLog.Reset();
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < len(sessions)) {
        str::ReplaceWithCopy(&tab->claudeSessionId, sessions[sessionIdx].sessionId);
        tab->claudeChatLog.Reset();
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
        // LoadSessionHistory calls WebViewEval which rebuilds claudeChatLog
    }

    AIChatFreeSessions(sessions);
}

// --- Stream JSON events ---
enum class ClaudeUpdateType {
    Text,
    Tool,
    Error,
    Flush,
    Finished,
};

struct ClaudeUpdateData {
    HWND hwndFrame;
    Str text;
    Str sessionId; // to identify which tab this belongs to
    ClaudeUpdateType updateType;
};

static void OnClaudeUpdate(ClaudeUpdateData* data) {
    MainWindow* win = AIChatFindMainWindowByFrame(data->hwndFrame);
    if (!win || !IsMainWindowValid(win) || !win->hwndClaudeBox) {
        str::Free(data->text);
        str::Free(data->sessionId);
        free(data);
        return;
    }
    {
        // find the tab this update belongs to
        WindowTab* tab = nullptr;
        for (WindowTab* t : win->Tabs()) {
            if (t->claudeSessionId && data->sessionId && str::Eq(t->claudeSessionId, data->sessionId)) {
                tab = t;
                break;
            }
        }
        if (!tab) {
            WindowTab* cur = win->CurrentTab();
            if (cur && cur->claudeSessionId && data->sessionId && str::Eq(cur->claudeSessionId, data->sessionId)) {
                tab = cur;
            }
        }
        // only update WebView if this tab is currently active
        bool isActiveTab = (tab && tab == win->CurrentTab());

        switch (data->updateType) {
            case ClaudeUpdateType::Text:
                if (isActiveTab) {
                    WebViewAppendText(win, data->text);
                }
                break;
            case ClaudeUpdateType::Tool:
                if (isActiveTab) {
                    WebViewAddTool(win, data->text);
                }
                break;
            case ClaudeUpdateType::Error:
                if (isActiveTab) {
                    WebViewAddError(win, data->text);
                }
                break;
            case ClaudeUpdateType::Flush:
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                }
                break;
            case ClaudeUpdateType::Finished:
                if (tab) {
                    CloseClaudeProcess(tab, true);
                }
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                    SetClaudeWorking(win, false);
                    PopulateSessionCombo(win);
                }
                break;
        }
    }
    str::Free(data->text);
    str::Free(data->sessionId);
    free(data);
}

static void PostUpdate(HWND hwndFrame, Str sessionId, Str text, ClaudeUpdateType type) {
    auto data = (ClaudeUpdateData*)calloc(1, sizeof(ClaudeUpdateData));
    data->hwndFrame = hwndFrame;
    data->sessionId = sessionId ? str::Dup(sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnClaudeUpdate, data));
}

struct ClaudeReadCtx {
    HANDLE hReadPipe;
    HWND hwndFrame;
    Str sessionId;
};

static void ClaudeReadThread(ClaudeReadCtx* ctx) {
    HANDLE hPipe = ctx->hReadPipe;
    HWND hwndFrame = ctx->hwndFrame;
    Str sessionId = ctx->sessionId;
    free(ctx);

    str::Builder lineBuf;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                Str line = ToStr(lineBuf);
                if (line) {
                    ClaudeCodeLog("<<<", line);
                }
                TempStr eventType = AIChatJsonStrTemp(line, "type");

                if (eventType && str::Eq(eventType, "assistant")) {
                    if (str::Contains(line, StrL("\"type\":\"text\""))) {
                        TempStr text = AIChatJsonStrTemp(line, "text");
                        if (len(text) > 0) {
                            PostUpdate(hwndFrame, sessionId, text, ClaudeUpdateType::Text);
                        }
                    }
                    if (str::Contains(line, StrL("\"type\":\"tool_use\""))) {
                        TempStr toolName = AIChatJsonStrTemp(line, "name");
                        if (toolName) {
                            TempStr fp = AIChatJsonStrTemp(line, "file_path");
                            TempStr cmd = AIChatJsonStrTemp(line, "command");
                            TempStr pat = AIChatJsonStrTemp(line, "pattern");
                            str::Builder desc;
                            desc.Append(fmt("Tool: %s", toolName));
                            if (fp) {
                                desc.Append(fmt(" (%s)", fp));
                            } else if (cmd) {
                                if (len(cmd) > 60) {
                                    desc.Append(fmt(" $ %.60s...", cmd));
                                } else {
                                    desc.Append(fmt(" $ %s", cmd));
                                }
                            } else if (pat) {
                                desc.Append(fmt(" /%s/", pat));
                            }
                            PostUpdate(hwndFrame, sessionId, ToStr(desc), ClaudeUpdateType::Tool);
                        }
                    }
                } else if (eventType && str::Eq(eventType, "user")) {
                    if (str::Contains(line, StrL("\"tool_use_result\""))) {
                        TempStr fp = AIChatJsonStrTemp(line, "filePath");
                        if (fp) {
                            str::Builder desc;
                            desc.Append(fmt("Result: %s", fp));
                            PostUpdate(hwndFrame, sessionId, ToStr(desc), ClaudeUpdateType::Tool);
                        }
                    }
                } else if (eventType && str::Eq(eventType, "result")) {
                    TempStr sub = AIChatJsonStrTemp(line, "subtype");
                    if (sub && str::Eq(sub, "error")) {
                        TempStr err = AIChatJsonStrTemp(line, "error");
                        if (err) {
                            PostUpdate(hwndFrame, sessionId, err, ClaudeUpdateType::Error);
                        }
                    }
                    // claude emits result before the process exits; don't wait for EOF
                    if (sub && (str::Eq(sub, "success") || str::Eq(sub, "completion") || str::Eq(sub, "error"))) {
                        PostUpdate(hwndFrame, sessionId, {}, ClaudeUpdateType::Flush);
                        PostUpdate(hwndFrame, sessionId, {}, ClaudeUpdateType::Finished);
                    }
                }

                lineBuf.Reset();
            } else if (buf[i] != '\r') {
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    CloseHandle(hPipe);
    PostUpdate(hwndFrame, sessionId, {}, ClaudeUpdateType::Finished);
    str::Free(sessionId);
}

static void StartClaudeReadThread(ClaudeReadCtx* ctx) {
    ClaudeReadThread(ctx);
}

static void SendClaudeMessage(MainWindow* win) {
    if (!win->claudeInput) {
        return;
    }
    if (!IsClaudeCodeSupportedForTab(win->CurrentTab())) {
        return;
    }
    HWND hwndInput = win->claudeInput->hwnd;
    int inputLen = GetWindowTextLengthW(hwndInput);
    if (inputLen == 0) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }
    if (tab->claudeProcess) {
        return; // this tab already has a running request
    }

    TempWStr inputW = HwndGetTextWTemp(hwndInput);
    TempStr input = ToUtf8Temp(inputW);
    HwndSetText(hwndInput, "");

    WebViewAddUser(win, input);
    SetClaudeWorking(win, true);

    bool isNewSession = len(tab->claudeSessionId) == 0;
    if (isNewSession) {
        str::ReplaceWithCopy(&tab->claudeSessionId, AIChatGenerateSessionIdTemp());
    }

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);
    TempStr fileName = path::GetBaseNameTemp(filePath);

    TempStr escapedInput = str::ReplaceTemp(input, StrL("\""), StrL("\\\""));

    // sync and save settings from UI
    SyncClaudeSettingsFromUI(win);

    Str efforts[] = {"low", "medium", "high", "max"};
    StrVec modelList;
    BuildClaudeModelsList(modelList);
    Str model = ResolveClaudeModel(modelList, gGlobalPrefs->claudeCode.model);
    int effortIdx = gGlobalPrefs->claudeCode.effort;
    if (effortIdx < 0 || effortIdx > 3) {
        effortIdx = 1;
    }
    Str permsFlag = gGlobalPrefs->claudeCode.skipPermissions ? "--dangerously-skip-permissions" : "";

    TempStr claudePath = FindClaudeExecutableTemp();
    if (!claudePath) {
        ClaudeCodeLog("error", "Cannot find claude executable");
        WebViewAddError(win, "Cannot find claude. Is Claude Code installed?");
        SetClaudeWorking(win, false);
        return;
    }

    TempStr sessionName = fmt("%s", fileName);

    ClaudeCodeLog(">>> user", input);
    ClaudeCodeLog(">>> session", fmt("%s (%s)", tab->claudeSessionId, Str(isNewSession ? "new" : "resume")));
    ClaudeCodeLog(">>> cwd", dir);

    TempStr cmdLine;
    if (isNewSession) {
        cmdLine =
            fmt("\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --session-id %s "
                "--name \"%s\" "
                "--append-system-prompt \"The user is currently reading the file: %s\" "
                "\"%s\"",
                claudePath, model, efforts[effortIdx], permsFlag, tab->claudeSessionId, sessionName, filePath,
                escapedInput);
    } else {
        cmdLine =
            fmt("\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --resume %s "
                "--append-system-prompt \"The user is currently reading the file: %s\" "
                "\"%s\"",
                claudePath, model, efforts[effortIdx], permsFlag, tab->claudeSessionId, filePath, escapedInput);
    }

    ClaudeCodeLog(">>> cmd", cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        ClaudeCodeLog("error", "Failed to launch claude process");
        WebViewAddError(win, "Failed to launch claude. Is it installed and in PATH?");
        SetClaudeWorking(win, false);
        return;
    }

    tab->claudeProcess = launch.hProcess;

    auto ctx = (ClaudeReadCtx*)calloc(1, sizeof(ClaudeReadCtx));
    ctx->hReadPipe = launch.hReadPipe;
    ctx->hwndFrame = win->hwndFrame;
    ctx->sessionId = str::Dup(tab->claudeSessionId);
    RunAsync(MkFunc0(StartClaudeReadThread, ctx), "ClaudeReadThread");
}

static TempStr FitClaudePanelTitleTemp(HWND labelHwnd, HFONT font, Str docName, int maxDx) {
    TempStr prefix = str::JoinTemp(_TRA("Claude chat"), StrL(" with "));
    return AIChatFitPanelTitleTemp(labelHwnd, font, prefix, docName, maxDx);
}

static void UpdateClaudePanelTitle(MainWindow* win, int labelDx) {
    if (!win || !win->claudeLabelWithClose) {
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

    HWND labelHwnd = win->claudeLabelWithClose->hwnd;
    HFONT font = win->claudeLabelWithClose->font;
    if (!font) {
        font = GetDefaultGuiFont(true, false);
    }
    if (labelDx <= 0 && win->hwndClaudeBox) {
        labelDx = ClientRect(win->hwndClaudeBox).dx;
    }
    int maxDx = AIChatLabelMaxTextDx(labelHwnd, labelDx);
    TempStr label = FitClaudePanelTitleTemp(labelHwnd, font, docName, maxDx);
    win->claudeLabelWithClose->SetLabel(label);
}

// --- Layout ---
static void LayoutClaudeBox(MainWindow* win) {
    HWND hwndContainer = win->hwndClaudeBox;
    Rect rc = ClientRect(hwndContainer);
    int y = 0;

    UpdateClaudePanelTitle(win, rc.dx);

    // label
    Size labelSize = win->claudeLabelWithClose->GetIdealSize();
    MoveWindow(win->claudeLabelWithClose->hwnd, 0, y, rc.dx, labelSize.dy, TRUE);
    y += labelSize.dy;

    // session combo — get actual height from font metrics
    int comboDy = 0;
    if (win->hwndClaudeSessionCombo) {
        // the visible edit part of a dropdown is determined by the font
        // GetComboBoxInfo or just use SendMessage CB_GETITEMHEIGHT
        int itemH = (int)SendMessageW(win->hwndClaudeSessionCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        comboDy = itemH + 8; // item height + borders
        // MoveWindow height for CBS_DROPDOWNLIST = visible height + dropdown list height
        MoveWindow(win->hwndClaudeSessionCombo, 2, y + 1, rc.dx - 4, comboDy + 200, TRUE);
    }
    y += comboDy + 3;

    // bottom: input, then [Model▾][Effort▾][☐Skip]
    Size inputSize = win->claudeInput->GetIdealSize();
    int inputDy = inputSize.dy + 4;
    int optRowDy = 32;
    if (win->hwndClaudeModelCombo) {
        int itemH = (int)SendMessageW(win->hwndClaudeModelCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        optRowDy = itemH + 8;
    }
    int bottomDy = inputDy + 4 + optRowDy;

    int webViewDy = rc.dy - y - bottomDy;
    if (webViewDy < 0) {
        webViewDy = 0;
    }

    if (win->claudeWebView) {
        MoveWindow(win->claudeWebView->hwnd, 0, y, rc.dx, webViewDy, TRUE);
        // defer UpdateWebviewSize during rapid WM_SIZE to avoid WebView2 put_Bounds freeze
        KillTimer(win->hwndClaudeBox, 43);
        SetTimer(win->hwndClaudeBox, 43, 50, nullptr);
    }
    y += webViewDy;

    // input row: [input box] [Stop] — stop button only visible when working
    int stopBtnDx = 50;
    WindowTab* curTab = win->CurrentTab();
    bool isWorking = (curTab && curTab->claudeProcess != nullptr);
    if (isWorking && win->hwndClaudeStopBtn) {
        MoveWindow(win->claudeInput->hwnd, 0, y, rc.dx - stopBtnDx - 2, inputDy, TRUE);
        MoveWindow(win->hwndClaudeStopBtn, rc.dx - stopBtnDx, y, stopBtnDx, inputDy, TRUE);
    } else {
        MoveWindow(win->claudeInput->hwnd, 0, y, rc.dx, inputDy, TRUE);
    }
    y += inputDy + 4;

    // options row: [Model▾] [Effort▾] [☐Skip]
    {
        int x = 2;
        int thirdDx = (rc.dx - 8) / 3;
        if (win->hwndClaudeModelCombo) {
            MoveWindow(win->hwndClaudeModelCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndClaudeEffortCombo) {
            MoveWindow(win->hwndClaudeEffortCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndClaudeSkipPermsCheck) {
            MoveWindow(win->hwndClaudeSkipPermsCheck, x + 8, y, rc.dx - x - 10, optRowDy, TRUE);
        }
    }
}

// --- WndProc ---
static LRESULT CALLBACK WndProcClaudeInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                           DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !IsShiftPressed()) {
        SendClaudeMessage(win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WndProcClaudeBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                         DWORD_PTR data) {
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
        case WM_SIZE:
            LayoutClaudeBox(win);
            break;
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_CLAUDE_LABEL_WITH_CLOSE) {
                ToggleClaudePanel(win);
            }
            if (LOWORD(wp) == IDC_CLAUDE_STOP_BTN) {
                StopClaude(win);
            }
            if (LOWORD(wp) == IDC_CLAUDE_SESSION_COMBO && HIWORD(wp) == CBN_SELCHANGE) {
                OnSessionComboChange(win);
            }
            break;
        case WM_TIMER:
            if (wp == 42) {
                KillTimer(hwnd, 42);
                AutoSelectRecentSession(win);
                PopulateSessionCombo(win);
            } else if (wp == 43) {
                KillTimer(hwnd, 43);
                if (win->claudeWebView) {
                    win->claudeWebView->UpdateWebviewSize();
                }
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// --- Splitter ---
constexpr int kClaudeMinDx = 150;

static void OnClaudeSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = ClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    if (dx < kClaudeMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    AIChatUpdateSidebarDx(win, dx, ev->finishedDragging);
    if (ev->finishedDragging) {
        RelayoutForClaudeSplitter(win);
    }
}

void RelayoutClaudePanel(MainWindow* win) {
    if (!win || !win->hwndClaudeBox || !win->claudeVisible) {
        return;
    }
    LayoutClaudeBox(win);
    KillTimer(win->hwndClaudeBox, 43);
    if (win->claudeWebView && win->claudeWebViewReady) {
        win->claudeWebView->UpdateWebviewSize();
    }
    RedrawWindow(win->hwndClaudeBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (win->claudeSplitter && win->claudeSplitter->hwnd) {
        InvalidateRect(win->claudeSplitter->hwnd, nullptr, TRUE);
    }
}

// --- Lazy WebView2 init ---
static void EnsureWebViewReady(MainWindow* win) {
    if (win->claudeWebViewReady) {
        return;
    }
    if (!HasWebView()) {
        return;
    }
    auto webView = new WebviewWnd();
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA);
    // use unique data dir per process to avoid locking conflicts
    webView->dataDir = str::Dup(fmt("%s\\SumatraPDF\\ClaudeWebView_%d", userProfile, (int)GetCurrentProcessId()));
    if (!LockDataResource(IDR_CLAUDE_MARKED_JS, &gClaudeMarkedJs)) {
        delete webView;
        return;
    }
    wstr::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = wstr::Dup(kClaudeVirtualHostW);
    webView->resourceProvider.ctx = &gClaudeMarkedJs;
    webView->resourceProvider.getResource = AIChatGetMarkedJsResource;

    Rect rc = ClientRect(win->hwndClaudeBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndClaudeBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr chatHtml = AIChatFormatChatHtmlTemp(kClaudeVirtualHost(), ClaudeBgColor());
        webView->SetHtml(chatHtml);
        win->claudeWebView = webView;
        win->claudeWebViewReady = true;
        RelayoutClaudePanel(win);
    } else {
        delete webView;
    }
}

// --- Public API ---
void CreateClaudePanel(MainWindow* win) {
    if (!IsClaudeCodeAvailable()) {
        return;
    }
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndClaudeBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    // splitter (non-live: only resize on mouse release)
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->claudeSplitter = new Splitter();
        win->claudeSplitter->onMove = MkFunc1Void(OnClaudeSplitterMove);
        win->claudeSplitter->Create(args);
    }

    // label
    auto label = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndClaudeBox;
        args.cmdId = IDC_CLAUDE_LABEL_WITH_CLOSE;
        args.isRtl = IsUIRtl();
        args.font = GetDefaultGuiFont(true, false);
        label->Create(args);
    }
    win->claudeLabelWithClose = label;
    label->SetPaddingXY(2, 2);
    UpdateClaudePanelTitle(win, 0);

    // session combo
    win->hwndClaudeSessionCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, dx, 200,
                        win->hwndClaudeBox, (HMENU)(UINT_PTR)IDC_CLAUDE_SESSION_COMBO, hmod, nullptr);
    SendMessageW(win->hwndClaudeSessionCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // webview deferred
    win->claudeWebView = nullptr;
    win->claudeWebViewReady = false;

    // model combo
    win->hwndClaudeModelCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200,
                        win->hwndClaudeBox, (HMENU)(UINT_PTR)IDC_CLAUDE_MODEL_COMBO, hmod, nullptr);
    SendMessageW(win->hwndClaudeModelCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // effort combo
    win->hwndClaudeEffortCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200,
                        win->hwndClaudeBox, (HMENU)(UINT_PTR)IDC_CLAUDE_EFFORT_COMBO, hmod, nullptr);
    SendMessageW(win->hwndClaudeEffortCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    SendMessageW(win->hwndClaudeEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Low");
    SendMessageW(win->hwndClaudeEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Medium");
    SendMessageW(win->hwndClaudeEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"High");
    SendMessageW(win->hwndClaudeEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Max");
    SendMessageW(win->hwndClaudeEffortCombo, CB_SETCURSEL, 1, 0); // default: Medium

    // skip-permissions checkbox
    win->hwndClaudeSkipPermsCheck =
        CreateWindowExW(0, L"BUTTON", L"Skip Permissions", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 150, 20,
                        win->hwndClaudeBox, (HMENU)(UINT_PTR)IDC_CLAUDE_SKIP_PERMS, hmod, nullptr);
    SendMessageW(win->hwndClaudeSkipPermsCheck, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // stop button (hidden by default, shown when agent is working)
    win->hwndClaudeStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | BS_PUSHBUTTON, 0, 0, 50, 24,
                                             win->hwndClaudeBox, (HMENU)(UINT_PTR)IDC_CLAUDE_STOP_BTN, hmod, nullptr);
    SendMessageW(win->hwndClaudeStopBtn, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    ShowWindow(win->hwndClaudeStopBtn, SW_HIDE);

    // input box
    auto input = new Edit();
    {
        Edit::CreateArgs args;
        args.parent = win->hwndClaudeBox;
        args.isMultiLine = true;
        args.idealSizeLines = 3;
        args.withBorder = true;
        args.cueText = "Ask about this document...";
        input->Create(args);
    }
    win->claudeInput = input;

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(input->hwnd, WndProcClaudeInput, inputSubclassId, (DWORD_PTR)win);

    win->claudeBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndClaudeBox, WndProcClaudeBox, win->claudeBoxSubclassId, (DWORD_PTR)win);

    ApplyClaudeSettingsToUI(win);
    AIChatApplySavedSidebarDx(win);
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || tab->claudeSessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    if (len(sessions) > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        str::ReplaceWithCopy(&tab->claudeSessionId, sessions[0].sessionId);

        // load its history
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
    }

    AIChatFreeSessions(sessions);
}

void OnAIChatWithClaudeCode(MainWindow* win) {
    if (!IsClaudeCodeAvailable()) {
        return;
    }
    if (!IsClaudeCodeInstalled()) {
        ShowClaudeCodeNotInstalledDialog();
        return;
    }
    ToggleClaudePanel(win);
}

void ToggleClaudePanel(MainWindow* win) {
    if (!IsClaudeCodeAvailable() || !win->hwndClaudeBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    if (AIChatGetTabPanelOpen(tab) == AIChatBackend::Claude) {
        AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    } else {
        if (!IsClaudeCodeSupportedForTab(tab)) {
            return;
        }
        AIChatSetTabPanelOpen(tab, AIChatBackend::Claude);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->claudeVisible) {
        UpdateClaudePanelTitle(win, 0);
        EnsureWebViewReady(win);
        UpdateClaudePanelForCurrentTab(win);
        PopulateSessionCombo(win);
        if (win->claudeInput) {
            HwndSetFocus(win->claudeInput->hwnd);
        }
        // defer auto-select so SetHtml has time to load the page
        SetTimer(win->hwndClaudeBox, 42, 500, nullptr);
    }
    RelayoutWindow(win);
}

// call when switching tabs to update session context
void OnClaudeTabChanged(MainWindow* win) {
    UpdateClaudePanelTitle(win, 0);
    WindowTab* tab = win->CurrentTab();
    bool supported = IsClaudeCodeSupportedForTab(tab);
    UpdateClaudePanelForCurrentTab(win);

    if (!win->claudeVisible) {
        return;
    }

    if (!supported) {
        WebViewShowUnsupportedFileType(win);
        return;
    }

    PopulateSessionCombo(win);
    WebViewClearChat(win);

    if (!tab) {
        return;
    }

    // update working state for this tab
    SetClaudeWorking(win, tab->claudeProcess != nullptr);

    // if tab has in-memory chat log, replay it (fast, includes current session)
    if (!tab->claudeChatLog.IsEmpty()) {
        ReplayChatLog(win, tab);
    } else if (tab->filePath && tab->claudeSessionId) {
        // fallback: load from disk
        TempStr dir = path::GetDirTemp(tab->filePath);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
    }
}

static bool ClaudeTabHasRunningProcess(WindowTab* tab) {
    return tab && tab->claudeProcess;
}

void ShutdownClaudeForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        CloseClaudeProcess(tab, true);
    }
    AIChatWaitForTabProcessesToFinish(win, ClaudeTabHasRunningProcess);
}

void DestroyClaudePanel(MainWindow* win) {
    win->claudeWebViewReady = false;

    if (win->hwndClaudeBox) {
        KillTimer(win->hwndClaudeBox, 42);
        KillTimer(win->hwndClaudeBox, 43);
        if (win->claudeBoxSubclassId) {
            RemoveWindowSubclass(win->hwndClaudeBox, WndProcClaudeBox, win->claudeBoxSubclassId);
            win->claudeBoxSubclassId = 0;
        }
    }

    // save webview dataDir before deleting so we can clean up
    Str webViewDataDir;
    WebviewWnd* webView = win->claudeWebView;
    win->claudeWebView = nullptr;
    if (webView) {
        webViewDataDir = str::Dup(webView->dataDir);
    }

    delete win->claudeLabelWithClose;
    win->claudeLabelWithClose = nullptr;
    delete webView;
    delete win->claudeInput;
    win->claudeInput = nullptr;
    delete win->claudeSplitter;
    win->claudeSplitter = nullptr;

    if (win->hwndClaudeBox) {
        DestroyWindow(win->hwndClaudeBox);
        win->hwndClaudeBox = nullptr;
    }
    win->hwndClaudeSessionCombo = nullptr;
    win->hwndClaudeModelCombo = nullptr;
    win->hwndClaudeEffortCombo = nullptr;
    win->hwndClaudeSkipPermsCheck = nullptr;
    win->hwndClaudeStopBtn = nullptr;

    // clean up per-process WebView2 cache dir
    if (webViewDataDir) {
        dir::RemoveAll(webViewDataDir);
        str::Free(webViewDataDir);
    }
}
