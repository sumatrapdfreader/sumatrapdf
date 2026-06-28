/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"

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
#include "CodexBuild.h"

bool IsCodexBuildAvailable() {
    return IsAIChatAvailable();
}

static TempStr FindCodexExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(str::FormatTemp("%s\\.codex\\bin\\codex.exe", userProfile.s));
        candidates.Append(str::FormatTemp("%s\\.local\\bin\\codex.exe", userProfile.s));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"codex.exe"), WStr(L"codex"));
}

bool IsCodexBuildInstalled() {
    return FindCodexExecutableTemp() != nullptr;
}

TempStr CodexBuildExecutablePathTemp() {
    return FindCodexExecutableTemp();
}

static Mutex gCodexBuildLogMutex;
static AIChatLogger gCodexBuildLogger = {&gCodexBuildLogMutex, "gpt-5.5-log.txt", "gpt-5.5"};

static void CodexBuildLog(Str direction, Str text) {
    AIChatLog(&gCodexBuildLogger, direction, text);
}

static Str kCodexBuildDocURI() {
    return Str("/AI-Chat-with-document#openai-codex");
}

static void ShowCodexBuildNotInstalledDialog() {
    AIChatNotInstalledDialogArgs args;
    args.windowTitle = _TRA("Codex chat");
    args.mainInstruction = _TRA("OpenAI Codex CLI must be installed for this functionality");
    args.docUri = kCodexBuildDocURI();
    AIChatShowNotInstalledDialog(args);
}

bool IsCodexBuildSupportedForFile(Str filePath, Kind engineKind) {
    return IsAIChatSupportedForFile(filePath, engineKind);
}

bool IsCodexBuildSupportedForTab(WindowTab* tab) {
    return IsAIChatSupportedForTab(tab);
}

#define IDC_CODEX_LABEL_WITH_CLOSE 1130
#define IDC_CODEX_SKIP_SANDBOX 1131
#define IDC_CODEX_SESSION_COMBO 1132
#define IDC_CODEX_MODEL_COMBO 1133
#define IDC_CODEX_SANDBOX_COMBO 1134
#define IDC_CODEX_STOP_BTN 1135

static Str kCodexVirtualHost() {
    return Str("https://sumatrapdf.codex/");
}
constexpr const WCHAR* kCodexVirtualHostW = L"https://sumatrapdf.codex/";

static LoadedDataResource gCodexMarkedJs;

static Str CodexBgColor() {
    Str bg = gGlobalPrefs->codexBuild.bgColor;
    if (str::IsEmpty(bg)) {
        return Str("#ffffff");
    }
    return bg;
}

static void BuildCodexModelsList(StrVec& models) {
    models.Reset();
    AIChatAppendModelUnique(models, "gpt-5.5");
    AIChatAppendModelUnique(models, "gpt-5.4");
    AIChatAppendModelUnique(models, "o3");
    Str extra = gGlobalPrefs->codexBuild.models;
    if (!str::IsEmpty(extra)) {
        StrVec parts;
        Split(&parts, extra, ",", true);
        for (int i = 0; i < parts.Size(); i++) {
            AIChatAppendModelUnique(models, parts.At(i));
        }
    }
}

static Str ResolveCodexModel(const StrVec& models, Str model) {
    int idx = AIChatFindModelInList(models, model);
    if (idx >= 0) {
        return models.At(idx);
    }
    idx = AIChatFindModelInList(models, "gpt-5.5");
    if (idx >= 0) {
        return models.At(idx);
    }
    return Str("gpt-5.5");
}

static void PopulateModelCombo(HWND combo) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    StrVec models;
    BuildCodexModelsList(models);
    for (int i = 0; i < models.Size(); i++) {
        TempStr display = AIChatModelDisplayNameTemp(models.At(i), "Gpt-5.5");
        TempWStr displayW = ToWStrTemp(display);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)displayW.s);
    }
}

// Apply persisted settings to the UI controls
static void ApplyCodexSettingsToUI(MainWindow* win) {
    int sandboxIdx = gGlobalPrefs->codexBuild.sandbox;
    if (sandboxIdx < 0 || sandboxIdx > 2) {
        sandboxIdx = 1;
    }
    if (win->hwndCodexModelCombo) {
        PopulateModelCombo(win->hwndCodexModelCombo);
        StrVec models;
        BuildCodexModelsList(models);
        Str model = ResolveCodexModel(models, gGlobalPrefs->codexBuild.model);
        int modelIdx = AIChatFindModelInList(models, model);
        if (modelIdx < 0) {
            modelIdx = 0;
        }
        SendMessageW(win->hwndCodexModelCombo, CB_SETCURSEL, modelIdx, 0);
    }
    if (win->hwndCodexSandboxCombo) {
        SendMessageW(win->hwndCodexSandboxCombo, CB_SETCURSEL, sandboxIdx, 0);
    }
    if (win->hwndCodexSkipSandboxCheck) {
        SendMessageW(win->hwndCodexSkipSandboxCheck, BM_SETCHECK,
                     gGlobalPrefs->codexBuild.skipSandbox ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

// Read current settings from UI controls and save
static void SyncCodexSettingsFromUI(MainWindow* win) {
    if (win->hwndCodexModelCombo) {
        int sel = (int)SendMessageW(win->hwndCodexModelCombo, CB_GETCURSEL, 0, 0);
        StrVec models;
        BuildCodexModelsList(models);
        if (sel >= 0 && sel < models.Size()) {
            str::ReplaceWithCopy(&gGlobalPrefs->codexBuild.model, models.At(sel));
        }
    }
    if (win->hwndCodexSandboxCombo) {
        gGlobalPrefs->codexBuild.sandbox = (int)SendMessageW(win->hwndCodexSandboxCombo, CB_GETCURSEL, 0, 0);
    }
    if (win->hwndCodexSkipSandboxCheck) {
        gGlobalPrefs->codexBuild.skipSandbox =
            (SendMessageW(win->hwndCodexSkipSandboxCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    SaveSettings();
}

// Execute JS on the WebView AND record it in the current tab's chat log
static void LayoutCodexBox(MainWindow* win);
static void AutoSelectRecentSession(MainWindow* win);
static void WebViewAddError(MainWindow* win, Str text); // forward decl
static void WebViewShowUnsupportedFileType(MainWindow* win);

static void UpdateCodexPanelForCurrentTab(MainWindow* win) {
    if (!win || !win->hwndCodexBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool supported = IsCodexBuildSupportedForTab(tab);
    bool working = supported && tab && tab->codexProcess != nullptr;
    bool enableInput = supported && !working;

    if (win->codexInput) {
        EnableWindow(win->codexInput->hwnd, enableInput);
        WStr cue = WStrL(L"Ask about this document...");
        if (!supported) {
            cue = WStrL(L"Not available for this file type");
        } else if (working) {
            cue = WStrL(L"Agent is working...");
        }
        SendMessageW(win->codexInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)cue.s);
    }
    if (win->hwndCodexSessionCombo) {
        EnableWindow(win->hwndCodexSessionCombo, enableInput);
    }
    if (win->hwndCodexModelCombo) {
        EnableWindow(win->hwndCodexModelCombo, enableInput);
    }
    if (win->hwndCodexSandboxCombo) {
        EnableWindow(win->hwndCodexSandboxCombo, enableInput);
    }
    if (win->hwndCodexSkipSandboxCheck) {
        EnableWindow(win->hwndCodexSkipSandboxCheck, enableInput);
    }
    if (win->hwndCodexStopBtn) {
        ShowWindow(win->hwndCodexStopBtn, working ? SW_SHOW : SW_HIDE);
        EnableWindow(win->hwndCodexStopBtn, working);
    }
    LayoutCodexBox(win);
}

static void SetCodexWorking(MainWindow* win, bool /*working*/) {
    UpdateCodexPanelForCurrentTab(win);
}

static void CloseCodexProcess(WindowTab* tab, bool terminateIfRunning) {
    if (!tab) {
        return;
    }
    AIChatCloseProcess(&tab->codexProcess, terminateIfRunning);
}

static void StopCodex(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->codexProcess) {
        CodexBuildLog("stop", tab->codexSessionId ? tab->codexSessionId : Str("(no session)"));
        CloseCodexProcess(tab, true);
        WebViewAddError(win, "Stopped by user.");
        SetCodexWorking(win, false);
    }
}

static void WebViewEval(MainWindow* win, Str js, bool record = true) {
    if (win->codexWebView && win->codexWebViewReady) {
        win->codexWebView->Eval(js);
    }
    if (record) {
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            if (!tab->codexChatLog) {
                tab->codexChatLog = new StrBuilder();
            }
            tab->codexChatLog->Append(js);
            tab->codexChatLog->AppendChar('\n');
        }
    }
}

static void WebViewAppendText(MainWindow* win, Str text) {
    TempStr js = str::FormatTemp("appendText('%s')", AIChatJsEscapeTemp(text).s);
    WebViewEval(win, js);
}

static void WebViewAddUser(MainWindow* win, Str text) {
    TempStr js = str::FormatTemp("addUser('%s')", AIChatJsEscapeTemp(text).s);
    WebViewEval(win, js);
}

static void WebViewAddTool(MainWindow* win, Str text) {
    TempStr js = str::FormatTemp("addTool('%s')", AIChatJsEscapeTemp(text).s);
    WebViewEval(win, js);
}

static void WebViewAddError(MainWindow* win, Str text) {
    CodexBuildLog("error", text);
    TempStr js = str::FormatTemp("addError('%s')", AIChatJsEscapeTemp(text).s);
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
    Str msg = "OpenAI Codex is only available for PDF and image files.";
    TempStr js = str::FormatTemp("addError('%s')", AIChatJsEscapeTemp(msg).s);
    WebViewEval(win, js, false);
}

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, WindowTab* tab) {
    if (!tab->codexChatLog || tab->codexChatLog->IsEmpty()) {
        return;
    }
    if (!win->codexWebView || !win->codexWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str log = tab->codexChatLog->LendData();
    Str rest = log;
    while (!str::IsEmpty(rest)) {
        Str lineEnd = str::FindChar(rest, '\n');
        int lineLen = lineEnd ? (int)(lineEnd.s - rest.s) : rest.len;
        if (lineLen > 0) {
            TempStr line = str::DupTemp(Str(rest.s, lineLen));
            win->codexWebView->Eval(line);
        }
        if (!lineEnd) {
            break;
        }
        rest.s = lineEnd.s + 1;
        rest.len -= lineLen + 1;
    }
}

// --- Session history ---
static TempStr CodexSessionsRootTemp() {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return {};
    }
    return str::FormatTemp("%s\\.codex\\sessions", userProfile.s);
}

static TempStr NormalizeCodexPathTemp(Str path) {
    if (!path) {
        return {};
    }
    if (str::StartsWith(path, "\\\\?\\")) {
        return str::DupTemp(Str(path.s + 4, path.len - 4));
    }
    return str::DupTemp(path);
}

static bool CodexPathsEqual(Str a, Str b) {
    TempStr na = NormalizeCodexPathTemp(a);
    TempStr nb = NormalizeCodexPathTemp(b);
    if (!na || !nb) {
        return false;
    }
    return path::IsSame(na, nb);
}

static bool IsCodexRolloutFileName(Str name) {
    return name && str::StartsWith(name, "rollout-") && str::EndsWithI(name, ".jsonl");
}

static TempStr ExtractCodexPromptFromHistoryLineTemp(Str line, Str sessionId) {
    TempStr sid = AIChatJsonStrTemp(line, "session_id");
    if (!sid || !str::Eq(sid, sessionId)) {
        return {};
    }
    return AIChatJsonStrTemp(line, "text");
}

static Str GetCodexSessionDescription(Str sessionId) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    TempStr historyPath = userProfile ? str::FormatTemp("%s\\.codex\\history.jsonl", userProfile.s) : nullptr;
    if (!historyPath) {
        return Str("(no description)");
    }
    ByteSlice data = file::ReadFile(historyPath);
    if (data.empty()) {
        return Str("(no description)");
    }
    Str content = AsStr(data);
    Str rest = content;
    Str result;

    while (!str::IsEmpty(rest) && !result) {
        Str lineEnd = str::FindChar(rest, '\n');
        if (!lineEnd) {
            lineEnd = str::FindChar(rest, '\r');
        }
        int lineLen = lineEnd ? (int)(lineEnd.s - rest.s) : rest.len;
        if (lineLen > 0) {
            TempStr line = str::DupTemp(Str(rest.s, lineLen));
            TempStr prompt = ExtractCodexPromptFromHistoryLineTemp(line, sessionId);
            if (prompt && str::Len(prompt) > 0) {
                result = str::Dup(prompt);
            }
        }
        if (!lineEnd) {
            break;
        }
        rest.s = lineEnd.s + 1;
        while (!str::IsEmpty(rest) && (*rest.s == '\n' || *rest.s == '\r')) {
            rest.s++;
            rest.len--;
        }
    }
    data.Free();
    return result ? result : Str("(no description)");
}

static bool ParseCodexRolloutMetaLine(Str line, Str matchDir, Str* sessionIdOut) {
    if (!str::Find(line, "\"type\":\"session_meta\"")) {
        return false;
    }
    Str payload = str::Find(line, "\"payload\":");
    TempStr cwd = payload ? AIChatJsonStrTemp(payload, "cwd") : nullptr;
    TempStr id = payload ? AIChatJsonStrTemp(payload, "id") : nullptr;
    if (!cwd || !id || !CodexPathsEqual(cwd, matchDir)) {
        return false;
    }
    *sessionIdOut = str::Dup(id);
    return true;
}

static void TryAddCodexSession(Str rolloutPath, const FILETIME& ft, Str matchDir, Vec<AIChatSessionInfo>& sessions) {
    ByteSlice data = file::ReadFile(rolloutPath);
    if (data.empty()) {
        return;
    }
    Str content = AsStr(data);
    Str lineEnd = str::FindChar(content, '\n');
    if (!lineEnd) {
        lineEnd = str::FindChar(content, '\r');
    }
    int lineLen = lineEnd ? (int)(lineEnd.s - content.s) : content.len;
    if (lineLen <= 0) {
        data.Free();
        return;
    }
    TempStr line = str::DupTemp(Str(content.s, lineLen));
    Str sessionId;
    if (!ParseCodexRolloutMetaLine(line, matchDir, &sessionId)) {
        data.Free();
        return;
    }
    i64 ts = AIChatFileTimeToMs(ft);
    for (int i = 0; i < sessions.Size(); i++) {
        if (str::Eq(sessions[i].sessionId, sessionId)) {
            if (ts > sessions[i].timestamp) {
                sessions[i].timestamp = ts;
            }
            str::Free(sessionId);
            data.Free();
            return;
        }
    }
    AIChatSessionInfo si;
    si.sessionId = sessionId;
    si.display = GetCodexSessionDescription(sessionId);
    si.project = str::Dup(matchDir);
    si.timestamp = ts;
    sessions.Append(si);
    data.Free();
}

static TempStr FindCodexRolloutPathTemp(Str sessionId) {
    TempStr root = CodexSessionsRootTemp();
    if (!root || !sessionId) {
        return {};
    }
    TempStr suffix = str::FormatTemp("%s.jsonl", sessionId.s);
    TempStr result = nullptr;
    TempStr yearPat = str::FormatTemp("%s\\*", root.s);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(ToWStrTemp(yearPat), &fdY);
    if (hY == INVALID_HANDLE_VALUE) {
        return {};
    }
    do {
        if ((fdY.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr year = ToUtf8Temp(fdY.cFileName);
        if (str::Eq(year, ".") || str::Eq(year, "..")) {
            continue;
        }
        TempStr monthPat = str::FormatTemp("%s\\%s\\*", root.s, year.s);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(ToWStrTemp(monthPat), &fdM);
        if (hM == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((fdM.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            TempStr month = ToUtf8Temp(fdM.cFileName);
            if (str::Eq(month, ".") || str::Eq(month, "..")) {
                continue;
            }
            TempStr dayPat = str::FormatTemp("%s\\%s\\%s\\*", root.s, year.s, month.s);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(ToWStrTemp(dayPat), &fdD);
            if (hD == INVALID_HANDLE_VALUE) {
                continue;
            }
            do {
                if (fdD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                TempStr name = ToUtf8Temp(fdD.cFileName);
                if (str::EndsWithI(name, suffix)) {
                    result = str::FormatTemp("%s\\%s\\%s\\%s", root.s, year.s, month.s, name.s);
                    FindClose(hD);
                    FindClose(hM);
                    FindClose(hY);
                    return result;
                }
            } while (FindNextFileW(hD, &fdD));
            FindClose(hD);
        } while (FindNextFileW(hM, &fdM));
        FindClose(hM);
    } while (FindNextFileW(hY, &fdY));
    FindClose(hY);
    return result;
}

// Scan ~/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl for sessions with matching cwd
static void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr root = CodexSessionsRootTemp();
    if (!root || !dir::Exists(Str(root))) {
        return;
    }

    TempStr yearPat = str::FormatTemp("%s\\*", root.s);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(ToWStrTemp(yearPat), &fdY);
    if (hY == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if ((fdY.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr year = ToUtf8Temp(fdY.cFileName);
        if (str::Eq(year, ".") || str::Eq(year, "..")) {
            continue;
        }
        TempStr monthPat = str::FormatTemp("%s\\%s\\*", root.s, year.s);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(ToWStrTemp(monthPat), &fdM);
        if (hM == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((fdM.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            TempStr month = ToUtf8Temp(fdM.cFileName);
            if (str::Eq(month, ".") || str::Eq(month, "..")) {
                continue;
            }
            TempStr dayPat = str::FormatTemp("%s\\%s\\%s\\*", root.s, year.s, month.s);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(ToWStrTemp(dayPat), &fdD);
            if (hD == INVALID_HANDLE_VALUE) {
                continue;
            }
            do {
                if (fdD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                TempStr name = ToUtf8Temp(fdD.cFileName);
                if (!IsCodexRolloutFileName(name)) {
                    continue;
                }
                TempStr fullPath = str::FormatTemp("%s\\%s\\%s\\%s", root.s, year.s, month.s, name.s);
                TryAddCodexSession(fullPath, fdD.ftLastWriteTime, dir, sessions);
            } while (FindNextFileW(hD, &fdD));
            FindClose(hD);
        } while (FindNextFileW(hM, &fdM));
        FindClose(hM);
    } while (FindNextFileW(hY, &fdY));
    FindClose(hY);

    AIChatSortSessionsByTimestampDesc(sessions);
}

static bool gPopulatingCombo = false; // guard against re-entrant CBN_SELCHANGE

// populate the session combo box for the current tab's directory
static void PopulateSessionCombo(MainWindow* win) {
    HWND combo = win->hwndCodexSessionCombo;
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
    for (int i = 0; i < sessions.Size(); i++) {
        Str display = sessions[i].display;
        if (str::IsEmpty(display)) {
            display = "(no description)";
        }
        TempStr label = ShortenStringUtf8Temp(display, 50);
        TempWStr labelW = ToWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW.s);

        if (tab->codexSessionId && str::Eq(tab->codexSessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (tab->codexSessionId && !foundCurrent) {
        Str label = "(current session)";
        TempWStr labelW = ToWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW.s);
        selectedIdx = sessions.Size() + 1;
    }

    SendMessageW(combo, CB_SETCURSEL, selectedIdx, 0);
    AIChatFreeSessions(sessions);

    gPopulatingCombo = false;
}

static bool IsCodexInjectedUserText(Str text) {
    if (!text) {
        return true;
    }
    if (str::Find(text, "# AGENTS.md")) {
        return true;
    }
    if (str::Find(text, "<environment_context>")) {
        return true;
    }
    if (str::Find(text, "<turn_aborted>")) {
        return true;
    }
    if (str::Find(text, "<INSTRUCTIONS>")) {
        return true;
    }
    return false;
}

static TempStr ExtractCodexRolloutUserTextTemp(Str line) {
    if (!str::Find(line, "\"type\":\"response_item\"")) {
        return {};
    }
    if (!str::Find(line, "\"role\":\"user\"")) {
        return {};
    }
    Str inputText = str::Find(line, "\"input_text\"");
    if (!inputText) {
        return {};
    }
    TempStr text = AIChatJsonStrTemp(inputText, "text");
    if (!text || IsCodexInjectedUserText(text)) {
        return {};
    }
    str::TrimWSInPlace(text, str::TrimOpt::Both);
    return str::Len(text) > 0 ? text : nullptr;
}

static TempStr ExtractCodexRolloutAssistantTextTemp(Str line) {
    if (!str::Find(line, "\"type\":\"response_item\"")) {
        return {};
    }
    if (!str::Find(line, "\"role\":\"assistant\"")) {
        return {};
    }
    Str outputText = str::Find(line, "\"output_text\"");
    if (!outputText) {
        return {};
    }
    return AIChatJsonStrTemp(outputText, "text");
}

static void AppendCodexRolloutTools(MainWindow* win, Str line) {
    if (!str::Find(line, "\"type\":\"response_item\"")) {
        return;
    }
    TempStr name = nullptr;
    if (str::Find(line, "\"type\":\"function_call\"")) {
        name = AIChatJsonStrTemp(line, "name");
    } else if (str::Find(line, "\"type\":\"custom_tool_call\"")) {
        name = AIChatJsonStrTemp(line, "name");
    }
    if (name && str::Len(name) > 0) {
        StrBuilder desc;
        desc.AppendFmt("Tool: %s", name.s);
        WebViewAddTool(win, desc.Get());
    }
}

// Load conversation history from Codex rollout JSONL
static void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    (void)dir;
    TempStr sessionPath = FindCodexRolloutPathTemp(sessionId);
    if (!sessionPath || !file::Exists(sessionPath)) {
        return;
    }

    ByteSlice data = file::ReadFile(sessionPath);
    if (data.empty()) {
        return;
    }

    Str content = AsStr(data);
    Str rest = content;

    while (!str::IsEmpty(rest)) {
        Str lineEnd = str::FindChar(rest, '\n');
        if (!lineEnd) {
            lineEnd = str::FindChar(rest, '\r');
        }
        int lineLen = lineEnd ? (int)(lineEnd.s - rest.s) : rest.len;
        if (lineLen > 0) {
            TempStr line = str::DupTemp(Str(rest.s, lineLen));
            TempStr userText = ExtractCodexRolloutUserTextTemp(line);
            if (userText) {
                WebViewAddUser(win, userText);
            } else {
                TempStr assistantText = ExtractCodexRolloutAssistantTextTemp(line);
                if (assistantText && str::Len(assistantText) > 0) {
                    WebViewAppendText(win, assistantText);
                    WebViewFlushBlock(win);
                } else {
                    AppendCodexRolloutTools(win, line);
                }
            }
        }
        if (!lineEnd) {
            break;
        }
        rest.s = lineEnd.s + 1;
        while (!str::IsEmpty(rest) && (*rest.s == '\n' || *rest.s == '\r')) {
            rest.s++;
            rest.len--;
        }
    }

    data.Free();
}

// handle combo selection change
static void OnSessionComboChange(MainWindow* win) {
    if (gPopulatingCombo) {
        return; // ignore changes triggered by PopulateSessionCombo
    }

    HWND combo = win->hwndCodexSessionCombo;
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }

    if (sel == 0) {
        // "New Session" — clear current session
        CodexBuildLog("session", "new");
        str::ReplaceWithCopy(&tab->codexSessionId, Str{});
        delete tab->codexChatLog;
        tab->codexChatLog = nullptr;
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < sessions.Size()) {
        CodexBuildLog("session", sessions[sessionIdx].sessionId);
        str::ReplaceWithCopy(&tab->codexSessionId, sessions[sessionIdx].sessionId);
        delete tab->codexChatLog;
        tab->codexChatLog = nullptr;
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->codexSessionId, dir);
        // LoadSessionHistory calls WebViewEval which rebuilds codexChatLog
    }

    AIChatFreeSessions(sessions);
}

// --- Stream JSON events ---
enum class CodexUpdateType {
    Text,
    Tool,
    Error,
    Flush,
    SessionId,
    Finished,
};

static Str kCodexPendingSessionId() {
    return Str("pending");
}

struct CodexUpdateData {
    HWND hwndFrame;
    Str text;
    Str sessionId; // to identify which tab this belongs to
    CodexUpdateType updateType;
};

static void OnCodexUpdate(CodexUpdateData* data) {
    MainWindow* win = AIChatFindMainWindowByFrame(data->hwndFrame);
    if (!win || !IsMainWindowValid(win) || !win->hwndCodexBox) {
        str::Free(data->text);
        str::Free(data->sessionId);
        free(data);
        return;
    }
    {
        WindowTab* tab = nullptr;
        for (WindowTab* t : win->Tabs()) {
            if (!t || !t->codexProcess) {
                continue;
            }
            if (data->sessionId && t->codexSessionId && str::Eq(t->codexSessionId, data->sessionId)) {
                tab = t;
                break;
            }
            if (data->sessionId && str::Eq(data->sessionId, kCodexPendingSessionId()) && !t->codexSessionId) {
                tab = t;
                break;
            }
        }
        if (!tab) {
            for (WindowTab* t : win->Tabs()) {
                if (t && t->codexSessionId && data->sessionId && str::Eq(t->codexSessionId, data->sessionId)) {
                    tab = t;
                    break;
                }
            }
        }
        bool isActiveTab = (tab && tab == win->CurrentTab());

        switch (data->updateType) {
            case CodexUpdateType::Text:
                if (data->text) {
                    CodexBuildLog("<<< text", data->text);
                }
                if (isActiveTab) {
                    WebViewAppendText(win, data->text);
                }
                break;
            case CodexUpdateType::Tool:
                if (data->text) {
                    CodexBuildLog("<<< tool", data->text);
                }
                if (isActiveTab) {
                    WebViewAddTool(win, data->text);
                }
                break;
            case CodexUpdateType::Error:
                if (isActiveTab) {
                    WebViewAddError(win, data->text);
                } else if (data->text) {
                    CodexBuildLog("error", data->text);
                }
                break;
            case CodexUpdateType::Flush:
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                }
                break;
            case CodexUpdateType::SessionId:
                if (data->text) {
                    CodexBuildLog("<<< session", data->text);
                }
                if (tab && data->text) {
                    str::ReplaceWithCopy(&tab->codexSessionId, data->text);
                }
                break;
            case CodexUpdateType::Finished:
                if (tab && tab->codexProcess) {
                    if (WaitForSingleObject(tab->codexProcess, 0) == WAIT_OBJECT_0) {
                        DWORD exitCode = 0;
                        GetExitCodeProcess(tab->codexProcess, &exitCode);
                        CodexBuildLog("exit", str::FormatTemp("%lu", exitCode));
                    }
                    CloseCodexProcess(tab, false);
                }
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                    SetCodexWorking(win, false);
                    PopulateSessionCombo(win);
                }
                break;
        }
    }
    str::Free(data->text);
    str::Free(data->sessionId);
    free(data);
}

static void PostUpdate(HWND hwndFrame, Str sessionId, Str text, CodexUpdateType type) {
    auto data = (CodexUpdateData*)calloc(1, sizeof(CodexUpdateData));
    data->hwndFrame = hwndFrame;
    data->sessionId = sessionId ? str::Dup(sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnCodexUpdate, data));
}

struct CodexReadCtx {
    HANDLE hReadPipe;
    HWND hwndFrame;
    Str sessionId;
};

static void CodexReadThread(CodexReadCtx* ctx) {
    HANDLE hPipe = ctx->hReadPipe;
    HWND hwndFrame = ctx->hwndFrame;
    Str sessionId = ctx->sessionId;
    free(ctx);

    StrBuilder lineBuf;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                Str line = lineBuf.LendData();
                if (line) {
                    CodexBuildLog("<<<", line);
                }
                if (!str::IsEmpty(line) && line.s[0] == '{') {
                    TempStr eventType = AIChatJsonStrTemp(line, "type");

                    if (eventType && str::Eq(eventType, "thread.started")) {
                        TempStr threadId = AIChatJsonStrTemp(line, "thread_id");
                        if (threadId) {
                            PostUpdate(hwndFrame, sessionId, threadId, CodexUpdateType::SessionId);
                            str::Free(sessionId);
                            sessionId = str::Dup(threadId);
                        }
                    } else if (eventType && str::Eq(eventType, "item.completed")) {
                        if (str::Find(line, "\"type\":\"agent_message\"")) {
                            Str p = str::Find(line, "\"type\":\"agent_message\"");
                            TempStr text = AIChatJsonStrTemp(p, "text");
                            if (text && str::Len(text) > 0) {
                                PostUpdate(hwndFrame, sessionId, text, CodexUpdateType::Text);
                            }
                        } else if (str::Find(line, "\"type\":\"command_execution\"")) {
                            Str p = str::Find(line, "\"type\":\"command_execution\"");
                            TempStr cmd = AIChatJsonStrTemp(p, "command");
                            if (cmd && str::Len(cmd) > 0) {
                                TempStr shortCmd = ShortenStringUtf8Temp(cmd, 80);
                                StrBuilder desc;
                                desc.AppendFmt("Tool: %s", shortCmd.s);
                                PostUpdate(hwndFrame, sessionId, desc.LendData(), CodexUpdateType::Tool);
                                PostUpdate(hwndFrame, sessionId, {}, CodexUpdateType::Flush);
                            }
                        }
                    } else if (eventType && str::Eq(eventType, "turn.completed")) {
                        PostUpdate(hwndFrame, sessionId, {}, CodexUpdateType::Flush);
                    }
                }

                lineBuf.Reset();
            } else if (buf[i] != '\r') {
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    Str rem = lineBuf.LendData();
    if (rem) {
        CodexBuildLog("<<<", rem);
    }
    CodexBuildLog("eof", "(stdout closed)");

    CloseHandle(hPipe);
    PostUpdate(hwndFrame, sessionId, {}, CodexUpdateType::Finished);
    str::Free(sessionId);
}

static void StartCodexReadThread(CodexReadCtx* ctx) {
    CodexReadThread(ctx);
}

static void SendCodexMessage(MainWindow* win) {
    if (!win->codexInput) {
        return;
    }
    if (!IsCodexBuildSupportedForTab(win->CurrentTab())) {
        return;
    }
    HWND hwndInput = win->codexInput->hwnd;
    int inputLen = GetWindowTextLengthW(hwndInput);
    if (inputLen == 0) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }
    if (tab->codexProcess) {
        return; // this tab already has a running request
    }

    TempWStr inputW = HwndGetTextWTemp(hwndInput);
    TempStr input = ToUtf8Temp(inputW);
    SetWindowTextW(hwndInput, L"");

    WebViewAddUser(win, input);
    SetCodexWorking(win, true);

    bool isNewSession = (tab->codexSessionId == nullptr);

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);

    TempStr prompt = str::FormatTemp("The user is currently reading the file: %s\n\n%s", filePath.s, input.s);
    TempStr escapedInput = str::ReplaceTemp(prompt, "\"", "\\\"");

    SyncCodexSettingsFromUI(win);

    Str sandboxes[] = {Str("read-only"), Str("workspace-write"), Str("danger-full-access")};
    StrVec modelList;
    BuildCodexModelsList(modelList);
    Str model = ResolveCodexModel(modelList, gGlobalPrefs->codexBuild.model);
    int sandboxIdx = gGlobalPrefs->codexBuild.sandbox;
    if (sandboxIdx < 0 || sandboxIdx > 2) {
        sandboxIdx = 1;
    }
    Str skipFlag = gGlobalPrefs->codexBuild.skipSandbox ? Str("--dangerously-bypass-approvals-and-sandbox") : Str{};

    TempStr codexPath = FindCodexExecutableTemp();
    if (!codexPath) {
        CodexBuildLog("error", "Cannot find codex executable");
        WebViewAddError(win, "Cannot find codex. Is OpenAI Codex installed?");
        SetCodexWorking(win, false);
        return;
    }

    CodexBuildLog(">>> user", input);
    CodexBuildLog(">>> session",
                  str::FormatTemp("%s (%s)", tab->codexSessionId ? tab->codexSessionId.s : kCodexPendingSessionId().s,
                                  isNewSession ? "new" : "resume"));
    CodexBuildLog(">>> cwd", dir);

    TempStr cmdLine;
    if (isNewSession) {
        if (skipFlag) {
            cmdLine = str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s %s \"%s\"",
                                      codexPath.s, dir.s, model.s, sandboxes[sandboxIdx].s, skipFlag.s, escapedInput.s);
        } else {
            cmdLine = str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s \"%s\"",
                                      codexPath.s, dir.s, model.s, sandboxes[sandboxIdx].s, escapedInput.s);
        }
    } else if (skipFlag) {
        cmdLine = str::FormatTemp("\"%s\" exec resume --json --skip-git-repo-check -m %s %s %s \"%s\"", codexPath.s,
                                  model.s, skipFlag.s, tab->codexSessionId.s, escapedInput.s);
    } else {
        cmdLine = str::FormatTemp("\"%s\" exec resume --json --skip-git-repo-check -m %s %s \"%s\"", codexPath.s,
                                  model.s, tab->codexSessionId.s, escapedInput.s);
    }

    CodexBuildLog(">>> cmd", cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        CodexBuildLog("error", "Failed to launch codex process");
        WebViewAddError(win, "Failed to launch codex. Is it installed and in PATH?");
        SetCodexWorking(win, false);
        return;
    }

    tab->codexProcess = launch.hProcess;
    CodexBuildLog(">>> start", str::FormatTemp("pid %lu", launch.processId));

    auto ctx = (CodexReadCtx*)calloc(1, sizeof(CodexReadCtx));
    ctx->hReadPipe = launch.hReadPipe;
    ctx->hwndFrame = win->hwndFrame;
    ctx->sessionId = str::Dup(tab->codexSessionId ? tab->codexSessionId : kCodexPendingSessionId());
    RunAsync(MkFunc0(StartCodexReadThread, ctx), "CodexReadThread");
}

static TempStr FitCodexPanelTitleTemp(HWND labelHwnd, HFONT font, Str docName, int maxDx) {
    TempStr prefix = str::JoinTemp(_TRA("Codex chat"), " with ");
    return AIChatFitPanelTitleTemp(labelHwnd, font, prefix, docName, maxDx);
}

static void UpdateCodexPanelTitle(MainWindow* win, int labelDx) {
    if (!win || !win->codexLabelWithClose) {
        return;
    }
    Str docName = "document";
    WindowTab* tab = win->CurrentTab();
    if (tab && !tab->IsAboutTab() && tab->filePath) {
        Str title = tab->GetTabTitle();
        if (!str::IsEmpty(title)) {
            docName = title;
        }
    }

    HWND labelHwnd = win->codexLabelWithClose->hwnd;
    HFONT font = win->codexLabelWithClose->font;
    if (!font) {
        font = GetDefaultGuiFont(true, false);
    }
    if (labelDx <= 0 && win->hwndCodexBox) {
        labelDx = ClientRect(win->hwndCodexBox).dx;
    }
    int maxDx = AIChatLabelMaxTextDx(labelHwnd, labelDx);
    TempStr label = FitCodexPanelTitleTemp(labelHwnd, font, docName, maxDx);
    win->codexLabelWithClose->SetLabel(label);
}

// --- Layout ---
static void LayoutCodexBox(MainWindow* win) {
    HWND hwndContainer = win->hwndCodexBox;
    Rect rc = ClientRect(hwndContainer);
    int y = 0;

    UpdateCodexPanelTitle(win, rc.dx);

    // label
    Size labelSize = win->codexLabelWithClose->GetIdealSize();
    MoveWindow(win->codexLabelWithClose->hwnd, 0, y, rc.dx, labelSize.dy, TRUE);
    y += labelSize.dy;

    // session combo — get actual height from font metrics
    int comboDy = 0;
    if (win->hwndCodexSessionCombo) {
        // the visible edit part of a dropdown is determined by the font
        // GetComboBoxInfo or just use SendMessage CB_GETITEMHEIGHT
        int itemH = (int)SendMessageW(win->hwndCodexSessionCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        comboDy = itemH + 8; // item height + borders
        // MoveWindow height for CBS_DROPDOWNLIST = visible height + dropdown list height
        MoveWindow(win->hwndCodexSessionCombo, 2, y + 1, rc.dx - 4, comboDy + 200, TRUE);
    }
    y += comboDy + 3;

    // bottom: input, then [Model▾][Effort▾][☐Skip]
    Size inputSize = win->codexInput->GetIdealSize();
    int inputDy = inputSize.dy + 4;
    int optRowDy = 32;
    if (win->hwndCodexModelCombo) {
        int itemH = (int)SendMessageW(win->hwndCodexModelCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        optRowDy = itemH + 8;
    }
    int bottomDy = inputDy + 4 + optRowDy;

    int webViewDy = rc.dy - y - bottomDy;
    if (webViewDy < 0) {
        webViewDy = 0;
    }

    if (win->codexWebView) {
        MoveWindow(win->codexWebView->hwnd, 0, y, rc.dx, webViewDy, TRUE);
        // defer UpdateWebviewSize during rapid WM_SIZE to avoid WebView2 put_Bounds freeze
        KillTimer(win->hwndCodexBox, 43);
        SetTimer(win->hwndCodexBox, 43, 50, nullptr);
    }
    y += webViewDy;

    // input row: [input box] [Stop] — stop button only visible when working
    int stopBtnDx = 50;
    WindowTab* curTab = win->CurrentTab();
    bool isWorking = (curTab && curTab->codexProcess != nullptr);
    if (isWorking && win->hwndCodexStopBtn) {
        MoveWindow(win->codexInput->hwnd, 0, y, rc.dx - stopBtnDx - 2, inputDy, TRUE);
        MoveWindow(win->hwndCodexStopBtn, rc.dx - stopBtnDx, y, stopBtnDx, inputDy, TRUE);
    } else {
        MoveWindow(win->codexInput->hwnd, 0, y, rc.dx, inputDy, TRUE);
    }
    y += inputDy + 4;

    // options row: [Model▾] [Effort▾] [☐Skip]
    {
        int x = 2;
        int thirdDx = (rc.dx - 8) / 3;
        if (win->hwndCodexModelCombo) {
            MoveWindow(win->hwndCodexModelCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndCodexSandboxCombo) {
            MoveWindow(win->hwndCodexSandboxCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndCodexSkipSandboxCheck) {
            MoveWindow(win->hwndCodexSkipSandboxCheck, x + 8, y, rc.dx - x - 10, optRowDy, TRUE);
        }
    }
}

// --- WndProc ---
static LRESULT CALLBACK WndProcCodexInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                          DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !IsShiftPressed()) {
        SendCodexMessage(win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WndProcCodexBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
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
            LayoutCodexBox(win);
            break;
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_CODEX_LABEL_WITH_CLOSE) {
                ToggleCodexPanel(win);
            }
            if (LOWORD(wp) == IDC_CODEX_STOP_BTN) {
                StopCodex(win);
            }
            if (LOWORD(wp) == IDC_CODEX_SESSION_COMBO && HIWORD(wp) == CBN_SELCHANGE) {
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
                if (win->codexWebView) {
                    win->codexWebView->UpdateWebviewSize();
                }
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// --- Splitter ---
constexpr int kCodexMinDx = 150;

static void OnCodexSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = ClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    if (dx < kCodexMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    AIChatUpdateSidebarDx(win, dx, ev->finishedDragging);
    if (ev->finishedDragging) {
        RelayoutForCodexSplitter(win);
    }
}

void RelayoutCodexPanel(MainWindow* win) {
    if (!win || !win->hwndCodexBox || !win->codexVisible) {
        return;
    }
    LayoutCodexBox(win);
    KillTimer(win->hwndCodexBox, 43);
    if (win->codexWebView && win->codexWebViewReady) {
        win->codexWebView->UpdateWebviewSize();
    }
    RedrawWindow(win->hwndCodexBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (win->codexSplitter && win->codexSplitter->hwnd) {
        InvalidateRect(win->codexSplitter->hwnd, nullptr, TRUE);
    }
}

// --- Lazy WebView2 init ---
static void EnsureWebViewReady(MainWindow* win) {
    if (win->codexWebViewReady) {
        return;
    }
    if (!HasWebView()) {
        return;
    }
    auto webView = new WebviewWnd();
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA);
    // use unique data dir per process to avoid locking conflicts
    webView->dataDir = str::Format("%s\\SumatraPDF\\CodexWebView_%d", userProfile.s, (int)GetCurrentProcessId());
    if (!LockDataResource(IDR_CLAUDE_MARKED_JS, &gCodexMarkedJs)) {
        delete webView;
        return;
    }
    str::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = str::Dup(kCodexVirtualHostW);
    webView->resourceProvider.ctx = &gCodexMarkedJs;
    webView->resourceProvider.getResource = AIChatGetMarkedJsResource;

    Rect rc = ClientRect(win->hwndCodexBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndCodexBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr chatHtml = AIChatFormatChatHtmlTemp(kCodexVirtualHost(), CodexBgColor());
        webView->SetHtml(chatHtml);
        win->codexWebView = webView;
        win->codexWebViewReady = true;
        RelayoutCodexPanel(win);
    } else {
        delete webView;
    }
}

// --- Public API ---
void CreateCodexPanel(MainWindow* win) {
    if (!IsCodexBuildAvailable()) {
        return;
    }
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndCodexBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    // splitter (non-live: only resize on mouse release)
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->codexSplitter = new Splitter();
        win->codexSplitter->onMove = MkFunc1Void(OnCodexSplitterMove);
        win->codexSplitter->Create(args);
    }

    // label
    auto label = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndCodexBox;
        args.cmdId = IDC_CODEX_LABEL_WITH_CLOSE;
        args.isRtl = IsUIRtl();
        args.font = GetDefaultGuiFont(true, false);
        label->Create(args);
    }
    win->codexLabelWithClose = label;
    label->SetPaddingXY(2, 2);
    UpdateCodexPanelTitle(win, 0);

    // session combo
    win->hwndCodexSessionCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, dx, 200,
                        win->hwndCodexBox, (HMENU)(UINT_PTR)IDC_CODEX_SESSION_COMBO, hmod, nullptr);
    SendMessageW(win->hwndCodexSessionCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // webview deferred
    win->codexWebView = nullptr;
    win->codexWebViewReady = false;

    // model combo
    win->hwndCodexModelCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200,
                        win->hwndCodexBox, (HMENU)(UINT_PTR)IDC_CODEX_MODEL_COMBO, hmod, nullptr);
    SendMessageW(win->hwndCodexModelCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // sandbox combo
    win->hwndCodexSandboxCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200,
                        win->hwndCodexBox, (HMENU)(UINT_PTR)IDC_CODEX_SANDBOX_COMBO, hmod, nullptr);
    SendMessageW(win->hwndCodexSandboxCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    SendMessageW(win->hwndCodexSandboxCombo, CB_ADDSTRING, 0, (LPARAM)L"Read-only");
    SendMessageW(win->hwndCodexSandboxCombo, CB_ADDSTRING, 0, (LPARAM)L"Workspace write");
    SendMessageW(win->hwndCodexSandboxCombo, CB_ADDSTRING, 0, (LPARAM)L"Full access");
    SendMessageW(win->hwndCodexSandboxCombo, CB_SETCURSEL, 1, 0);

    // skip-permissions checkbox
    win->hwndCodexSkipSandboxCheck =
        CreateWindowExW(0, L"BUTTON", L"Skip Sandbox", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 160, 20,
                        win->hwndCodexBox, (HMENU)(UINT_PTR)IDC_CODEX_SKIP_SANDBOX, hmod, nullptr);
    SendMessageW(win->hwndCodexSkipSandboxCheck, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // stop button (hidden by default, shown when agent is working)
    win->hwndCodexStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | BS_PUSHBUTTON, 0, 0, 50, 24,
                                            win->hwndCodexBox, (HMENU)(UINT_PTR)IDC_CODEX_STOP_BTN, hmod, nullptr);
    SendMessageW(win->hwndCodexStopBtn, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    ShowWindow(win->hwndCodexStopBtn, SW_HIDE);

    // input box
    auto input = new Edit();
    {
        Edit::CreateArgs args;
        args.parent = win->hwndCodexBox;
        args.isMultiLine = true;
        args.idealSizeLines = 3;
        args.withBorder = true;
        args.cueText = "Ask about this document...";
        input->Create(args);
    }
    win->codexInput = input;

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(input->hwnd, WndProcCodexInput, inputSubclassId, (DWORD_PTR)win);

    win->codexBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndCodexBox, WndProcCodexBox, win->codexBoxSubclassId, (DWORD_PTR)win);

    ApplyCodexSettingsToUI(win);
    AIChatApplySavedSidebarDx(win);
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || tab->codexSessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    if (sessions.Size() > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        str::ReplaceWithCopy(&tab->codexSessionId, sessions[0].sessionId);

        // load its history
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->codexSessionId, dir);
    }

    AIChatFreeSessions(sessions);
}

void OnAIChatWithOpenAICodex(MainWindow* win) {
    if (!IsCodexBuildAvailable()) {
        return;
    }
    if (!IsCodexBuildInstalled()) {
        ShowCodexBuildNotInstalledDialog();
        return;
    }
    ToggleCodexPanel(win);
}

void ToggleCodexPanel(MainWindow* win) {
    if (!IsCodexBuildAvailable() || !win->hwndCodexBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    if (AIChatGetTabPanelOpen(tab) == AIChatBackend::Codex) {
        AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    } else {
        if (!IsCodexBuildSupportedForTab(tab)) {
            return;
        }
        AIChatSetTabPanelOpen(tab, AIChatBackend::Codex);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->codexVisible) {
        UpdateCodexPanelTitle(win, 0);
        EnsureWebViewReady(win);
        UpdateCodexPanelForCurrentTab(win);
        PopulateSessionCombo(win);
        if (win->codexInput) {
            HwndSetFocus(win->codexInput->hwnd);
        }
        // defer auto-select so SetHtml has time to load the page
        SetTimer(win->hwndCodexBox, 42, 500, nullptr);
    }
    RelayoutWindow(win);
}

// call when switching tabs to update session context
void OnCodexTabChanged(MainWindow* win) {
    UpdateCodexPanelTitle(win, 0);
    WindowTab* tab = win->CurrentTab();
    bool supported = IsCodexBuildSupportedForTab(tab);
    UpdateCodexPanelForCurrentTab(win);

    if (!win->codexVisible) {
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
    SetCodexWorking(win, tab->codexProcess != nullptr);

    // if tab has in-memory chat log, replay it (fast, includes current session)
    if (tab->codexChatLog && !tab->codexChatLog->IsEmpty()) {
        ReplayChatLog(win, tab);
    } else if (tab->filePath && tab->codexSessionId) {
        // fallback: load from disk
        TempStr dir = path::GetDirTemp(tab->filePath);
        LoadSessionHistory(win, tab->codexSessionId, dir);
    }
}

static bool CodexTabHasRunningProcess(WindowTab* tab) {
    return tab && tab->codexProcess;
}

void ShutdownCodexForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        CloseCodexProcess(tab, true);
    }
    AIChatWaitForTabProcessesToFinish(win, CodexTabHasRunningProcess);
}

void DestroyCodexPanel(MainWindow* win) {
    win->codexWebViewReady = false;

    if (win->hwndCodexBox) {
        KillTimer(win->hwndCodexBox, 42);
        KillTimer(win->hwndCodexBox, 43);
        if (win->codexBoxSubclassId) {
            RemoveWindowSubclass(win->hwndCodexBox, WndProcCodexBox, win->codexBoxSubclassId);
            win->codexBoxSubclassId = 0;
        }
    }

    // save webview dataDir before deleting so we can clean up
    Str webViewDataDir;
    WebviewWnd* webView = win->codexWebView;
    win->codexWebView = nullptr;
    if (webView) {
        webViewDataDir = str::Dup(webView->dataDir);
    }

    delete win->codexLabelWithClose;
    win->codexLabelWithClose = nullptr;
    delete webView;
    delete win->codexInput;
    win->codexInput = nullptr;
    delete win->codexSplitter;
    win->codexSplitter = nullptr;

    if (win->hwndCodexBox) {
        DestroyWindow(win->hwndCodexBox);
        win->hwndCodexBox = nullptr;
    }
    win->hwndCodexSessionCombo = nullptr;
    win->hwndCodexModelCombo = nullptr;
    win->hwndCodexSandboxCombo = nullptr;
    win->hwndCodexSkipSandboxCheck = nullptr;
    win->hwndCodexStopBtn = nullptr;

    // clean up per-process WebView2 cache dir
    if (webViewDataDir) {
        dir::RemoveAll(webViewDataDir);
        str::Free(webViewDataDir);
    }
}
