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
#include "Theme.h"
#include "DarkModeSubclass.h"
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
        candidates.Append(fmt("%s\\.codex\\bin\\codex.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\codex.exe", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"codex.exe"), WStr(L"codex"));
}

bool IsCodexBuildInstalled() {
    return len(FindCodexExecutableTemp()) > 0;
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
    return StrL("/AI-Chat-with-document#openai-codex");
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
    return StrL("https://sumatrapdf.codex/");
}
constexpr const WCHAR* kCodexVirtualHostW = L"https://sumatrapdf.codex/";

static LoadedDataResource gCodexMarkedJs;

static Str CodexBgColor() {
    Str bg = gGlobalPrefs->codexBuild.bgColor;
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

static void BuildCodexModelsList(StrVec& models) {
    models.Reset();
    AIChatAppendModelUnique(models, "gpt-5.5");
    AIChatAppendModelUnique(models, "gpt-5.4");
    AIChatAppendModelUnique(models, "o3");
    Str extra = gGlobalPrefs->codexBuild.models;
    if (len(extra) > 0) {
        StrVec parts;
        Split(&parts, extra, ",", true);
        for (int i = 0; i < len(parts); i++) {
            AIChatAppendModelUnique(models, parts[i]);
        }
    }
}

static Str ResolveCodexModel(const StrVec& models, Str model) {
    int idx = AIChatFindModelInList(models, model);
    if (idx >= 0) {
        return models[idx];
    }
    idx = AIChatFindModelInList(models, "gpt-5.5");
    if (idx >= 0) {
        return models[idx];
    }
    return StrL("gpt-5.5");
}

static void PopulateModelCombo(HWND combo) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    StrVec models;
    BuildCodexModelsList(models);
    for (int i = 0; i < len(models); i++) {
        TempStr display = AIChatModelDisplayNameTemp(models[i], "Gpt-5.5");
        WCHAR* displayW = CWStrTemp(display);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)displayW);
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
        if (sel >= 0 && sel < len(models)) {
            str::ReplaceWithCopy(&gGlobalPrefs->codexBuild.model, models[sel]);
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
        CodexBuildLog("stop", tab->codexSessionId ? tab->codexSessionId : StrL("(no session)"));
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
            tab->codexChatLog.Append(js);
            tab->codexChatLog.AppendChar('\n');
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
    CodexBuildLog("error", text);
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
    Str msg = "OpenAI Codex is only available for PDF and image files.";
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(msg));
    WebViewEval(win, js, false);
}

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, WindowTab* tab) {
    if (tab->codexChatLog.IsEmpty()) {
        return;
    }
    if (!win->codexWebView || !win->codexWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str log = ToStr(tab->codexChatLog);
    Str rest = log;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) > 0) {
            win->codexWebView->Eval(str::DupTemp(line));
        }
    }
}

// --- Session history ---
static TempStr CodexSessionsRootTemp() {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return {};
    }
    return fmt("%s\\.codex\\sessions", userProfile);
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
    TempStr historyPath = userProfile ? fmt("%s\\.codex\\history.jsonl", userProfile) : nullptr;
    if (!historyPath) {
        return StrL("(no description)");
    }
    Str data = file::ReadFile(historyPath);
    if (len(data) == 0) {
        return StrL("(no description)");
    }
    Str content = data;
    Str rest = content;
    Str result;
    Str line;

    while (!result && str::NextLine(rest, line, rest)) {
        if (len(line) == 0) {
            continue;
        }
        TempStr prompt = ExtractCodexPromptFromHistoryLineTemp(str::DupTemp(line), sessionId);
        if (len(prompt) > 0) {
            result = str::Dup(prompt);
        }
    }
    str::Free(data);
    return result ? result : StrL("(no description)");
}

static bool ParseCodexRolloutMetaLine(Str line, Str matchDir, Str* sessionIdOut) {
    if (!str::Contains(line, StrL("\"type\":\"session_meta\""))) {
        return false;
    }
    Str payload;
    str::Cut(line, StrL("\"payload\":"), nullptr, &payload);
    TempStr cwd = payload ? AIChatJsonStrTemp(payload, "cwd") : nullptr;
    TempStr id = payload ? AIChatJsonStrTemp(payload, "id") : nullptr;
    if (!cwd || !id || !CodexPathsEqual(cwd, matchDir)) {
        return false;
    }
    *sessionIdOut = str::Dup(id);
    return true;
}

static void TryAddCodexSession(Str rolloutPath, const FILETIME& ft, Str matchDir, Vec<AIChatSessionInfo>& sessions) {
    Str data = file::ReadFile(rolloutPath);
    if (len(data) == 0) {
        return;
    }
    Str content = data;
    Str firstLine, rest;
    if (!str::NextLine(content, firstLine, rest) || len(firstLine) == 0) {
        str::Free(data);
        return;
    }
    TempStr line = str::DupTemp(firstLine);
    Str sessionId;
    if (!ParseCodexRolloutMetaLine(line, matchDir, &sessionId)) {
        str::Free(data);
        return;
    }
    i64 ts = AIChatFileTimeToMs(ft);
    for (int i = 0; i < len(sessions); i++) {
        if (str::Eq(sessions[i].sessionId, sessionId)) {
            if (ts > sessions[i].timestamp) {
                sessions[i].timestamp = ts;
            }
            str::Free(sessionId);
            str::Free(data);
            return;
        }
    }
    AIChatSessionInfo si;
    si.sessionId = sessionId;
    si.display = GetCodexSessionDescription(sessionId);
    si.project = str::Dup(matchDir);
    si.timestamp = ts;
    sessions.Append(si);
    str::Free(data);
}

static TempStr FindCodexRolloutPathTemp(Str sessionId) {
    TempStr root = CodexSessionsRootTemp();
    if (!root || !sessionId) {
        return {};
    }
    TempStr suffix = fmt("%s.jsonl", sessionId);
    TempStr result = nullptr;
    TempStr yearPat = fmt("%s\\*", root);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(CWStrTemp(yearPat), &fdY);
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
        TempStr monthPat = fmt("%s\\%s\\*", root, year);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(CWStrTemp(monthPat), &fdM);
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
            TempStr dayPat = fmt("%s\\%s\\%s\\*", root, year, month);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(CWStrTemp(dayPat), &fdD);
            if (hD == INVALID_HANDLE_VALUE) {
                continue;
            }
            do {
                if (fdD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                TempStr name = ToUtf8Temp(fdD.cFileName);
                if (str::EndsWithI(name, suffix)) {
                    result = fmt("%s\\%s\\%s\\%s", root, year, month, name);
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
    if (!root || !dir::Exists(root)) {
        return;
    }

    TempStr yearPat = fmt("%s\\*", root);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(CWStrTemp(yearPat), &fdY);
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
        TempStr monthPat = fmt("%s\\%s\\*", root, year);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(CWStrTemp(monthPat), &fdM);
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
            TempStr dayPat = fmt("%s\\%s\\%s\\*", root, year, month);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(CWStrTemp(dayPat), &fdD);
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
                TempStr fullPath = fmt("%s\\%s\\%s\\%s", root, year, month, name);
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
    for (int i = 0; i < len(sessions); i++) {
        Str display = sessions[i].display;
        if (len(display) == 0) {
            display = "(no description)";
        }
        TempStr label = ShortenStringUtf8Temp(display, 50);
        WCHAR* labelW = CWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);

        if (tab->codexSessionId && str::Eq(tab->codexSessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (tab->codexSessionId && !foundCurrent) {
        Str label = "(current session)";
        WCHAR* labelW = CWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);
        selectedIdx = len(sessions) + 1;
    }

    SendMessageW(combo, CB_SETCURSEL, selectedIdx, 0);
    AIChatFreeSessions(sessions);

    gPopulatingCombo = false;
}

static bool IsCodexInjectedUserText(Str text) {
    if (!text) {
        return true;
    }
    if (str::Contains(text, StrL("# AGENTS.md"))) {
        return true;
    }
    if (str::Contains(text, StrL("<environment_context>"))) {
        return true;
    }
    if (str::Contains(text, StrL("<turn_aborted>"))) {
        return true;
    }
    if (str::Contains(text, StrL("<INSTRUCTIONS>"))) {
        return true;
    }
    return false;
}

static TempStr ExtractCodexRolloutUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return {};
    }
    if (!str::Contains(line, StrL("\"role\":\"user\""))) {
        return {};
    }
    Str inputText;
    if (!str::Cut(line, StrL("\"input_text\""), nullptr, &inputText)) {
        return {};
    }
    TempStr text = AIChatJsonStrTemp(inputText, "text");
    if (!text || IsCodexInjectedUserText(text)) {
        return {};
    }
    text.len -= str::TrimWSInPlace(text, str::TrimOpt::Both);
    return len(text) > 0 ? text : nullptr;
}

static TempStr ExtractCodexRolloutAssistantTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return {};
    }
    if (!str::Contains(line, StrL("\"role\":\"assistant\""))) {
        return {};
    }
    Str outputText;
    if (!str::Cut(line, StrL("\"output_text\""), nullptr, &outputText)) {
        return {};
    }
    return AIChatJsonStrTemp(outputText, "text");
}

static void AppendCodexRolloutTools(MainWindow* win, Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return;
    }
    TempStr name = nullptr;
    if (str::Contains(line, StrL("\"type\":\"function_call\""))) {
        name = AIChatJsonStrTemp(line, "name");
    } else if (str::Contains(line, StrL("\"type\":\"custom_tool_call\""))) {
        name = AIChatJsonStrTemp(line, "name");
    }
    if (len(name) > 0) {
        str::Builder desc;
        desc.Append(fmt("Tool: %s", name));
        WebViewAddTool(win, ToStr(desc));
    }
}

// Load conversation history from Codex rollout JSONL
static void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    (void)dir;
    TempStr sessionPath = FindCodexRolloutPathTemp(sessionId);
    if (!sessionPath || !file::Exists(sessionPath)) {
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
            TempStr userText = ExtractCodexRolloutUserTextTemp(line);
            if (userText) {
                WebViewAddUser(win, userText);
            } else {
                TempStr assistantText = ExtractCodexRolloutAssistantTextTemp(line);
                if (len(assistantText) > 0) {
                    WebViewAppendText(win, assistantText);
                    WebViewFlushBlock(win);
                } else {
                    AppendCodexRolloutTools(win, line);
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
        tab->codexChatLog.Reset();
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < len(sessions)) {
        CodexBuildLog("session", sessions[sessionIdx].sessionId);
        str::ReplaceWithCopy(&tab->codexSessionId, sessions[sessionIdx].sessionId);
        tab->codexChatLog.Reset();
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
    return StrL("pending");
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
                        CodexBuildLog("exit", fmt("%lu", exitCode));
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

    str::Builder lineBuf;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                Str line = ToStr(lineBuf);
                if (line) {
                    CodexBuildLog("<<<", line);
                }
                if (len(line) > 0 && line.s[0] == '{') {
                    TempStr eventType = AIChatJsonStrTemp(line, "type");

                    if (eventType && str::Eq(eventType, "thread.started")) {
                        TempStr threadId = AIChatJsonStrTemp(line, "thread_id");
                        if (threadId) {
                            PostUpdate(hwndFrame, sessionId, threadId, CodexUpdateType::SessionId);
                            str::ReplaceWithCopy(&sessionId, threadId);
                        }
                    } else if (eventType && str::Eq(eventType, "item.completed")) {
                        Str p;
                        if (str::Cut(line, StrL("\"type\":\"agent_message\""), nullptr, &p)) {
                            TempStr text = AIChatJsonStrTemp(p, "text");
                            if (len(text) > 0) {
                                PostUpdate(hwndFrame, sessionId, text, CodexUpdateType::Text);
                            }
                        } else if (str::Cut(line, StrL("\"type\":\"command_execution\""), nullptr, &p)) {
                            TempStr cmd = AIChatJsonStrTemp(p, "command");
                            if (len(cmd) > 0) {
                                TempStr shortCmd = ShortenStringUtf8Temp(cmd, 80);
                                str::Builder desc;
                                desc.Append(fmt("Tool: %s", shortCmd));
                                PostUpdate(hwndFrame, sessionId, ToStr(desc), CodexUpdateType::Tool);
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

    Str rem = ToStr(lineBuf);
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
    HwndSetText(hwndInput, "");

    WebViewAddUser(win, input);
    SetCodexWorking(win, true);

    bool isNewSession = len(tab->codexSessionId) == 0;

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);

    TempStr prompt = fmt("The user is currently reading the file: %s\n\n%s", filePath, input);
    TempStr escapedInput = str::ReplaceTemp(prompt, StrL("\""), StrL("\\\""));

    SyncCodexSettingsFromUI(win);

    Str sandboxes[] = {StrL("read-only"), StrL("workspace-write"), StrL("danger-full-access")};
    StrVec modelList;
    BuildCodexModelsList(modelList);
    Str model = ResolveCodexModel(modelList, gGlobalPrefs->codexBuild.model);
    int sandboxIdx = gGlobalPrefs->codexBuild.sandbox;
    if (sandboxIdx < 0 || sandboxIdx > 2) {
        sandboxIdx = 1;
    }
    Str skipFlag = gGlobalPrefs->codexBuild.skipSandbox ? StrL("--dangerously-bypass-approvals-and-sandbox") : Str{};

    TempStr codexPath = FindCodexExecutableTemp();
    if (!codexPath) {
        CodexBuildLog("error", "Cannot find codex executable");
        WebViewAddError(win, "Cannot find codex. Is OpenAI Codex installed?");
        SetCodexWorking(win, false);
        return;
    }

    CodexBuildLog(">>> user", input);
    CodexBuildLog(">>> session", fmt("%s (%s)", tab->codexSessionId ? tab->codexSessionId : kCodexPendingSessionId(),
                                     Str(isNewSession ? "new" : "resume")));
    CodexBuildLog(">>> cwd", dir);

    TempStr cmdLine;
    if (isNewSession) {
        if (skipFlag) {
            cmdLine = fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s %s \"%s\"", codexPath, dir,
                          model, sandboxes[sandboxIdx], skipFlag, escapedInput);
        } else {
            cmdLine = fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s \"%s\"", codexPath, dir,
                          model, sandboxes[sandboxIdx], escapedInput);
        }
    } else if (skipFlag) {
        cmdLine = fmt("\"%s\" exec resume --json --skip-git-repo-check -m %s %s %s \"%s\"", codexPath, model, skipFlag,
                      tab->codexSessionId, escapedInput);
    } else {
        cmdLine = fmt("\"%s\" exec resume --json --skip-git-repo-check -m %s %s \"%s\"", codexPath, model,
                      tab->codexSessionId, escapedInput);
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
    CodexBuildLog(">>> start", fmt("pid %lu", launch.processId));

    auto ctx = (CodexReadCtx*)calloc(1, sizeof(CodexReadCtx));
    ctx->hReadPipe = launch.hReadPipe;
    ctx->hwndFrame = win->hwndFrame;
    ctx->sessionId = str::Dup(tab->codexSessionId ? tab->codexSessionId : kCodexPendingSessionId());
    RunAsync(MkFunc0(StartCodexReadThread, ctx), "CodexReadThread");
}

static TempStr FitCodexPanelTitleTemp(HWND labelHwnd, HFONT font, Str docName, int maxDx) {
    TempStr prefix = str::JoinTemp(_TRA("Codex chat"), StrL(" with "));
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
        if (len(title) > 0) {
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
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)win->brControlBgColor;
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
            } else if (wp == 44) {
                // WebView recreated after a theme change: restore the chat
                KillTimer(hwnd, 44);
                OnCodexTabChanged(win);
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
        ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
    }
}

void RelayoutCodexPanel(MainWindow* win) {
    if (!win || !win->hwndCodexBox || !win->uiState.codexVisible) {
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
    webView->dataDir = str::Dup(fmt("%s\\SumatraPDF\\CodexWebView_%d", userProfile, (int)GetCurrentProcessId()));
    if (!LockDataResource(IDR_CLAUDE_MARKED_JS, &gCodexMarkedJs)) {
        delete webView;
        return;
    }
    wstr::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = wstr::Dup(kCodexVirtualHostW);
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

// apply theme colors to the panel's native controls and, since the chat
// colors are baked into the WebView's html, recreate the WebView (the chat
// log is replayed into it once the new page has loaded)
void UpdateCodexTheme(MainWindow* win) {
    if (!win || !win->hwndCodexBox) {
        return;
    }
    COLORREF bgCol = ThemeControlBackgroundColor();
    COLORREF txtCol = ThemeWindowTextColor();
    if (win->codexLabelWithClose) {
        win->codexLabelWithClose->SetColors(txtCol, bgCol);
    }
    if (win->codexInput) {
        win->codexInput->SetColors(txtCol, bgCol);
    }
    // the panel is created after the frame-wide dark mode pass, so its
    // controls (e.g. the checkbox) need their own subclass + theme pass
    if (UseDarkModeLib() && !IsCurrentThemeDefault()) {
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndCodexBox);
    }
    if (win->codexWebView) {
        delete win->codexWebView;
        win->codexWebView = nullptr;
        win->codexWebViewReady = false;
        if (win->uiState.codexVisible) {
            EnsureWebViewReady(win);
            // replay the chat once the new page has had time to load
            SetTimer(win->hwndCodexBox, 44, 600, nullptr);
        }
    }
    RedrawWindow(win->hwndCodexBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
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
    UpdateCodexTheme(win);
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

    if (len(sessions) > 0) {
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

    if (win->uiState.codexVisible) {
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
    ScheduleUiUpdate(win);
}

// call when switching tabs to update session context
void OnCodexTabChanged(MainWindow* win) {
    UpdateCodexPanelTitle(win, 0);
    WindowTab* tab = win->CurrentTab();
    bool supported = IsCodexBuildSupportedForTab(tab);
    UpdateCodexPanelForCurrentTab(win);

    if (!win->uiState.codexVisible) {
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
    if (!tab->codexChatLog.IsEmpty()) {
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
