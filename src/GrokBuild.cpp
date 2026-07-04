/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Thread.h"
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
#include "GrokBuild.h"

bool IsGrokBuildAvailable() {
    return IsAIChatAvailable();
}

static TempStr FindGrokExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.grok\\bin\\grok.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\grok.exe", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"grok.exe"), WStr(L"grok"));
}

bool IsGrokBuildInstalled() {
    return len(FindGrokExecutableTemp()) > 0;
}

TempStr GrokBuildExecutablePathTemp() {
    return FindGrokExecutableTemp();
}

static Mutex gGrokBuildLogMutex;
static AIChatLogger gGrokBuildLogger = {&gGrokBuildLogMutex, "grok-build-log.txt", "grok-build"};

static void GrokBuildLog(Str direction, Str text) {
    AIChatLog(&gGrokBuildLogger, direction, text);
}

static Str kGrokBuildDocURI() {
    return StrL("/AI-Chat-with-document#grok-build");
}

static void ShowGrokBuildNotInstalledDialog() {
    AIChatNotInstalledDialogArgs args;
    args.windowTitle = _TRA("Grok chat");
    args.mainInstruction = _TRA("Grok Build cli must be installed for this functionality");
    args.docUri = kGrokBuildDocURI();
    AIChatShowNotInstalledDialog(args);
}

bool IsGrokBuildSupportedForFile(Str filePath, Kind engineKind) {
    return IsAIChatSupportedForFile(filePath, engineKind);
}

bool IsGrokBuildSupportedForTab(WindowTab* tab) {
    return IsAIChatSupportedForTab(tab);
}

#define IDC_GROK_LABEL_WITH_CLOSE 1120
#define IDC_GROK_ALWAYS_APPROVE 1121
#define IDC_GROK_SESSION_COMBO 1122
#define IDC_GROK_MODEL_COMBO 1123
#define IDC_GROK_EFFORT_COMBO 1124
#define IDC_GROK_STOP_BTN 1125

static Str kGrokVirtualHost() {
    return StrL("https://sumatrapdf.grok/");
}
constexpr const WCHAR* kGrokVirtualHostW = L"https://sumatrapdf.grok/";

static LoadedDataResource gGrokMarkedJs;

static Str GrokBgColor() {
    Str bg = gGlobalPrefs->grokBuild.bgColor;
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

static void BuildGrokModelsList(StrVec& models) {
    models.Reset();
    AIChatAppendModelUnique(models, "grok-composer-2.5-fast");
    AIChatAppendModelUnique(models, "grok-build");
    Str extra = gGlobalPrefs->grokBuild.models;
    if (len(extra) > 0) {
        StrVec parts;
        Split(&parts, extra, ",", true);
        for (int i = 0; i < len(parts); i++) {
            AIChatAppendModelUnique(models, parts.At(i));
        }
    }
}

static Str ResolveGrokModel(const StrVec& models, Str model) {
    int idx = AIChatFindModelInList(models, model);
    if (idx >= 0) {
        return models.At(idx);
    }
    idx = AIChatFindModelInList(models, "grok-composer-2.5-fast");
    if (idx >= 0) {
        return models.At(idx);
    }
    return StrL("grok-composer-2.5-fast");
}

static void PopulateModelCombo(HWND combo) {
    if (!combo) {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    StrVec models;
    BuildGrokModelsList(models);
    for (int i = 0; i < len(models); i++) {
        TempStr display = AIChatModelDisplayNameTemp(models.At(i), "Grok-composer-2.5-fast");
        WCHAR* displayW = CWStrTemp(display);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)displayW);
    }
}

// Apply persisted settings to the UI controls
static void ApplyGrokSettingsToUI(MainWindow* win) {
    int effortIdx = gGlobalPrefs->grokBuild.effort;
    if (effortIdx < 0 || effortIdx > 4) {
        effortIdx = 1;
    }
    if (win->hwndGrokModelCombo) {
        PopulateModelCombo(win->hwndGrokModelCombo);
        StrVec models;
        BuildGrokModelsList(models);
        Str model = ResolveGrokModel(models, gGlobalPrefs->grokBuild.model);
        int modelIdx = AIChatFindModelInList(models, model);
        if (modelIdx < 0) {
            modelIdx = 0;
        }
        SendMessageW(win->hwndGrokModelCombo, CB_SETCURSEL, modelIdx, 0);
    }
    if (win->hwndGrokEffortCombo) {
        SendMessageW(win->hwndGrokEffortCombo, CB_SETCURSEL, effortIdx, 0);
    }
    if (win->hwndGrokAlwaysApproveCheck) {
        SendMessageW(win->hwndGrokAlwaysApproveCheck, BM_SETCHECK,
                     gGlobalPrefs->grokBuild.alwaysApprove ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

// Read current settings from UI controls and save
static void SyncGrokSettingsFromUI(MainWindow* win) {
    if (win->hwndGrokModelCombo) {
        int sel = (int)SendMessageW(win->hwndGrokModelCombo, CB_GETCURSEL, 0, 0);
        StrVec models;
        BuildGrokModelsList(models);
        if (sel >= 0 && sel < len(models)) {
            str::ReplaceWithCopy(&gGlobalPrefs->grokBuild.model, models.At(sel));
        }
    }
    if (win->hwndGrokEffortCombo) {
        gGlobalPrefs->grokBuild.effort = (int)SendMessageW(win->hwndGrokEffortCombo, CB_GETCURSEL, 0, 0);
    }
    if (win->hwndGrokAlwaysApproveCheck) {
        gGlobalPrefs->grokBuild.alwaysApprove =
            (SendMessageW(win->hwndGrokAlwaysApproveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    AIChatUpdateSidebarDx(win, win->aiChatDx, false);
    SaveSettings();
}

// Execute JS on the WebView AND record it in the current tab's chat log
static void LayoutGrokBox(MainWindow* win);
static void AutoSelectRecentSession(MainWindow* win);
static void WebViewAddError(MainWindow* win, Str text); // forward decl
static void WebViewShowUnsupportedFileType(MainWindow* win);

static void UpdateGrokPanelForCurrentTab(MainWindow* win) {
    if (!win || !win->hwndGrokBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool supported = IsGrokBuildSupportedForTab(tab);
    bool working = supported && tab && tab->grokProcess != nullptr;
    bool enableInput = supported && !working;

    if (win->grokInput) {
        EnableWindow(win->grokInput->hwnd, enableInput);
        WStr cue = WStrL(L"Ask about this document...");
        if (!supported) {
            cue = WStrL(L"Not available for this file type");
        } else if (working) {
            cue = WStrL(L"Agent is working...");
        }
        SendMessageW(win->grokInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)cue.s);
    }
    if (win->hwndGrokSessionCombo) {
        EnableWindow(win->hwndGrokSessionCombo, enableInput);
    }
    if (win->hwndGrokModelCombo) {
        EnableWindow(win->hwndGrokModelCombo, enableInput);
    }
    if (win->hwndGrokEffortCombo) {
        EnableWindow(win->hwndGrokEffortCombo, enableInput);
    }
    if (win->hwndGrokAlwaysApproveCheck) {
        EnableWindow(win->hwndGrokAlwaysApproveCheck, enableInput);
    }
    if (win->hwndGrokStopBtn) {
        ShowWindow(win->hwndGrokStopBtn, working ? SW_SHOW : SW_HIDE);
        EnableWindow(win->hwndGrokStopBtn, working);
    }
    LayoutGrokBox(win);
}

static void SetGrokWorking(MainWindow* win, bool /*working*/) {
    UpdateGrokPanelForCurrentTab(win);
}

static void CloseGrokProcess(WindowTab* tab, bool terminateIfRunning) {
    if (!tab) {
        return;
    }
    AIChatCloseProcess(&tab->grokProcess, terminateIfRunning);
}

static void StopGrok(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->grokProcess) {
        GrokBuildLog("stop", tab->grokSessionId ? tab->grokSessionId : StrL("(no session)"));
        CloseGrokProcess(tab, true);
        WebViewAddError(win, "Stopped by user.");
        SetGrokWorking(win, false);
    }
}

static void WebViewEval(MainWindow* win, Str js, bool record = true) {
    if (win->grokWebView && win->grokWebViewReady) {
        win->grokWebView->Eval(js);
    }
    if (record) {
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            tab->grokChatLog.Append(js);
            tab->grokChatLog.AppendChar('\n');
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
    GrokBuildLog("error", text);
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
    Str msg = "Grok Build is only available for PDF and image files.";
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(msg));
    WebViewEval(win, js, false);
}

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, WindowTab* tab) {
    if (tab->grokChatLog.IsEmpty()) {
        return;
    }
    if (!win->grokWebView || !win->grokWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    Str log = ToStr(tab->grokChatLog);
    Str rest = log;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        if (len(line) > 0) {
            win->grokWebView->Eval(str::DupTemp(line));
        }
    }
}

// --- Session history ---

// URL-encode a path the way Grok stores session dirs (e.g. C:\foo -> C%3A%5Cfoo)
static TempStr EncodeGrokDirTemp(Str dir) {
    str::Builder buf;
    for (int i = 0; i < dir.len; i++) {
        unsigned char c = (unsigned char)dir.s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            buf.AppendChar((char)c);
        } else {
            buf.Append(fmt("%%%02X", c));
        }
    }
    return ToStrTemp(buf);
}

static bool IsGrokSessionDirName(Str name) {
    if (!name) {
        return false;
    }
    int n = len(name);
    if (n != 36) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char c = name.s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return false;
            }
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

static TempStr GrokSessionsProjectDirTemp(Str dir) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return {};
    }
    TempStr encodedDir = EncodeGrokDirTemp(dir);
    return fmt("%s\\.grok\\sessions\\%s", userProfile, encodedDir);
}

static TempStr ExtractGrokPromptFromHistoryLineTemp(Str line, Str sessionId) {
    TempStr sid = AIChatJsonStrTemp(line, "session_id");
    if (!sid || !str::Eq(sid, sessionId)) {
        return {};
    }
    return AIChatJsonStrTemp(line, "prompt");
}

static Str GetGrokSessionDescription(Str projectDir, Str sessionId) {
    TempStr historyPath = fmt("%s\\prompt_history.jsonl", projectDir);
    Str data = file::ReadFile(historyPath);
    if (str::IsEmpty(data)) {
        return StrL("(no description)");
    }
    Str content = data;
    Str rest = content;
    Str result;
    Str line;

    while (!result && str::NextLine(rest, line, rest)) {
        if (str::IsEmpty(line)) {
            continue;
        }
        TempStr prompt = ExtractGrokPromptFromHistoryLineTemp(str::DupTemp(line), sessionId);
        if (len(prompt) > 0) {
            result = str::Dup(prompt);
        }
    }
    str::Free(data);
    return result ? result : StrL("(no description)");
}

// Scan ~/.grok/sessions/<url-encoded-dir>/ for session subdirectories
static void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr projectDir = GrokSessionsProjectDirTemp(dir);
    if (!projectDir || !dir::Exists(projectDir)) {
        return;
    }

    TempStr pattern = fmt("%s\\*", projectDir);
    WIN32_FIND_DATAW fd;
    WCHAR* patternW = CWStrTemp(pattern);
    HANDLE hFind = FindFirstFileW(patternW, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr fileName = ToUtf8Temp(fd.cFileName);
        if (str::Eq(fileName, ".") || str::Eq(fileName, "..")) {
            continue;
        }
        if (!IsGrokSessionDirName(fileName)) {
            continue;
        }

        Str desc = GetGrokSessionDescription(projectDir, fileName);
        i64 ts = AIChatFileTimeToMs(fd.ftLastWriteTime);

        AIChatSessionInfo si;
        si.sessionId = str::Dup(fileName);
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
    HWND combo = win->hwndGrokSessionCombo;
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

        if (tab->grokSessionId && str::Eq(tab->grokSessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (tab->grokSessionId && !foundCurrent) {
        Str label = "(current session)";
        WCHAR* labelW = CWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);
        selectedIdx = len(sessions) + 1;
    }

    SendMessageW(combo, CB_SETCURSEL, selectedIdx, 0);
    AIChatFreeSessions(sessions);

    gPopulatingCombo = false;
}

static TempStr StripGrokUserQueryWrapperTemp(Str text) {
    if (!text) {
        return {};
    }
    Str contentStart;
    if (!str::Cut(text, StrL("<user_query>"), nullptr, &contentStart)) {
        return {}; // skip injected context (user_info, rules, skills, etc.)
    }
    Str content;
    if (!str::Cut(contentStart, StrL("</user_query>"), &content, nullptr)) {
        return {};
    }
    TempStr result = str::DupTemp(content);
    str::TrimWSInPlace(result, str::TrimOpt::Both);
    return len(result) > 0 ? result : nullptr;
}

static TempStr ExtractGrokChatUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"user\""))) {
        return {};
    }
    if (str::Contains(line, StrL("\"type\":\"text\",\"text\":\""))) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        return StripGrokUserQueryWrapperTemp(text);
    }
    TempStr content = AIChatJsonStrTemp(line, "content");
    return StripGrokUserQueryWrapperTemp(content);
}

static void AppendGrokHistoryTools(MainWindow* win, Str line) {
    Str searchFrom;
    if (!str::Cut(line, StrL("\"tool_calls\":["), nullptr, &searchFrom)) {
        return;
    }
    while (true) {
        Str rest;
        if (!str::Cut(searchFrom, StrL("\"name\":\""), nullptr, &rest)) {
            break;
        }
        str::Builder nameBuf;
        int j = 0;
        while (j < rest.len && rest.s[j] != '"') {
            if (rest.s[j] == '\\' && j + 1 < rest.len) {
                j++;
                char c = rest.s[j];
                if (c == 'n') {
                    nameBuf.AppendChar('\n');
                } else if (c == 't') {
                    nameBuf.AppendChar('\t');
                } else if (c == '\\') {
                    nameBuf.AppendChar('\\');
                } else if (c == '"') {
                    nameBuf.AppendChar('"');
                } else {
                    nameBuf.AppendChar(c);
                }
            } else {
                nameBuf.AppendChar(rest.s[j]);
            }
            j++;
        }
        if (len(nameBuf) > 0) {
            str::Builder desc;
            desc.Append(fmt("Tool: %s", ToStr(nameBuf)));
            WebViewAddTool(win, ToStr(desc));
        }
        if (j + 1 >= rest.len) {
            break;
        }
        searchFrom = Str(rest.s + j + 1, rest.len - j - 1);
    }
}

// Load conversation history from Grok's chat_history.jsonl
static void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    TempStr projectDir = GrokSessionsProjectDirTemp(dir);
    if (!projectDir) {
        return;
    }
    TempStr sessionPath = fmt("%s\\%s\\chat_history.jsonl", projectDir, sessionId);
    if (!file::Exists(sessionPath)) {
        return;
    }

    Str data = file::ReadFile(sessionPath);
    if (str::IsEmpty(data)) {
        return;
    }

    Str content = data;
    Str rest = content;
    Str lineRaw;

    while (str::NextLine(rest, lineRaw, rest)) {
        if (!str::IsEmpty(lineRaw)) {
            TempStr line = str::DupTemp(lineRaw);
            TempStr userText = ExtractGrokChatUserTextTemp(line);
            if (userText) {
                WebViewAddUser(win, userText);
            } else if (str::Contains(line, StrL("\"type\":\"assistant\""))) {
                TempStr text = AIChatJsonStrTemp(line, "content");
                if (len(text) > 0) {
                    WebViewAppendText(win, text);
                }
                AppendGrokHistoryTools(win, line);
                WebViewFlushBlock(win);
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

    HWND combo = win->hwndGrokSessionCombo;
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }

    if (sel == 0) {
        // "New Session" — clear current session
        GrokBuildLog("session", "new");
        str::ReplaceWithCopy(&tab->grokSessionId, Str{});
        tab->grokChatLog.Reset();
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < len(sessions)) {
        GrokBuildLog("session", sessions[sessionIdx].sessionId);
        str::ReplaceWithCopy(&tab->grokSessionId, sessions[sessionIdx].sessionId);
        tab->grokChatLog.Reset();
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->grokSessionId, dir);
        // LoadSessionHistory calls WebViewEval which rebuilds grokChatLog
    }

    AIChatFreeSessions(sessions);
}

// --- Stream JSON events ---
enum class GrokUpdateType {
    Text,
    Tool,
    Error,
    Flush,
    SessionId,
    Finished,
};

static Str kGrokPendingSessionId() {
    return StrL("pending");
}

struct GrokUpdateData {
    HWND hwndFrame;
    Str text;
    Str sessionId; // to identify which tab this belongs to
    GrokUpdateType updateType;
};

static void OnGrokUpdate(GrokUpdateData* data) {
    MainWindow* win = AIChatFindMainWindowByFrame(data->hwndFrame);
    if (!win || !IsMainWindowValid(win) || !win->hwndGrokBox) {
        str::Free(data->text);
        str::Free(data->sessionId);
        free(data);
        return;
    }
    {
        WindowTab* tab = nullptr;
        for (WindowTab* t : win->Tabs()) {
            if (!t || !t->grokProcess) {
                continue;
            }
            if (data->sessionId && t->grokSessionId && str::Eq(t->grokSessionId, data->sessionId)) {
                tab = t;
                break;
            }
            if (data->sessionId && str::Eq(data->sessionId, kGrokPendingSessionId()) && !t->grokSessionId) {
                tab = t;
                break;
            }
        }
        if (!tab) {
            for (WindowTab* t : win->Tabs()) {
                if (t && t->grokSessionId && data->sessionId && str::Eq(t->grokSessionId, data->sessionId)) {
                    tab = t;
                    break;
                }
            }
        }
        bool isActiveTab = (tab && tab == win->CurrentTab());

        switch (data->updateType) {
            case GrokUpdateType::Text:
                if (data->text) {
                    GrokBuildLog("<<< text", data->text);
                }
                if (isActiveTab) {
                    WebViewAppendText(win, data->text);
                }
                break;
            case GrokUpdateType::Tool:
                if (data->text) {
                    GrokBuildLog("<<< tool", data->text);
                }
                if (isActiveTab) {
                    WebViewAddTool(win, data->text);
                }
                break;
            case GrokUpdateType::Error:
                if (isActiveTab) {
                    WebViewAddError(win, data->text);
                } else if (data->text) {
                    GrokBuildLog("error", data->text);
                }
                break;
            case GrokUpdateType::Flush:
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                }
                break;
            case GrokUpdateType::SessionId:
                if (data->text) {
                    GrokBuildLog("<<< session", data->text);
                }
                if (tab && data->text) {
                    str::ReplaceWithCopy(&tab->grokSessionId, data->text);
                }
                break;
            case GrokUpdateType::Finished:
                if (tab && tab->grokProcess) {
                    if (WaitForSingleObject(tab->grokProcess, 0) == WAIT_OBJECT_0) {
                        DWORD exitCode = 0;
                        GetExitCodeProcess(tab->grokProcess, &exitCode);
                        GrokBuildLog("exit", fmt("%lu", exitCode));
                    }
                    CloseGrokProcess(tab, false);
                }
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                    SetGrokWorking(win, false);
                    PopulateSessionCombo(win);
                }
                break;
        }
    }
    str::Free(data->text);
    str::Free(data->sessionId);
    free(data);
}

static void PostUpdate(HWND hwndFrame, Str sessionId, Str text, GrokUpdateType type) {
    auto data = (GrokUpdateData*)calloc(1, sizeof(GrokUpdateData));
    data->hwndFrame = hwndFrame;
    data->sessionId = sessionId ? str::Dup(sessionId) : Str{};
    data->text = text ? str::Dup(text) : Str{};
    data->updateType = type;
    uitask::Post(MkFunc0(OnGrokUpdate, data));
}

struct GrokReadCtx {
    HANDLE hReadPipe;
    HWND hwndFrame;
    Str sessionId;
};

static void GrokReadThread(GrokReadCtx* ctx) {
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
                    GrokBuildLog("<<<", line);
                }
                TempStr eventType = AIChatJsonStrTemp(line, "type");

                if (eventType && str::Eq(eventType, "thought")) {
                    TempStr thought = AIChatJsonStrTemp(line, "data");
                    if (len(thought) > 0) {
                        GrokBuildLog("<<< thought", thought);
                    }
                } else if (eventType && str::Eq(eventType, "text")) {
                    TempStr text = AIChatJsonStrTemp(line, "data");
                    if (len(text) > 0) {
                        PostUpdate(hwndFrame, sessionId, text, GrokUpdateType::Text);
                    }
                } else if (eventType && str::Eq(eventType, "error")) {
                    TempStr err = AIChatJsonStrTemp(line, "data");
                    if (!err) {
                        err = AIChatJsonStrTemp(line, "message");
                    }
                    if (err) {
                        PostUpdate(hwndFrame, sessionId, err, GrokUpdateType::Error);
                    }
                } else if (eventType && str::Eq(eventType, "end")) {
                    GrokBuildLog("<<< end", line);
                    TempStr newSessionId = AIChatJsonStrTemp(line, "sessionId");
                    if (newSessionId) {
                        PostUpdate(hwndFrame, sessionId, newSessionId, GrokUpdateType::SessionId);
                        str::ReplaceWithCopy(&sessionId, newSessionId);
                    }
                    PostUpdate(hwndFrame, sessionId, {}, GrokUpdateType::Flush);
                    PostUpdate(hwndFrame, sessionId, {}, GrokUpdateType::Finished);
                }

                lineBuf.Reset();
            } else if (buf[i] != '\r') {
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    Str rem = ToStr(lineBuf);
    if (rem) {
        GrokBuildLog("<<<", rem);
    }
    GrokBuildLog("eof", "(stdout closed)");

    CloseHandle(hPipe);
    PostUpdate(hwndFrame, sessionId, {}, GrokUpdateType::Finished);
    str::Free(sessionId);
}

static void StartGrokReadThread(GrokReadCtx* ctx) {
    GrokReadThread(ctx);
}

static void SendGrokMessage(MainWindow* win) {
    if (!win->grokInput) {
        return;
    }
    if (!IsGrokBuildSupportedForTab(win->CurrentTab())) {
        return;
    }
    HWND hwndInput = win->grokInput->hwnd;
    int inputLen = GetWindowTextLengthW(hwndInput);
    if (inputLen == 0) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }
    if (tab->grokProcess) {
        return; // this tab already has a running request
    }

    TempWStr inputW = HwndGetTextWTemp(hwndInput);
    TempStr input = ToUtf8Temp(inputW);
    HwndSetText(hwndInput, "");

    WebViewAddUser(win, input);
    SetGrokWorking(win, true);

    bool isNewSession = len(tab->grokSessionId) == 0;

    Str filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);

    TempStr escapedInput = str::ReplaceTemp(input, StrL("\""), StrL("\\\""));

    SyncGrokSettingsFromUI(win);

    Str efforts[] = {StrL("low"), StrL("medium"), StrL("high"), StrL("xhigh"), StrL("max")};
    StrVec modelList;
    BuildGrokModelsList(modelList);
    Str model = ResolveGrokModel(modelList, gGlobalPrefs->grokBuild.model);
    int effortIdx = gGlobalPrefs->grokBuild.effort;
    if (effortIdx < 0 || effortIdx > 4) {
        effortIdx = 1;
    }
    Str permsFlag = gGlobalPrefs->grokBuild.alwaysApprove ? StrL("--always-approve") : Str{};
    TempStr rules = fmt("The user is currently reading the file: %s", filePath);

    TempStr grokPath = FindGrokExecutableTemp();
    if (!grokPath) {
        GrokBuildLog("error", "Cannot find grok executable");
        WebViewAddError(win, "Cannot find grok. Is Grok Build installed?");
        SetGrokWorking(win, false);
        return;
    }

    GrokBuildLog(">>> user", input);
    GrokBuildLog(">>> session", fmt("%s (%s)", tab->grokSessionId ? tab->grokSessionId : kGrokPendingSessionId(),
                                    Str(isNewSession ? "new" : "resume")));
    GrokBuildLog(">>> cwd", dir);

    TempStr cmdLine;
    if (isNewSession) {
        cmdLine =
            fmt("\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort %s %s --rules \"%s\"",
                grokPath, escapedInput, dir, model, efforts[effortIdx], permsFlag, rules);
    } else {
        cmdLine =
            fmt("\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort %s %s -r %s --rules "
                "\"%s\"",
                grokPath, escapedInput, dir, model, efforts[effortIdx], permsFlag, tab->grokSessionId, rules);
    }

    GrokBuildLog(">>> cmd", cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, dir, &launch)) {
        GrokBuildLog("error", "Failed to launch grok process");
        WebViewAddError(win, "Failed to launch grok. Is it installed and in PATH?");
        SetGrokWorking(win, false);
        return;
    }

    tab->grokProcess = launch.hProcess;
    GrokBuildLog(">>> start", fmt("pid %lu", launch.processId));

    auto ctx = (GrokReadCtx*)calloc(1, sizeof(GrokReadCtx));
    ctx->hReadPipe = launch.hReadPipe;
    ctx->hwndFrame = win->hwndFrame;
    ctx->sessionId = str::Dup(tab->grokSessionId ? tab->grokSessionId : kGrokPendingSessionId());
    RunAsync(MkFunc0(StartGrokReadThread, ctx), "GrokReadThread");
}

static TempStr FitGrokPanelTitleTemp(HWND labelHwnd, HFONT font, Str docName, int maxDx) {
    TempStr prefix = str::JoinTemp(_TRA("Grok chat"), StrL(" with "));
    return AIChatFitPanelTitleTemp(labelHwnd, font, prefix, docName, maxDx);
}

static void UpdateGrokPanelTitle(MainWindow* win, int labelDx) {
    if (!win || !win->grokLabelWithClose) {
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

    HWND labelHwnd = win->grokLabelWithClose->hwnd;
    HFONT font = win->grokLabelWithClose->font;
    if (!font) {
        font = GetDefaultGuiFont(true, false);
    }
    if (labelDx <= 0 && win->hwndGrokBox) {
        labelDx = ClientRect(win->hwndGrokBox).dx;
    }
    int maxDx = AIChatLabelMaxTextDx(labelHwnd, labelDx);
    TempStr label = FitGrokPanelTitleTemp(labelHwnd, font, docName, maxDx);
    win->grokLabelWithClose->SetLabel(label);
}

// --- Layout ---
static void LayoutGrokBox(MainWindow* win) {
    HWND hwndContainer = win->hwndGrokBox;
    Rect rc = ClientRect(hwndContainer);
    int y = 0;

    UpdateGrokPanelTitle(win, rc.dx);

    // label
    Size labelSize = win->grokLabelWithClose->GetIdealSize();
    MoveWindow(win->grokLabelWithClose->hwnd, 0, y, rc.dx, labelSize.dy, TRUE);
    y += labelSize.dy;

    // session combo — get actual height from font metrics
    int comboDy = 0;
    if (win->hwndGrokSessionCombo) {
        // the visible edit part of a dropdown is determined by the font
        // GetComboBoxInfo or just use SendMessage CB_GETITEMHEIGHT
        int itemH = (int)SendMessageW(win->hwndGrokSessionCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        comboDy = itemH + 8; // item height + borders
        // MoveWindow height for CBS_DROPDOWNLIST = visible height + dropdown list height
        MoveWindow(win->hwndGrokSessionCombo, 2, y + 1, rc.dx - 4, comboDy + 200, TRUE);
    }
    y += comboDy + 3;

    // bottom: input, then [Model▾][Effort▾][☐Skip]
    Size inputSize = win->grokInput->GetIdealSize();
    int inputDy = inputSize.dy + 4;
    int optRowDy = 32;
    if (win->hwndGrokModelCombo) {
        int itemH = (int)SendMessageW(win->hwndGrokModelCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        optRowDy = itemH + 8;
    }
    int bottomDy = inputDy + 4 + optRowDy;

    int webViewDy = rc.dy - y - bottomDy;
    if (webViewDy < 0) {
        webViewDy = 0;
    }

    if (win->grokWebView) {
        MoveWindow(win->grokWebView->hwnd, 0, y, rc.dx, webViewDy, TRUE);
        // defer UpdateWebviewSize during rapid WM_SIZE to avoid WebView2 put_Bounds freeze
        KillTimer(win->hwndGrokBox, 43);
        SetTimer(win->hwndGrokBox, 43, 50, nullptr);
    }
    y += webViewDy;

    // input row: [input box] [Stop] — stop button only visible when working
    int stopBtnDx = 50;
    WindowTab* curTab = win->CurrentTab();
    bool isWorking = (curTab && curTab->grokProcess != nullptr);
    if (isWorking && win->hwndGrokStopBtn) {
        MoveWindow(win->grokInput->hwnd, 0, y, rc.dx - stopBtnDx - 2, inputDy, TRUE);
        MoveWindow(win->hwndGrokStopBtn, rc.dx - stopBtnDx, y, stopBtnDx, inputDy, TRUE);
    } else {
        MoveWindow(win->grokInput->hwnd, 0, y, rc.dx, inputDy, TRUE);
    }
    y += inputDy + 4;

    // options row: [Model▾] [Effort▾] [☐Skip]
    {
        int x = 2;
        int thirdDx = (rc.dx - 8) / 3;
        if (win->hwndGrokModelCombo) {
            MoveWindow(win->hwndGrokModelCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndGrokEffortCombo) {
            MoveWindow(win->hwndGrokEffortCombo, x, y, thirdDx, optRowDy + 200, TRUE);
            x += thirdDx + 2;
        }
        if (win->hwndGrokAlwaysApproveCheck) {
            MoveWindow(win->hwndGrokAlwaysApproveCheck, x + 8, y, rc.dx - x - 10, optRowDy, TRUE);
        }
    }
}

// --- WndProc ---
static LRESULT CALLBACK WndProcGrokInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                         DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !IsShiftPressed()) {
        SendGrokMessage(win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WndProcGrokBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
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
            LayoutGrokBox(win);
            break;
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_GROK_LABEL_WITH_CLOSE) {
                ToggleGrokPanel(win);
            }
            if (LOWORD(wp) == IDC_GROK_STOP_BTN) {
                StopGrok(win);
            }
            if (LOWORD(wp) == IDC_GROK_SESSION_COMBO && HIWORD(wp) == CBN_SELCHANGE) {
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
                if (win->grokWebView) {
                    win->grokWebView->UpdateWebviewSize();
                }
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// --- Splitter ---
constexpr int kGrokMinDx = 150;

static void OnGrokSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = ClientRect(win->hwndFrame);
    int dx = rFrame.dx - pcur.x;
    if (dx < kGrokMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    AIChatUpdateSidebarDx(win, dx, ev->finishedDragging);
    if (ev->finishedDragging) {
        RelayoutForGrokSplitter(win);
    }
}

void RelayoutGrokPanel(MainWindow* win) {
    if (!win || !win->hwndGrokBox || !win->grokVisible) {
        return;
    }
    LayoutGrokBox(win);
    KillTimer(win->hwndGrokBox, 43);
    if (win->grokWebView && win->grokWebViewReady) {
        win->grokWebView->UpdateWebviewSize();
    }
    RedrawWindow(win->hwndGrokBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (win->grokSplitter && win->grokSplitter->hwnd) {
        InvalidateRect(win->grokSplitter->hwnd, nullptr, TRUE);
    }
}

// --- Lazy WebView2 init ---
static void EnsureWebViewReady(MainWindow* win) {
    if (win->grokWebViewReady) {
        return;
    }
    if (!HasWebView()) {
        return;
    }
    auto webView = new WebviewWnd();
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA);
    // use unique data dir per process to avoid locking conflicts
    webView->dataDir = str::Dup(fmt("%s\\SumatraPDF\\GrokWebView_%d", userProfile, (int)GetCurrentProcessId()));
    if (!LockDataResource(IDR_CLAUDE_MARKED_JS, &gGrokMarkedJs)) {
        delete webView;
        return;
    }
    wstr::Free(webView->resourceUriPrefix);
    webView->resourceUriPrefix = wstr::Dup(kGrokVirtualHostW);
    webView->resourceProvider.ctx = &gGrokMarkedJs;
    webView->resourceProvider.getResource = AIChatGetMarkedJsResource;

    Rect rc = ClientRect(win->hwndGrokBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndGrokBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr chatHtml = AIChatFormatChatHtmlTemp(kGrokVirtualHost(), GrokBgColor());
        webView->SetHtml(chatHtml);
        win->grokWebView = webView;
        win->grokWebViewReady = true;
        RelayoutGrokPanel(win);
    } else {
        delete webView;
    }
}

// --- Public API ---
void CreateGrokPanel(MainWindow* win) {
    if (!IsGrokBuildAvailable()) {
        return;
    }
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndGrokBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    // splitter (non-live: only resize on mouse release)
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->grokSplitter = new Splitter();
        win->grokSplitter->onMove = MkFunc1Void(OnGrokSplitterMove);
        win->grokSplitter->Create(args);
    }

    // label
    auto label = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndGrokBox;
        args.cmdId = IDC_GROK_LABEL_WITH_CLOSE;
        args.isRtl = IsUIRtl();
        args.font = GetDefaultGuiFont(true, false);
        label->Create(args);
    }
    win->grokLabelWithClose = label;
    label->SetPaddingXY(2, 2);
    UpdateGrokPanelTitle(win, 0);

    // session combo
    win->hwndGrokSessionCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, dx, 200,
                        win->hwndGrokBox, (HMENU)(UINT_PTR)IDC_GROK_SESSION_COMBO, hmod, nullptr);
    SendMessageW(win->hwndGrokSessionCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // webview deferred
    win->grokWebView = nullptr;
    win->grokWebViewReady = false;

    // model combo
    win->hwndGrokModelCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200, win->hwndGrokBox,
                        (HMENU)(UINT_PTR)IDC_GROK_MODEL_COMBO, hmod, nullptr);
    SendMessageW(win->hwndGrokModelCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // effort combo
    win->hwndGrokEffortCombo =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 100, 200, win->hwndGrokBox,
                        (HMENU)(UINT_PTR)IDC_GROK_EFFORT_COMBO, hmod, nullptr);
    SendMessageW(win->hwndGrokEffortCombo, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    SendMessageW(win->hwndGrokEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Low");
    SendMessageW(win->hwndGrokEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Medium");
    SendMessageW(win->hwndGrokEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"High");
    SendMessageW(win->hwndGrokEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"XHigh");
    SendMessageW(win->hwndGrokEffortCombo, CB_ADDSTRING, 0, (LPARAM)L"Max");
    SendMessageW(win->hwndGrokEffortCombo, CB_SETCURSEL, 1, 0); // default: Medium

    // skip-permissions checkbox
    win->hwndGrokAlwaysApproveCheck =
        CreateWindowExW(0, L"BUTTON", L"Always Approve", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 160, 20,
                        win->hwndGrokBox, (HMENU)(UINT_PTR)IDC_GROK_ALWAYS_APPROVE, hmod, nullptr);
    SendMessageW(win->hwndGrokAlwaysApproveCheck, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);

    // stop button (hidden by default, shown when agent is working)
    win->hwndGrokStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | BS_PUSHBUTTON, 0, 0, 50, 24,
                                           win->hwndGrokBox, (HMENU)(UINT_PTR)IDC_GROK_STOP_BTN, hmod, nullptr);
    SendMessageW(win->hwndGrokStopBtn, WM_SETFONT, (WPARAM)GetDefaultGuiFont(), TRUE);
    ShowWindow(win->hwndGrokStopBtn, SW_HIDE);

    // input box
    auto input = new Edit();
    {
        Edit::CreateArgs args;
        args.parent = win->hwndGrokBox;
        args.isMultiLine = true;
        args.idealSizeLines = 3;
        args.withBorder = true;
        args.cueText = "Ask about this document...";
        input->Create(args);
    }
    win->grokInput = input;

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(input->hwnd, WndProcGrokInput, inputSubclassId, (DWORD_PTR)win);

    win->grokBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndGrokBox, WndProcGrokBox, win->grokBoxSubclassId, (DWORD_PTR)win);

    ApplyGrokSettingsToUI(win);
    AIChatApplySavedSidebarDx(win);
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || tab->grokSessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<AIChatSessionInfo> sessions;
    CollectSessions(dir, sessions);

    if (len(sessions) > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        str::ReplaceWithCopy(&tab->grokSessionId, sessions[0].sessionId);

        // load its history
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->grokSessionId, dir);
    }

    AIChatFreeSessions(sessions);
}

void OnAIChatWithGrokBuild(MainWindow* win) {
    if (!IsGrokBuildAvailable()) {
        return;
    }
    if (!IsGrokBuildInstalled()) {
        ShowGrokBuildNotInstalledDialog();
        return;
    }
    ToggleGrokPanel(win);
}

void ToggleGrokPanel(MainWindow* win) {
    if (!IsGrokBuildAvailable() || !win->hwndGrokBox) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    if (AIChatGetTabPanelOpen(tab) == AIChatBackend::Grok) {
        AIChatSetTabPanelOpen(tab, AIChatBackend::None);
    } else {
        if (!IsGrokBuildSupportedForTab(tab)) {
            return;
        }
        AIChatSetTabPanelOpen(tab, AIChatBackend::Grok);
    }
    AIChatSyncPanelsToCurrentTab(win);

    if (win->grokVisible) {
        UpdateGrokPanelTitle(win, 0);
        EnsureWebViewReady(win);
        UpdateGrokPanelForCurrentTab(win);
        PopulateSessionCombo(win);
        if (win->grokInput) {
            HwndSetFocus(win->grokInput->hwnd);
        }
        // defer auto-select so SetHtml has time to load the page
        SetTimer(win->hwndGrokBox, 42, 500, nullptr);
    }
    RelayoutWindow(win);
}

// call when switching tabs to update session context
void OnGrokTabChanged(MainWindow* win) {
    UpdateGrokPanelTitle(win, 0);
    WindowTab* tab = win->CurrentTab();
    bool supported = IsGrokBuildSupportedForTab(tab);
    UpdateGrokPanelForCurrentTab(win);

    if (!win->grokVisible) {
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
    SetGrokWorking(win, tab->grokProcess != nullptr);

    // if tab has in-memory chat log, replay it (fast, includes current session)
    if (!tab->grokChatLog.IsEmpty()) {
        ReplayChatLog(win, tab);
    } else if (tab->filePath && tab->grokSessionId) {
        // fallback: load from disk
        TempStr dir = path::GetDirTemp(tab->filePath);
        LoadSessionHistory(win, tab->grokSessionId, dir);
    }
}

static bool GrokTabHasRunningProcess(WindowTab* tab) {
    return tab && tab->grokProcess;
}

void ShutdownGrokForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        CloseGrokProcess(tab, true);
    }
    AIChatWaitForTabProcessesToFinish(win, GrokTabHasRunningProcess);
}

void DestroyGrokPanel(MainWindow* win) {
    win->grokWebViewReady = false;

    if (win->hwndGrokBox) {
        KillTimer(win->hwndGrokBox, 42);
        KillTimer(win->hwndGrokBox, 43);
        if (win->grokBoxSubclassId) {
            RemoveWindowSubclass(win->hwndGrokBox, WndProcGrokBox, win->grokBoxSubclassId);
            win->grokBoxSubclassId = 0;
        }
    }

    // save webview dataDir before deleting so we can clean up
    Str webViewDataDir;
    WebviewWnd* webView = win->grokWebView;
    win->grokWebView = nullptr;
    if (webView) {
        webViewDataDir = str::Dup(webView->dataDir);
    }

    delete win->grokLabelWithClose;
    win->grokLabelWithClose = nullptr;
    delete webView;
    delete win->grokInput;
    win->grokInput = nullptr;
    delete win->grokSplitter;
    win->grokSplitter = nullptr;

    if (win->hwndGrokBox) {
        DestroyWindow(win->hwndGrokBox);
        win->hwndGrokBox = nullptr;
    }
    win->hwndGrokSessionCombo = nullptr;
    win->hwndGrokModelCombo = nullptr;
    win->hwndGrokEffortCombo = nullptr;
    win->hwndGrokAlwaysApproveCheck = nullptr;
    win->hwndGrokStopBtn = nullptr;

    // clean up per-process WebView2 cache dir
    if (webViewDataDir) {
        dir::RemoveAll(webViewDataDir);
        str::Free(webViewDataDir);
    }
}
