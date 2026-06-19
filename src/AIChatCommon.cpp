/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/Log.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "resource.h"

#include "utils/GuessFileType.h"

#include "AppTools.h"
#include "AIChatCommon.h"
#include "EngineAll.h"

bool IsAIChatAvailable() {
#ifdef _MSC_VER
    return IsWindows10OrGreater();
#else
    return false;
#endif
}

bool IsAIChatSupportedForFile(const char* filePath, Kind engineKind) {
    if (!filePath) {
        return false;
    }
    if (engineKind == kindEngineComicBooks || engineKind == kindEngineImageDir) {
        return false;
    }
    Kind kind = GuessFileTypeFromName(filePath);
    if (kind == kindFilePDF) {
        return true;
    }
    return IsEngineImageSupportedFileType(kind);
}

bool IsAIChatSupportedForTab(WindowTab* tab) {
    if (!tab || tab->IsAboutTab() || !tab->filePath) {
        return false;
    }
    return IsAIChatSupportedForFile(tab->filePath, tab->GetEngineType());
}

TempStr AIChatJsEscapeTemp(const char* s) {
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

TempStr AIChatJsonStrTemp(const char* json, const char* key) {
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

MainWindow* AIChatFindMainWindowByFrame(HWND hwndFrame) {
    for (MainWindow* w : gWindows) {
        if (w->hwndFrame == hwndFrame) {
            return w;
        }
    }
    return nullptr;
}

void AIChatFreeSessions(Vec<AIChatSessionInfo>& sessions) {
    for (int i = 0; i < sessions.Size(); i++) {
        str::Free(sessions[i].sessionId);
        str::Free(sessions[i].display);
        str::Free(sessions[i].project);
    }
    sessions.Reset();
}

void AIChatSortSessionsByTimestampDesc(Vec<AIChatSessionInfo>& sessions) {
    int n = sessions.Size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (sessions[j].timestamp < sessions[j + 1].timestamp) {
                AIChatSessionInfo tmp = sessions[j];
                sessions[j] = sessions[j + 1];
                sessions[j + 1] = tmp;
            }
        }
    }
}

i64 AIChatFileTimeToMs(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (i64)(uli.QuadPart / 10000);
}

void AIChatLog(AIChatLogger* logger, const char* direction, const char* text) {
    if (!logger) {
        return;
    }
    if (!text) {
        text = "";
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    StrBuilder entry;
    entry.AppendFmt("[%04d-%02d-%02d %02d:%02d:%02d] %s: ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                    st.wSecond, direction);
    entry.Append(text);
    if (entry.LastChar() != '\n') {
        entry.AppendChar('\n');
    }

    if (logger->logTag) {
        logfa("%s %s: %s", logger->logTag, direction, text);
    }

    TempStr dir = GetNotImportantDataDirTemp();
    if (!dir || !logger->logFileName) {
        return;
    }
    TempStr path = path::JoinTemp(dir, logger->logFileName);
    if (!path || !logger->mutex) {
        return;
    }

    logger->mutex->Lock();
    FILE* f = fopen(path, "a");
    if (f) {
        fwrite(entry.Get(), 1, entry.Size(), f);
        fflush(f);
        fclose(f);
    }
    logger->mutex->Unlock();
}

constexpr int kBtnIdAIChatLearnMore = 100;

static HRESULT CALLBACK AIChatNotInstalledDialogCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                         LONG_PTR lpRefData) {
    const char* docUri = (const char*)lpRefData;
    switch (msg) {
        case TDN_HYPERLINK_CLICKED:
            LaunchDocumentation(docUri);
            break;
        case TDN_BUTTON_CLICKED:
            if ((int)wParam == kBtnIdAIChatLearnMore) {
                LaunchDocumentation(docUri);
                return S_FALSE;
            }
            break;
    }
    return S_OK;
}

void AIChatShowNotInstalledDialog(const AIChatNotInstalledDialogArgs& args) {
    const char* linkLabel = _TRA("AI Chat documentation");
    TempStr content = str::FormatTemp(_TRA("See <a href=\"#\">%s</a> for setup instructions."), linkLabel);

    TASKDIALOG_BUTTON buttons[2];
    buttons[0].nButtonID = IDOK;
    buttons[0].pszButtonText = ToWStrTemp(_TRA("OK"));
    buttons[1].nButtonID = kBtnIdAIChatLearnMore;
    buttons[1].pszButtonText = ToWStrTemp(_TRA("Learn more"));

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = ToWStrTemp(args.windowTitle);
    dialogConfig.pszMainInstruction = ToWStrTemp(args.mainInstruction);
    dialogConfig.pszContent = ToWStrTemp(content);
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.pfCallback = AIChatNotInstalledDialogCallback;
    dialogConfig.lpCallbackData = (LONG_PTR)args.docUri;
    dialogConfig.pButtons = buttons;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;

    TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
}

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, const WCHAR* searchExeName,
                                 const WCHAR* searchNameNoExt) {
#ifdef _MSC_VER
    for (int i = 0; i < fullPathCandidates.Size(); i++) {
        if (file::Exists(fullPathCandidates.At(i))) {
            return fullPathCandidates.At(i);
        }
    }
    WCHAR pathW[MAX_PATH];
    if (searchExeName && SearchPathW(nullptr, searchExeName, nullptr, MAX_PATH, pathW, nullptr) > 0) {
        return ToUtf8Temp(pathW);
    }
    if (searchNameNoExt && SearchPathW(nullptr, searchNameNoExt, L".exe", MAX_PATH, pathW, nullptr) > 0) {
        return ToUtf8Temp(pathW);
    }
#endif
    return nullptr;
}

void AIChatAppendModelUnique(StrVec& models, const char* model) {
    if (str::IsEmpty(model)) {
        return;
    }
    TempStr norm = str::DupTemp(model);
    char* s = norm;
    while (str::IsWs(*s)) {
        s++;
    }
    if (!*s) {
        return;
    }
    str::ToLowerInPlace(s);
    for (int i = 0; i < models.Size(); i++) {
        if (str::EqI(models.At(i), s)) {
            return;
        }
    }
    models.Append(s);
}

int AIChatFindModelInList(const StrVec& models, const char* model) {
    if (str::IsEmpty(model)) {
        return -1;
    }
    TempStr norm = str::DupTemp(model);
    str::ToLowerInPlace(norm);
    for (int i = 0; i < models.Size(); i++) {
        if (str::EqI(models.At(i), norm)) {
            return i;
        }
    }
    return -1;
}

TempStr AIChatModelDisplayNameTemp(const char* model, const char* defaultDisplay) {
    if (str::IsEmpty(model)) {
        return str::DupTemp(defaultDisplay ? defaultDisplay : "");
    }
    char* dup = str::DupTemp(model);
    dup[0] = (char)toupper((unsigned char)dup[0]);
    return dup;
}

bool AIChatGetMarkedJsResource(void* ctx, const char* path, WebViewResourceResult* res) {
    auto* data = (LoadedDataResource*)ctx;
    if (!data || !res || str::IsEmpty(path)) {
        return false;
    }
    if (!str::EqI(path, "/marked.min.js") && !str::EqI(path, "marked.min.js")) {
        return false;
    }
    res->data = (char*)data->data;
    res->dataLen = data->dataSize;
    res->contentType = str::Dup("text/javascript");
    res->ownsData = false;
    return res->dataLen > 0;
}

static const char* kAIChatHtmlFmt = R"(<!DOCTYPE html><html><head><meta charset='utf-8'>
<script src='%smarked.min.js'></script>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Segoe UI', sans-serif; font-size: 13px; margin: 0; padding: 6px;
  background: %s; color: #222; line-height: 1.4; }
p { margin: 2px 0; }
h1,h2,h3,h4 { margin: 6px 0 2px 0; }
ul,ol { margin: 2px 0 2px 18px; }
li { margin: 1px 0; }
.user { color: #1a5276; font-weight: bold; margin: 8px 0 2px 0; padding: 4px 0;
  border-top: 1px solid #ccc; }
.tool { color: #555; font-size: 11px; font-style: italic;
  border-left: 3px solid #999; padding-left: 6px; margin: 2px 0; }
.assistant { margin: 2px 0; }
.assistant pre { background: #f0f0f0; padding: 6px; border-radius: 4px;
  overflow-x: auto; margin: 3px 0; font-size: 12px; }
.assistant code { background: #e8e8e8; padding: 1px 3px; border-radius: 2px; font-size: 12px; }
.assistant pre code { background: none; padding: 0; }
.error { color: #c0392b; font-weight: bold; margin: 4px 0; }
</style></head><body><div id='chat'></div>
<script>
var chatDiv = document.getElementById('chat');
var currentBlock = null;
var currentRaw = '';
function addUser(text) {
  flushBlock();
  var d = document.createElement('div');
  d.className = 'user';
  d.textContent = 'You: ' + text;
  chatDiv.appendChild(d);
  scrollToBottom();
}
function addTool(text) {
  flushBlock();
  var d = document.createElement('div');
  d.className = 'tool';
  d.textContent = text;
  chatDiv.appendChild(d);
  scrollToBottom();
}
function addError(text) {
  flushBlock();
  var d = document.createElement('div');
  d.className = 'error';
  d.textContent = text;
  chatDiv.appendChild(d);
  scrollToBottom();
}
function appendText(text) {
  if (!currentBlock) {
    currentBlock = document.createElement('div');
    currentBlock.className = 'assistant';
    chatDiv.appendChild(currentBlock);
    currentRaw = '';
  }
  currentRaw += text;
  if (typeof marked !== 'undefined') {
    currentBlock.innerHTML = marked.parse(currentRaw);
  } else {
    currentBlock.textContent = currentRaw;
  }
  scrollToBottom();
}
function flushBlock() {
  currentBlock = null; currentRaw = '';
}
function clearChat() {
  chatDiv.innerHTML = '';
  flushBlock();
}
function scrollToBottom() {
  window.scrollTo(0, document.body.scrollHeight);
}
</script></body></html>)";

TempStr AIChatFormatChatHtmlTemp(const char* virtualHost, const char* bgColor) {
    const char* host = virtualHost ? virtualHost : "";
    const char* bg = bgColor ? bgColor : "#ffffff";
    return str::FormatTemp(kAIChatHtmlFmt, host, bg);
}

void AIChatCloseProcess(HANDLE* processHandle, bool terminateIfRunning) {
    if (!processHandle || !*processHandle) {
        return;
    }
    HANDLE h = *processHandle;
    *processHandle = nullptr;
    if (terminateIfRunning && WaitForSingleObject(h, 0) == WAIT_TIMEOUT) {
        TerminateProcess(h, 0);
    }
    CloseHandle(h);
}

bool AIChatLaunchProcessWithStdoutPipe(const char* cmdLine, const char* cwd, AIChatProcessLaunchResult* out) {
    if (!out || str::IsEmpty(cmdLine)) {
        return false;
    }
    *out = {};

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    WCHAR* cmdLineW = ToWStrTemp(cmdLine);
    WCHAR* dirW = cwd ? ToWStrTemp(cwd) : nullptr;

    BOOL ok = CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, dirW, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        return false;
    }

    CloseHandle(pi.hThread);
    out->ok = true;
    out->hProcess = pi.hProcess;
    out->hReadPipe = hReadPipe;
    out->processId = pi.dwProcessId;
    return true;
}

constexpr int kAIChatLabelCloseBtnDx = 16;
constexpr int kAIChatLabelCloseBtnSpaceDx = 8;
constexpr int kAIChatLabelPadX = 2;

int AIChatLabelMaxTextDx(HWND labelHwnd, int labelDx) {
    int padX = DpiScale(labelHwnd, kAIChatLabelPadX);
    int btnDx = DpiScale(labelHwnd, kAIChatLabelCloseBtnDx);
    int spaceDx = DpiScale(labelHwnd, kAIChatLabelCloseBtnSpaceDx);
    int maxDx = labelDx - btnDx - spaceDx - 2 * padX;
    return maxDx > 0 ? maxDx : 0;
}

TempStr AIChatFitPanelTitleTemp(HWND labelHwnd, HFONT font, const char* prefix, const char* docName, int maxDx) {
    TempStr full = str::JoinTemp(prefix, docName);
    if (maxDx <= 0) {
        return full;
    }
    Size sz = HwndMeasureText(labelHwnd, full, font);
    if (sz.dx <= maxDx) {
        return full;
    }

    int nRunes = utf8StrLen((u8*)docName);
    if (nRunes < 0) {
        return full;
    }

    TempStr best = str::JoinTemp(prefix, ShortenStringUtf8Temp(docName, 1));
    int lo = 1;
    int hi = nRunes;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        TempStr trial = str::JoinTemp(prefix, ShortenStringUtf8Temp(docName, mid));
        sz = HwndMeasureText(labelHwnd, trial, font);
        if (sz.dx <= maxDx) {
            best = trial;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

char* AIChatGenerateSessionId() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return str::Format("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3,
                       guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
                       guid.Data4[6], guid.Data4[7]);
}

static void HideClaudePanel(MainWindow* win) {
    win->claudeVisible = false;
    if (win->hwndClaudeBox) {
        HwndSetVisibility(win->hwndClaudeBox, false);
    }
    if (win->claudeSplitter && win->claudeSplitter->hwnd) {
        HwndSetVisibility(win->claudeSplitter->hwnd, false);
    }
}

static void HideGrokPanel(MainWindow* win) {
    win->grokVisible = false;
    if (win->hwndGrokBox) {
        HwndSetVisibility(win->hwndGrokBox, false);
    }
    if (win->grokSplitter && win->grokSplitter->hwnd) {
        HwndSetVisibility(win->grokSplitter->hwnd, false);
    }
}

static void HideCodexPanel(MainWindow* win) {
    win->codexVisible = false;
    if (win->hwndCodexBox) {
        HwndSetVisibility(win->hwndCodexBox, false);
    }
    if (win->codexSplitter && win->codexSplitter->hwnd) {
        HwndSetVisibility(win->codexSplitter->hwnd, false);
    }
}

void AIChatHideOtherPanels(MainWindow* win, AIChatBackend keepVisible) {
    if (!win) {
        return;
    }
    if (keepVisible != AIChatBackend::Claude) {
        HideClaudePanel(win);
    }
    if (keepVisible != AIChatBackend::Grok) {
        HideGrokPanel(win);
    }
    if (keepVisible != AIChatBackend::Codex) {
        HideCodexPanel(win);
    }
}

void AIChatWaitForTabProcessesToFinish(MainWindow* win, bool (*tabHasRunningProcess)(WindowTab*)) {
    if (!win || !tabHasRunningProcess) {
        return;
    }
    for (int i = 0; i < 20; i++) {
        uitask::DrainQueue();
        bool anyRunning = false;
        for (WindowTab* tab : win->Tabs()) {
            if (tab && tabHasRunningProcess(tab)) {
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