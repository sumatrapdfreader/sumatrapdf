/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
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
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "resource.h"

#include "AppTools.h"
#include "ClaudeCode.h"

#define IDC_CLAUDE_LABEL_WITH_CLOSE 1110
#define IDC_CLAUDE_SKIP_PERMS 1111
#define IDC_CLAUDE_SESSION_COMBO 1112
#define IDC_CLAUDE_MODEL_COMBO 1113
#define IDC_CLAUDE_EFFORT_COMBO 1114
#define IDC_CLAUDE_STOP_BTN 1115

// --- Persistent Claude settings ---
struct ClaudeSettings {
    int modelIdx = 0;  // 0=Sonnet, 1=Opus, 2=Haiku
    int effortIdx = 1; // 0=Low, 1=Medium, 2=High, 3=Max
    bool skipPerms = false;
    char bgColor[16] = "#ffffff"; // chat background color (CSS hex)
    int claudeDx = 0;             // sidebar width (0 = use default)
};

static ClaudeSettings gClaudeSettings;

static TempStr GetClaudeSettingsPathTemp() {
    return GetPathInAppDataDirTemp("CCSumatraPDF-claude.txt");
}

static void LoadClaudeSettings() {
    TempStr path = GetClaudeSettingsPathTemp();
    if (!path || !file::Exists(path)) {
        return;
    }
    ByteSlice data = file::ReadFile(path);
    if (data.empty()) {
        return;
    }
    const char* s = (const char*)data.data();
    const char* end = s + data.size();
    while (s < end) {
        const char* lineEnd = s;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') {
            lineEnd++;
        }
        if (lineEnd > s) {
            TempStr line = str::DupTemp(s, (int)(lineEnd - s));
            if (str::StartsWith(line, "model=")) {
                gClaudeSettings.modelIdx = atoi(line + 6);
            } else if (str::StartsWith(line, "effort=")) {
                gClaudeSettings.effortIdx = atoi(line + 7);
            } else if (str::StartsWith(line, "skipPerms=")) {
                gClaudeSettings.skipPerms = atoi(line + 10) != 0;
            } else if (str::StartsWith(line, "bgColor=")) {
                const char* val = line + 8;
                if (str::Len(val) > 0 && str::Len(val) < 16) {
                    str::BufSet(gClaudeSettings.bgColor, dimof(gClaudeSettings.bgColor), val);
                }
            } else if (str::StartsWith(line, "claudeDx=")) {
                gClaudeSettings.claudeDx = atoi(line + 9);
            }
        }
        s = lineEnd;
        while (s < end && (*s == '\n' || *s == '\r')) {
            s++;
        }
    }
    data.Free();
}

static void SaveClaudeSettings() {
    TempStr path = GetClaudeSettingsPathTemp();
    if (!path) {
        return;
    }
    TempStr content =
        str::FormatTemp("model=%d\neffort=%d\nskipPerms=%d\nbgColor=%s\nclaudeDx=%d\n", gClaudeSettings.modelIdx,
                        gClaudeSettings.effortIdx, gClaudeSettings.skipPerms ? 1 : 0, gClaudeSettings.bgColor,
                        gClaudeSettings.claudeDx);
    ByteSlice d = {(u8*)content, str::Len(content)};
    file::WriteFile(path, d);
}

// Apply saved settings to the UI controls
static void ApplyClaudeSettingsToUI(MainWindow* win) {
    if (win->hwndClaudeModelCombo) {
        SendMessageW(win->hwndClaudeModelCombo, CB_SETCURSEL, gClaudeSettings.modelIdx, 0);
    }
    if (win->hwndClaudeEffortCombo) {
        SendMessageW(win->hwndClaudeEffortCombo, CB_SETCURSEL, gClaudeSettings.effortIdx, 0);
    }
    if (win->hwndClaudeSkipPermsCheck) {
        SendMessageW(win->hwndClaudeSkipPermsCheck, BM_SETCHECK,
                     gClaudeSettings.skipPerms ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

// Read current settings from UI controls and save
static void SyncClaudeSettingsFromUI(MainWindow* win) {
    if (win->hwndClaudeModelCombo) {
        gClaudeSettings.modelIdx = (int)SendMessageW(win->hwndClaudeModelCombo, CB_GETCURSEL, 0, 0);
    }
    if (win->hwndClaudeEffortCombo) {
        gClaudeSettings.effortIdx = (int)SendMessageW(win->hwndClaudeEffortCombo, CB_GETCURSEL, 0, 0);
    }
    if (win->hwndClaudeSkipPermsCheck) {
        gClaudeSettings.skipPerms = (SendMessageW(win->hwndClaudeSkipPermsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (win->claudeDx > 0) {
        gClaudeSettings.claudeDx = win->claudeDx;
    }
    SaveClaudeSettings();
}

// Claude sidebar background color is set via CSS in kClaudeChatHtml

static char* GenerateSessionId() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return str::Format("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3,
                       guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
                       guid.Data4[6], guid.Data4[7]);
}

// clang-format off
static const char* kClaudeChatHtmlFmt =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<script src='https://cdn.jsdelivr.net/npm/marked/marked.min.js'></script>"
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; }"
    "body { font-family: 'Segoe UI', sans-serif; font-size: 13px; margin: 0; padding: 6px; "
    "  background: %s; color: #222; line-height: 1.4; }"
    "p { margin: 2px 0; }"
    "h1,h2,h3,h4 { margin: 6px 0 2px 0; }"
    "ul,ol { margin: 2px 0 2px 18px; }"
    "li { margin: 1px 0; }"
    ".user { color: #1a5276; font-weight: bold; margin: 8px 0 2px 0; padding: 4px 0; "
    "  border-top: 1px solid #ccc; }"
    ".tool { color: #555; font-size: 11px; font-style: italic; "
    "  border-left: 3px solid #999; padding-left: 6px; margin: 2px 0; }"
    ".assistant { margin: 2px 0; }"
    ".assistant pre { background: #f0f0f0; padding: 6px; border-radius: 4px; "
    "  overflow-x: auto; margin: 3px 0; font-size: 12px; }"
    ".assistant code { background: #e8e8e8; padding: 1px 3px; border-radius: 2px; font-size: 12px; }"
    ".assistant pre code { background: none; padding: 0; }"
    ".error { color: #c0392b; font-weight: bold; margin: 4px 0; }"
    "</style></head><body><div id='chat'></div>"
    "<script>"
    "var chatDiv = document.getElementById('chat');"
    "var currentBlock = null;"
    "var currentRaw = '';"
    "function addUser(text) {"
    "  flushBlock();"
    "  var d = document.createElement('div');"
    "  d.className = 'user';"
    "  d.textContent = 'You: ' + text;"
    "  chatDiv.appendChild(d);"
    "  scrollToBottom();"
    "}"
    "function addTool(text) {"
    "  flushBlock();"
    "  var d = document.createElement('div');"
    "  d.className = 'tool';"
    "  d.textContent = text;"
    "  chatDiv.appendChild(d);"
    "  scrollToBottom();"
    "}"
    "function addError(text) {"
    "  flushBlock();"
    "  var d = document.createElement('div');"
    "  d.className = 'error';"
    "  d.textContent = text;"
    "  chatDiv.appendChild(d);"
    "  scrollToBottom();"
    "}"
    "function appendText(text) {"
    "  if (!currentBlock) {"
    "    currentBlock = document.createElement('div');"
    "    currentBlock.className = 'assistant';"
    "    chatDiv.appendChild(currentBlock);"
    "    currentRaw = '';"
    "  }"
    "  currentRaw += text;"
    "  if (typeof marked !== 'undefined') {"
    "    currentBlock.innerHTML = marked.parse(currentRaw);"
    "  } else {"
    "    currentBlock.textContent = currentRaw;"
    "  }"
    "  scrollToBottom();"
    "}"
    "function flushBlock() {"
    "  currentBlock = null; currentRaw = '';"
    "}"
    "function clearChat() {"
    "  chatDiv.innerHTML = '';"
    "  flushBlock();"
    "}"
    "function scrollToBottom() {"
    "  window.scrollTo(0, document.body.scrollHeight);"
    "}"
    "</script></body></html>";
// clang-format on

static TempStr JsEscapeTemp(const char* s) {
    if (!s) {
        return str::DupTemp("");
    }
    StrBuilder buf;
    while (*s) {
        switch (*s) {
            case '\\':
                buf.Append("\\\\");
                break;
            case '\'':
                buf.Append("\\'");
                break;
            case '\n':
                buf.Append("\\n");
                break;
            case '\r':
                buf.Append("\\r");
                break;
            case '\t':
                buf.Append("\\t");
                break;
            default:
                buf.AppendChar(*s);
                break;
        }
        s++;
    }
    return str::DupTemp(buf.LendData());
}

// Execute JS on the WebView AND record it in the current tab's chat log
static void LayoutClaudeBox(MainWindow* win);
static void AutoSelectRecentSession(MainWindow* win);
static void WebViewAddError(MainWindow* win, const char* text); // forward decl

static void SetClaudeWorking(MainWindow* win, bool working) {
    if (win->claudeInput) {
        EnableWindow(win->claudeInput->hwnd, !working);
        if (working) {
            SendMessageW(win->claudeInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)L"Agent is working...");
        } else {
            SendMessageW(win->claudeInput->hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)L"Ask about this PDF...");
        }
    }
    if (win->hwndClaudeStopBtn) {
        ShowWindow(win->hwndClaudeStopBtn, working ? SW_SHOW : SW_HIDE);
    }
    // relayout so stop button appears/disappears next to input
    LayoutClaudeBox(win);
}

static void StopClaude(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->claudeProcess) {
        TerminateProcess(tab->claudeProcess, 0);
        CloseHandle(tab->claudeProcess);
        tab->claudeProcess = nullptr;
        WebViewAddError(win, "Stopped by user.");
        SetClaudeWorking(win, false);
    }
}

static void WebViewEval(MainWindow* win, const char* js, bool record = true) {
    if (win->claudeWebView && win->claudeWebViewReady) {
        win->claudeWebView->Eval(js);
    }
    if (record) {
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            if (!tab->claudeChatLog) {
                tab->claudeChatLog = new StrBuilder();
            }
            tab->claudeChatLog->Append(js);
            tab->claudeChatLog->AppendChar('\n');
        }
    }
}

static void WebViewAppendText(MainWindow* win, const char* text) {
    TempStr js = str::FormatTemp("appendText('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddUser(MainWindow* win, const char* text) {
    TempStr js = str::FormatTemp("addUser('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddTool(MainWindow* win, const char* text) {
    TempStr js = str::FormatTemp("addTool('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewAddError(MainWindow* win, const char* text) {
    TempStr js = str::FormatTemp("addError('%s')", JsEscapeTemp(text));
    WebViewEval(win, js);
}

static void WebViewFlushBlock(MainWindow* win) {
    WebViewEval(win, "flushBlock()");
}

static void WebViewClearChat(MainWindow* win) {
    WebViewEval(win, "clearChat()", false); // don't record clear
}

// Replay a tab's chat log into the WebView
static void ReplayChatLog(MainWindow* win, WindowTab* tab) {
    if (!tab->claudeChatLog || tab->claudeChatLog->IsEmpty()) {
        return;
    }
    if (!win->claudeWebView || !win->claudeWebViewReady) {
        return;
    }
    // the log is newline-separated JS commands
    const char* s = tab->claudeChatLog->LendData();
    const char* end = s + tab->claudeChatLog->Size();
    while (s < end) {
        const char* lineEnd = s;
        while (lineEnd < end && *lineEnd != '\n') {
            lineEnd++;
        }
        if (lineEnd > s) {
            TempStr line = str::DupTemp(s, (int)(lineEnd - s));
            win->claudeWebView->Eval(line);
        }
        s = lineEnd + 1;
    }
}

static MainWindow* FindMainWindowByFrame(HWND hwndFrame) {
    for (MainWindow* w : gWindows) {
        if (w->hwndFrame == hwndFrame) {
            return w;
        }
    }
    return nullptr;
}

// --- JSON helpers ---
static TempStr JsonStrTemp(const char* json, const char* key) {
    TempStr pattern = str::FormatTemp("\"%s\":\"", key);
    const char* start = str::Find(json, pattern);
    if (!start) {
        return nullptr;
    }
    start += str::Len(pattern);
    StrBuilder buf;
    while (*start && *start != '"') {
        if (*start == '\\' && *(start + 1)) {
            start++;
            if (*start == 'n') {
                buf.AppendChar('\n');
            } else if (*start == 't') {
                buf.AppendChar('\t');
            } else if (*start == '\\') {
                buf.AppendChar('\\');
            } else if (*start == '"') {
                buf.AppendChar('"');
            } else {
                buf.AppendChar(*start);
            }
        } else {
            buf.AppendChar(*start);
        }
        start++;
    }
    return str::DupTemp(buf.LendData());
}

// --- Session history ---
struct SessionInfo {
    char* sessionId;
    char* display; // first user message
    char* project; // directory
    i64 timestamp;
};

// Compute the encoded project dir path that Claude uses:
// E:\foo_bar -> E--foo-bar (: removed, \ -> -, _ -> -)
static TempStr EncodeClaudeDirTemp(const char* dir) {
    StrBuilder buf;
    while (*dir) {
        char c = *dir;
        if (c == ':' || c == '\\' || c == '/' || c == '_' || c == ' ') {
            buf.AppendChar('-');
        } else {
            buf.AppendChar(c);
        }
        dir++;
    }
    // remove trailing dash if present
    if (buf.Size() > 0 && buf.LendData()[buf.Size() - 1] == '-') {
        buf.RemoveLast();
    }
    return str::DupTemp(buf.LendData());
}

// Extract user message text from a JSON line.
// Handles both "content":"string" and "content":[{"type":"text","text":"..."}] formats.
static TempStr ExtractUserTextTemp(const char* line) {
    if (!str::Find(line, "\"role\":\"user\"")) {
        return nullptr;
    }
    // skip tool_result messages (they have role:user but contain tool output, not user text)
    if (str::Find(line, "\"tool_result\"")) {
        return nullptr;
    }
    // try string format: "content":"text"
    if (str::Find(line, "\"content\":\"")) {
        TempStr content = JsonStrTemp(line, "content");
        if (content && str::Len(content) > 0 && !str::Find(content, "<command-")) {
            return content;
        }
    }
    // try array format: "content":[{"type":"text","text":"..."}]
    if (str::Find(line, "\"content\":[")) {
        // find the text field inside the first text block
        const char* textBlock = str::Find(line, "\"type\":\"text\",\"text\":\"");
        if (!textBlock) {
            textBlock = str::Find(line, "\"text\":\"");
        }
        if (textBlock) {
            TempStr text = JsonStrTemp(textBlock - 1, "text");
            if (text && str::Len(text) > 0 && !str::Find(text, "<command-")) {
                return text;
            }
        }
    }
    return nullptr;
}

// Read the first user message from a session JSONL as description
static char* GetSessionDescription(const char* sessionPath) {
    ByteSlice data = file::ReadFile(sessionPath);
    if (data.empty()) {
        return str::Dup("(empty)");
    }
    const char* s = (const char*)data.data();
    const char* end = s + data.size();
    char* result = nullptr;

    while (s < end && !result) {
        const char* lineEnd = s;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') {
            lineEnd++;
        }
        if (lineEnd > s) {
            int lineLen = (int)(lineEnd - s);
            TempStr line = str::DupTemp(s, lineLen);
            TempStr userText = ExtractUserTextTemp(line);
            if (userText) {
                result = str::Dup(userText);
            }
        }
        s = lineEnd;
        while (s < end && (*s == '\n' || *s == '\r')) {
            s++;
        }
    }
    data.Free();
    return result ? result : str::Dup("(no description)");
}

// Scan ~/.claude/projects/<encoded-dir>/ for .jsonl session files
static void CollectSessions(const char* dir, Vec<SessionInfo>& sessions) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr projectDir = str::FormatTemp("%s\\.claude\\projects\\%s", userProfile, encodedDir);

    // scan for *.jsonl files in the project directory
    TempStr pattern = str::FormatTemp("%s\\*.jsonl", projectDir);
    WIN32_FIND_DATAW fd;
    WCHAR* patternW = ToWStrTemp(pattern);
    HANDLE hFind = FindFirstFileW(patternW, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        TempStr fileName = ToUtf8Temp(fd.cFileName);
        // session files are named <uuid>.jsonl
        int nameLen = str::Leni(fileName);
        if (nameLen < 42) { // uuid (36) + .jsonl (6) = 42
            continue;
        }
        // extract session ID (remove .jsonl extension)
        TempStr sessionId = str::DupTemp(fileName, nameLen - 6);

        TempStr fullPath = str::FormatTemp("%s\\%s", projectDir, fileName);
        char* desc = GetSessionDescription(fullPath);

        // use file modification time as timestamp
        ULARGE_INTEGER uli;
        uli.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        i64 ts = (i64)(uli.QuadPart / 10000); // to milliseconds

        SessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = ts;
        sessions.Append(si);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    // sort by timestamp descending (most recent first)
    int n = sessions.Size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (sessions[j].timestamp < sessions[j + 1].timestamp) {
                SessionInfo tmp = sessions[j];
                sessions[j] = sessions[j + 1];
                sessions[j + 1] = tmp;
            }
        }
    }
}

static void FreeSessions(Vec<SessionInfo>& sessions) {
    for (int i = 0; i < sessions.Size(); i++) {
        str::Free(sessions[i].sessionId);
        str::Free(sessions[i].display);
        str::Free(sessions[i].project);
    }
    sessions.Reset();
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
    Vec<SessionInfo> sessions;
    CollectSessions(dir, sessions);

    int selectedIdx = 0;
    bool foundCurrent = false;
    for (int i = 0; i < sessions.Size(); i++) {
        TempStr label;
        int dispLen = str::Leni(sessions[i].display);
        if (dispLen > 50) {
            TempStr shortDisp = str::DupTemp(sessions[i].display, 50);
            label = str::FormatTemp("%.8s: %s...", sessions[i].sessionId, shortDisp);
        } else {
            label = str::FormatTemp("%.8s: %s", sessions[i].sessionId, sessions[i].display);
        }
        WCHAR* labelW = ToWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);

        if (tab->claudeSessionId && str::Eq(tab->claudeSessionId, sessions[i].sessionId)) {
            selectedIdx = i + 1;
            foundCurrent = true;
        }
    }

    // if current tab has a session but it wasn't found on disk, add it anyway
    if (tab->claudeSessionId && !foundCurrent) {
        TempStr label = str::FormatTemp("%.8s: (current session)", tab->claudeSessionId);
        WCHAR* labelW = ToWStrTemp(label);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labelW);
        selectedIdx = sessions.Size() + 1;
    }

    SendMessageW(combo, CB_SETCURSEL, selectedIdx, 0);
    FreeSessions(sessions);

    gPopulatingCombo = false;
}

// Load conversation history from a session's JSONL file
static void LoadSessionHistory(MainWindow* win, const char* sessionId, const char* dir) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr sessionPath = str::FormatTemp("%s\\.claude\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);

    if (!file::Exists(sessionPath)) {
        return;
    }

    ByteSlice data = file::ReadFile(sessionPath);
    if (data.empty()) {
        return;
    }

    const char* s = (const char*)data.data();
    const char* end = s + data.size();

    while (s < end) {
        const char* lineEnd = s;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') {
            lineEnd++;
        }
        if (lineEnd > s) {
            int lineLen = (int)(lineEnd - s);
            TempStr line = str::DupTemp(s, lineLen);

            // Session JSONL format:
            // User messages: content can be string or array
            // Assistant messages: content is always array

            TempStr userText = ExtractUserTextTemp(line);
            if (userText) {
                WebViewAddUser(win, userText);
            } else if (str::Find(line, "\"role\":\"assistant\"")) {
                // assistant message
                if (str::Find(line, "\"type\":\"thinking\"")) {
                    // skip thinking blocks
                } else if (str::Find(line, "\"type\":\"text\"")) {
                    TempStr text = JsonStrTemp(line, "text");
                    if (text && str::Len(text) > 0) {
                        WebViewAppendText(win, text);
                        WebViewFlushBlock(win);
                    }
                } else if (str::Find(line, "\"type\":\"tool_use\"")) {
                    TempStr toolName = JsonStrTemp(line, "name");
                    if (toolName) {
                        TempStr fp = JsonStrTemp(line, "file_path");
                        StrBuilder desc;
                        desc.AppendFmt("Tool: %s", toolName);
                        if (fp) {
                            desc.AppendFmt(" (%s)", fp);
                        }
                        WebViewAddTool(win, desc.LendData());
                    }
                }
            }
        }
        s = lineEnd;
        while (s < end && (*s == '\n' || *s == '\r')) {
            s++;
        }
    }

    data.Free();
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
        str::Free(tab->claudeSessionId);
        tab->claudeSessionId = nullptr;
        delete tab->claudeChatLog;
        tab->claudeChatLog = nullptr;
        WebViewClearChat(win);
        return;
    }

    // re-collect sessions to get the ID
    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<SessionInfo> sessions;
    CollectSessions(dir, sessions);

    int sessionIdx = sel - 1;
    if (sessionIdx >= 0 && sessionIdx < sessions.Size()) {
        str::Free(tab->claudeSessionId);
        tab->claudeSessionId = str::Dup(sessions[sessionIdx].sessionId);
        delete tab->claudeChatLog;
        tab->claudeChatLog = nullptr;
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
        // LoadSessionHistory calls WebViewEval which rebuilds claudeChatLog
    }

    FreeSessions(sessions);
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
    char* text;
    char* sessionId; // to identify which tab this belongs to
    ClaudeUpdateType updateType;
};

static void OnClaudeUpdate(ClaudeUpdateData* data) {
    MainWindow* win = FindMainWindowByFrame(data->hwndFrame);
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
                if (isActiveTab) {
                    WebViewFlushBlock(win);
                    SetClaudeWorking(win, false);
                    PopulateSessionCombo(win);
                }
                if (tab) {
                    tab->claudeProcess = nullptr;
                }
                break;
        }
    }
    str::Free(data->text);
    str::Free(data->sessionId);
    free(data);
}

static void PostUpdate(HWND hwndFrame, const char* sessionId, const char* text, ClaudeUpdateType type) {
    auto data = (ClaudeUpdateData*)calloc(1, sizeof(ClaudeUpdateData));
    data->hwndFrame = hwndFrame;
    data->sessionId = sessionId ? str::Dup(sessionId) : nullptr;
    data->text = text ? str::Dup(text) : nullptr;
    data->updateType = type;
    uitask::Post(MkFunc0(OnClaudeUpdate, data));
}

struct ClaudeReadCtx {
    HANDLE hReadPipe;
    HWND hwndFrame;
    char* sessionId;
};

static void ClaudeReadThread(ClaudeReadCtx* ctx) {
    HANDLE hPipe = ctx->hReadPipe;
    HWND hwndFrame = ctx->hwndFrame;
    char* sessionId = ctx->sessionId;
    free(ctx);

    StrBuilder lineBuf;
    char buf[4096];
    DWORD bytesRead;

    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buf[i] == '\n') {
                const char* line = lineBuf.LendData();
                TempStr eventType = JsonStrTemp(line, "type");

                if (eventType && str::Eq(eventType, "assistant")) {
                    if (str::Find(line, "\"type\":\"text\"")) {
                        TempStr text = JsonStrTemp(line, "text");
                        if (text && str::Len(text) > 0) {
                            PostUpdate(hwndFrame, sessionId, text, ClaudeUpdateType::Text);
                        }
                    }
                    if (str::Find(line, "\"type\":\"tool_use\"")) {
                        TempStr toolName = JsonStrTemp(line, "name");
                        if (toolName) {
                            TempStr fp = JsonStrTemp(line, "file_path");
                            TempStr cmd = JsonStrTemp(line, "command");
                            TempStr pat = JsonStrTemp(line, "pattern");
                            StrBuilder desc;
                            desc.AppendFmt("Tool: %s", toolName);
                            if (fp) {
                                desc.AppendFmt(" (%s)", fp);
                            } else if (cmd) {
                                if (str::Leni(cmd) > 60) {
                                    desc.AppendFmt(" $ %.60s...", cmd);
                                } else {
                                    desc.AppendFmt(" $ %s", cmd);
                                }
                            } else if (pat) {
                                desc.AppendFmt(" /%s/", pat);
                            }
                            PostUpdate(hwndFrame, sessionId, desc.LendData(), ClaudeUpdateType::Tool);
                        }
                    }
                } else if (eventType && str::Eq(eventType, "user")) {
                    if (str::Find(line, "\"tool_use_result\"")) {
                        TempStr fp = JsonStrTemp(line, "filePath");
                        if (fp) {
                            StrBuilder desc;
                            desc.AppendFmt("Result: %s", fp);
                            PostUpdate(hwndFrame, sessionId, desc.LendData(), ClaudeUpdateType::Tool);
                        }
                    }
                } else if (eventType && str::Eq(eventType, "result")) {
                    TempStr sub = JsonStrTemp(line, "subtype");
                    if (sub && str::Eq(sub, "error")) {
                        TempStr err = JsonStrTemp(line, "error");
                        if (err) {
                            PostUpdate(hwndFrame, sessionId, err, ClaudeUpdateType::Error);
                        }
                    }
                }

                lineBuf.Reset();
            } else if (buf[i] != '\r') {
                lineBuf.AppendChar(buf[i]);
            }
        }
    }

    CloseHandle(hPipe);
    PostUpdate(hwndFrame, sessionId, nullptr, ClaudeUpdateType::Finished);
    str::Free(sessionId);
}

static void StartClaudeReadThread(ClaudeReadCtx* ctx) {
    ClaudeReadThread(ctx);
}

static void SendClaudeMessage(MainWindow* win) {
    if (!win->claudeInput) {
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

    WCHAR* inputW = AllocArray<WCHAR>(inputLen + 1);
    GetWindowTextW(hwndInput, inputW, inputLen + 1);
    TempStr input = ToUtf8Temp(inputW);
    free(inputW);
    SetWindowTextW(hwndInput, L"");

    WebViewAddUser(win, input);
    SetClaudeWorking(win, true);

    bool isNewSession = (tab->claudeSessionId == nullptr);
    if (isNewSession) {
        tab->claudeSessionId = GenerateSessionId();
    }

    const char* filePath = tab->filePath;
    TempStr dir = path::GetDirTemp(filePath);
    TempStr fileName = path::GetBaseNameTemp(filePath);

    TempStr escapedInput = str::ReplaceTemp(input, "\"", "\\\"");

    // sync and save settings from UI
    SyncClaudeSettingsFromUI(win);

    const char* models[] = {"sonnet", "opus", "haiku"};
    const char* efforts[] = {"low", "medium", "high", "max"};
    int modelIdx = gClaudeSettings.modelIdx;
    int effortIdx = gClaudeSettings.effortIdx;
    if (modelIdx < 0 || modelIdx > 2) {
        modelIdx = 0;
    }
    if (effortIdx < 0 || effortIdx > 3) {
        effortIdx = 1;
    }
    const char* permsFlag = gClaudeSettings.skipPerms ? "--dangerously-skip-permissions" : "";

    // find claude executable
    TempStr claudePath = nullptr;
    {
        // check common install locations
        TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
        if (userProfile) {
            TempStr candidates[] = {
                str::FormatTemp("%s\\.local\\bin\\claude.exe", userProfile),
                str::FormatTemp("%s\\AppData\\Local\\Programs\\claude-code\\claude.exe", userProfile),
                str::FormatTemp("%s\\AppData\\Roaming\\npm\\claude.cmd", userProfile),
            };
            for (auto& c : candidates) {
                if (file::Exists(c)) {
                    claudePath = c;
                    break;
                }
            }
        }
        // fallback: SearchPath
        if (!claudePath) {
            WCHAR claudePathW[MAX_PATH];
            if (SearchPathW(nullptr, L"claude.exe", nullptr, MAX_PATH, claudePathW, nullptr) > 0) {
                claudePath = ToUtf8Temp(claudePathW);
            } else if (SearchPathW(nullptr, L"claude", L".exe", MAX_PATH, claudePathW, nullptr) > 0) {
                claudePath = ToUtf8Temp(claudePathW);
            }
        }
        if (!claudePath) {
            WebViewAddError(win, "Cannot find claude. Is Claude Code installed?");
            SetClaudeWorking(win, false);
            return;
        }
    }

    TempStr sessionName = str::FormatTemp("%s", fileName);

    TempStr cmdLine;
    if (isNewSession) {
        cmdLine = str::FormatTemp(
            "\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --session-id %s "
            "--name \"%s\" "
            "--append-system-prompt \"The user is currently reading the file: %s\" "
            "\"%s\"",
            claudePath, models[modelIdx], efforts[effortIdx], permsFlag, tab->claudeSessionId, sessionName, filePath,
            escapedInput);
    } else {
        cmdLine = str::FormatTemp(
            "\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --resume %s "
            "--append-system-prompt \"The user is currently reading the file: %s\" "
            "\"%s\"",
            claudePath, models[modelIdx], efforts[effortIdx], permsFlag, tab->claudeSessionId, filePath, escapedInput);
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        WebViewAddError(win, "Failed to create pipe");
        SetClaudeWorking(win, false);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    WCHAR* cmdLineW = ToWStrTemp(cmdLine);
    WCHAR* dirW = ToWStrTemp(dir);

    BOOL ok = CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, dirW, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        WebViewAddError(win, "Failed to launch claude. Is it installed and in PATH?");
        SetClaudeWorking(win, false);
        return;
    }

    CloseHandle(pi.hThread);
    tab->claudeProcess = pi.hProcess;

    auto ctx = (ClaudeReadCtx*)calloc(1, sizeof(ClaudeReadCtx));
    ctx->hReadPipe = hReadPipe;
    ctx->hwndFrame = win->hwndFrame;
    ctx->sessionId = str::Dup(tab->claudeSessionId);
    RunAsync(MkFunc0(StartClaudeReadThread, ctx), "ClaudeReadThread");
}

// --- Layout ---
static void LayoutClaudeBox(MainWindow* win) {
    HWND hwndContainer = win->hwndClaudeBox;
    Rect rc = ClientRect(hwndContainer);
    int y = 0;

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

    // bottom: [Model▾][Effort▾][☐Skip] on one row, then input
    Size inputSize = win->claudeInput->GetIdealSize();
    int inputDy = inputSize.dy + 4;
    int optRowDy = 32;
    int bottomDy = optRowDy + 4 + inputDy;

    int webViewDy = rc.dy - y - bottomDy;
    if (webViewDy < 0) {
        webViewDy = 0;
    }

    if (win->claudeWebView) {
        MoveWindow(win->claudeWebView->hwnd, 0, y, rc.dx, webViewDy, TRUE);
        // defer UpdateWebviewSize to avoid WebView2 put_Bounds freeze
        // use timer ID 43 with short delay
        SetTimer(win->hwndClaudeBox, 43, 50, nullptr);
    }
    y += webViewDy;

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
            MoveWindow(win->hwndClaudeSkipPermsCheck, x, y + 2, rc.dx - x - 2, optRowDy - 4, TRUE);
        }
    }
    y += optRowDy + 4;

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
}

// --- WndProc ---
static LRESULT CALLBACK WndProcClaudeInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                           DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
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
    int claudeDx = rFrame.dx - pcur.x;
    if (claudeDx < kClaudeMinDx || claudeDx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    win->claudeDx = claudeDx;
    if (ev->finishedDragging) {
        gClaudeSettings.claudeDx = claudeDx;
        SaveClaudeSettings();
        RelayoutForClaudeSplitter(win);
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
    webView->dataDir = str::Format("%s\\SumatraPDF\\ClaudeWebView_%d", userProfile, (int)GetCurrentProcessId());

    Rect rc = ClientRect(win->hwndClaudeBox);
    CreateWebViewArgs wvArgs;
    wvArgs.parent = win->hwndClaudeBox;
    wvArgs.pos = Rect(0, 0, rc.dx, rc.dy);
    webView->Create(wvArgs);

    if (webView->hwnd) {
        TempStr chatHtml = str::FormatTemp(kClaudeChatHtmlFmt, gClaudeSettings.bgColor);
        webView->SetHtml(chatHtml);
        win->claudeWebView = webView;
        win->claudeWebViewReady = true;
        LayoutClaudeBox(win);
    } else {
        delete webView;
    }
}

// --- Public API ---
void CreateClaudePanel(MainWindow* win) {
    LoadClaudeSettings();
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
    label->SetLabel("Claude Code");

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
    SendMessageW(win->hwndClaudeModelCombo, CB_ADDSTRING, 0, (LPARAM)L"Sonnet");
    SendMessageW(win->hwndClaudeModelCombo, CB_ADDSTRING, 0, (LPARAM)L"Opus");
    SendMessageW(win->hwndClaudeModelCombo, CB_ADDSTRING, 0, (LPARAM)L"Haiku");
    SendMessageW(win->hwndClaudeModelCombo, CB_SETCURSEL, 0, 0); // default: Sonnet

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
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = "Ask about this PDF...";
        input->Create(args);
    }
    win->claudeInput = input;

    UINT_PTR inputSubclassId = NextSubclassId();
    SetWindowSubclass(input->hwnd, WndProcClaudeInput, inputSubclassId, (DWORD_PTR)win);

    win->claudeBoxSubclassId = NextSubclassId();
    SetWindowSubclass(win->hwndClaudeBox, WndProcClaudeBox, win->claudeBoxSubclassId, (DWORD_PTR)win);

    ApplyClaudeSettingsToUI(win);
    if (gClaudeSettings.claudeDx > 0) {
        win->claudeDx = gClaudeSettings.claudeDx;
    }
}

// Auto-select the most recent session for the current tab if none is set
static void AutoSelectRecentSession(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || tab->claudeSessionId) {
        return; // already has a session or no file
    }

    TempStr dir = path::GetDirTemp(tab->filePath);
    Vec<SessionInfo> sessions;
    CollectSessions(dir, sessions);

    if (sessions.Size() > 0) {
        // sessions are sorted by timestamp desc, so [0] is most recent
        tab->claudeSessionId = str::Dup(sessions[0].sessionId);

        // load its history
        WebViewClearChat(win);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
    }

    FreeSessions(sessions);
}

void ToggleClaudePanel(MainWindow* win) {
    win->claudeVisible = !win->claudeVisible;
    HwndSetVisibility(win->hwndClaudeBox, win->claudeVisible);
    HwndSetVisibility(win->claudeSplitter->hwnd, win->claudeVisible);

    if (win->claudeVisible) {
        EnsureWebViewReady(win);
        PopulateSessionCombo(win);
        if (win->claudeInput) {
            HwndSetFocus(win->claudeInput->hwnd);
        }
        // defer auto-select so SetHtml has time to load the page
        SetTimer(win->hwndClaudeBox, 42, 500, nullptr);
    }
}

// call when switching tabs to update session context
void OnClaudeTabChanged(MainWindow* win) {
    if (!win->claudeVisible) {
        return;
    }
    PopulateSessionCombo(win);
    WebViewClearChat(win);

    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }

    // update working state for this tab
    SetClaudeWorking(win, tab->claudeProcess != nullptr);

    // if tab has in-memory chat log, replay it (fast, includes current session)
    if (tab->claudeChatLog && !tab->claudeChatLog->IsEmpty()) {
        ReplayChatLog(win, tab);
    } else if (tab->filePath && tab->claudeSessionId) {
        // fallback: load from disk
        TempStr dir = path::GetDirTemp(tab->filePath);
        LoadSessionHistory(win, tab->claudeSessionId, dir);
    }
}

void ShutdownClaudeForMainWindow(MainWindow* win) {
    if (!win) {
        return;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (tab && tab->claudeProcess) {
            TerminateProcess(tab->claudeProcess, 0);
            CloseHandle(tab->claudeProcess);
            tab->claudeProcess = nullptr;
        }
    }
    // read threads post uitask updates when their pipes close
    for (int i = 0; i < 20; i++) {
        uitask::DrainQueue();
        bool anyRunning = false;
        for (WindowTab* tab : win->Tabs()) {
            if (tab && tab->claudeProcess) {
                anyRunning = true;
            }
        }
        if (!anyRunning) {
            break;
        }
        Sleep(10);
    }
    uitask::DrainQueue();
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
    char* webViewDataDir = nullptr;
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
