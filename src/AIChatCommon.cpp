/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
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

#include "base/GuessFileType.h"

#include "AIChatCommon.h"
#include "EngineAll.h"

bool IsAIChatAvailable() {
    // the chat UI is a WebView
    return HasWebView();
}

bool IsAIChatSupportedForFile(Str filePath, Kind engineKind) {
    if (!filePath) {
        return false;
    }
    if (engineKind == kindEngineComicBooks || engineKind == kindEngineImageDir) {
        return false;
    }
    FileType kind = GuessFileTypeFromName(filePath);
    if (kind == FileType::PDF) {
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

TempStr AIChatJsEscapeTemp(Str s) {
    if (!s) {
        return str::DupTemp("");
    }
    str::Builder buf;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        switch (c) {
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
                buf.AppendChar(c);
                break;
        }
    }
    return ToStrTemp(buf);
}

TempStr AIChatJsonStrTemp(Str json, Str key) {
    TempStr pattern = fmt("\"%s\":\"", key);
    Str rest;
    if (!str::Cut(json, pattern, nullptr, &rest)) {
        return {};
    }
    str::Builder buf;
    for (int i = 0; i < rest.len; i++) {
        char c = rest.s[i];
        if (c == '"') {
            break;
        }
        if (c == '\\' && i + 1 < rest.len) {
            i++;
            c = rest.s[i];
            if (c == 'n') {
                buf.AppendChar('\n');
            } else if (c == 't') {
                buf.AppendChar('\t');
            } else if (c == '\\') {
                buf.AppendChar('\\');
            } else if (c == '"') {
                buf.AppendChar('"');
            } else {
                buf.AppendChar(c);
            }
        } else {
            buf.AppendChar(c);
        }
    }
    return ToStrTemp(buf);
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
    for (int i = 0; i < len(sessions); i++) {
        str::Free(sessions[i].sessionId);
        str::Free(sessions[i].display);
        str::Free(sessions[i].project);
    }
    sessions.Reset();
}

void AIChatSortSessionsByTimestampDesc(Vec<AIChatSessionInfo>& sessions) {
    int n = len(sessions);
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

void AIChatLog(AIChatLogger* logger, Str direction, Str text) {
    if (!logger) {
        return;
    }
    if (!text) {
        text = "";
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    str::Builder entry;
    entry.Append(fmt("[%04d-%02d-%02d %02d:%02d:%02d] %s: ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                     st.wSecond, direction));
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
    FILE* f = fopen(path.s, "a");
    if (f) {
        fwrite(ToStr(entry).s, 1, len(entry), f);
        fflush(f);
        fclose(f);
    }
    logger->mutex->Unlock();
}

constexpr int kBtnIdAIChatLearnMore = 100;

static HRESULT CALLBACK AIChatNotInstalledDialogCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                         LONG_PTR lpRefData) {
    Str docUri = lpRefData ? *(Str*)lpRefData : Str{};
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
    Str linkLabel = _TRA("AI Chat documentation");
    TempStr content = fmt(_TRA("See <a href=\"#\">%s</a> for setup instructions.").s, linkLabel);

    TASKDIALOG_BUTTON buttons[2];
    buttons[0].nButtonID = IDOK;
    buttons[0].pszButtonText = CWStrTemp(_TRA("OK"));
    buttons[1].nButtonID = kBtnIdAIChatLearnMore;
    buttons[1].pszButtonText = CWStrTemp(_TRA("Learn more"));

    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = CWStrTemp(args.windowTitle);
    dialogConfig.pszMainInstruction = CWStrTemp(args.mainInstruction);
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.pfCallback = AIChatNotInstalledDialogCallback;
    dialogConfig.lpCallbackData = (LONG_PTR)&args.docUri;
    dialogConfig.pButtons = buttons;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;

    TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
}

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, WStr searchExeName, WStr searchNameNoExt) {
#ifdef _MSC_VER
    for (int i = 0; i < len(fullPathCandidates); i++) {
        if (file::Exists(fullPathCandidates[i])) {
            // copy into the temp arena: callers pass a local StrVec that is
            // destroyed on return, so returning a view into it would dangle
            return str::DupTemp(fullPathCandidates[i]);
        }
    }
    WCHAR pathW[MAX_PATH];
    if (searchExeName && SearchPathW(nullptr, searchExeName.s, nullptr, MAX_PATH, pathW, nullptr) > 0) {
        return ToUtf8Temp(pathW);
    }
    if (searchNameNoExt && SearchPathW(nullptr, searchNameNoExt.s, L".exe", MAX_PATH, pathW, nullptr) > 0) {
        return ToUtf8Temp(pathW);
    }
#endif
    return nullptr;
}

void AIChatAppendModelUnique(StrVec& models, Str model) {
    if (len(model) == 0) {
        return;
    }
    TempStr norm = str::DupTemp(model);
    int start = 0;
    while (start < norm.len && str::IsWs(norm.s[start])) {
        start++;
    }
    if (start >= norm.len) {
        return;
    }
    norm = Str(norm.s + start, norm.len - start);
    str::ToLowerInPlace(norm);
    for (int i = 0; i < len(models); i++) {
        if (str::EqI(models[i], norm)) {
            return;
        }
    }
    models.Append(norm);
}

int AIChatFindModelInList(const StrVec& models, Str model) {
    if (len(model) == 0) {
        return -1;
    }
    TempStr norm = str::DupTemp(model);
    str::ToLowerInPlace(norm);
    for (int i = 0; i < len(models); i++) {
        if (str::EqI(models[i], norm)) {
            return i;
        }
    }
    return -1;
}

// the saved model if it's in the list, else defaultModel
Str AIChatResolveModel(const StrVec& models, Str model, Str defaultModel) {
    int idx = AIChatFindModelInList(models, model);
    if (idx >= 0) {
        return models[idx];
    }
    idx = AIChatFindModelInList(models, defaultModel);
    if (idx >= 0) {
        return models[idx];
    }
    return defaultModel;
}

TempStr AIChatModelDisplayNameTemp(Str model, Str defaultDisplay) {
    if (len(model) == 0) {
        return str::DupTemp(defaultDisplay ? defaultDisplay : StrL(""));
    }
    TempStr dup = str::DupTemp(model);
    if (len(dup) > 0) {
        dup.s[0] = (char)toupper((unsigned char)dup.s[0]);
    }
    return dup;
}

bool AIChatGetMarkedJsResource(void* ctx, Str path, WebViewResourceResult* res) {
    auto* data = (LoadedDataResource*)ctx;
    if (!data || !res || len(path) == 0) {
        return false;
    }
    if (!str::EqI(path, "/marked.min.js") && !str::EqI(path, "marked.min.js")) {
        return false;
    }
    res->data = data->data;
    res->dataLen = data->dataSize;
    res->contentType = str::Dup(StrL("text/javascript"));
    res->ownsData = false;
    return res->dataLen > 0;
}

static const char* kAIChatHtmlFmt = R"(<!DOCTYPE html><html><head><meta charset='utf-8'>
<script src='%smarked.min.js'></script>
<style>
:root { %s }
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Segoe UI', sans-serif; font-size: 13px; margin: 0; padding: 6px;
  background: var(--bg); color: var(--fg); line-height: 1.4; }
p { margin: 2px 0; }
h1,h2,h3,h4 { margin: 6px 0 2px 0; }
ul,ol { margin: 2px 0 2px 18px; }
li { margin: 1px 0; }
.user { color: var(--user); font-weight: bold; margin: 8px 0 2px 0; padding: 4px 0;
  border-top: 1px solid var(--border); }
.tool { color: var(--muted); font-size: 11px; font-style: italic;
  border-left: 3px solid var(--muted); padding-left: 6px; margin: 2px 0; }
.assistant { margin: 2px 0; }
.assistant pre { background: var(--code-bg); padding: 6px; border-radius: 4px;
  overflow-x: auto; margin: 3px 0; font-size: 12px; }
.assistant code { background: var(--code-bg); padding: 1px 3px; border-radius: 2px; font-size: 12px; }
.assistant pre code { background: none; padding: 0; }
.error { color: var(--error); font-weight: bold; margin: 4px 0; }
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

static TempStr ColorToCssTemp(COLORREF c) {
    return fmt("#%02x%02x%02x", (int)GetRValue(c), (int)GetGValue(c), (int)GetBValue(c));
}

// bgColor is the per-backend BgColor setting; "#ffffff" is its default value
// and means "follow the theme". An explicitly different color keeps the
// classic light chat colors on top of that background.
TempStr AIChatFormatChatHtmlTemp(Str virtualHost, Str bgColor) {
    Str host = virtualHost ? virtualHost : StrL("");
    bool followTheme = str::IsEmptyOrWhiteSpace(bgColor) || str::EqI(bgColor, StrL("#ffffff"));
    COLORREF themeBg = ThemeControlBackgroundColor();
    bool dark = followTheme && !IsLightColor(themeBg);
    TempStr bg = followTheme ? ColorToCssTemp(themeBg) : str::DupTemp(bgColor);
    TempStr fg = dark ? ColorToCssTemp(ThemeWindowTextColor()) : str::DupTemp("#222222");
    Str muted = dark ? StrL("#a0a0a0") : StrL("#555555");
    Str user = dark ? StrL("#7fb3d5") : StrL("#1a5276");
    Str border = dark ? StrL("#4a4a4a") : StrL("#cccccc");
    Str codeBg = dark ? StrL("#3a3a3a") : StrL("#f0f0f0");
    Str error = dark ? StrL("#e74c3c") : StrL("#c0392b");
    TempStr cssVars = fmt("--bg:%s; --fg:%s; --muted:%s; --user:%s; --border:%s; --code-bg:%s; --error:%s;", bg, fg,
                          muted, user, border, codeBg, error);
    return fmt(kAIChatHtmlFmt, host, cssVars);
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

bool AIChatLaunchProcessWithStdoutPipe(Str cmdLine, Str cwd, AIChatProcessLaunchResult* out) {
    if (!out || len(cmdLine) == 0) {
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
    WCHAR* cmdLineW = CWStrTemp(cmdLine);
    WCHAR* dirW = cwd ? CWStrTemp(cwd) : nullptr;

    BOOL ok = CreateProcessW(nullptr, cmdLineW, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd ? dirW : nullptr,
                             &si, &pi);
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

TempStr AIChatFitPanelTitleTemp(HWND labelHwnd, HFONT font, Str prefix, Str docName, int maxDx) {
    TempStr full = str::JoinTemp(prefix, docName);
    if (maxDx <= 0) {
        return full;
    }
    Size sz = HwndMeasureText(labelHwnd, full, font);
    if (sz.dx <= maxDx) {
        return full;
    }

    int nRunes = utf8StrLen((u8*)docName.s);
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

TempStr AIChatGenerateSessionIdTemp() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return fmt("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
               guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

static AIChatBackend BackendFromTabStorage(int v) {
    if (v < 0 || v > 2) {
        return AIChatBackend::None;
    }
    return (AIChatBackend)v;
}

static int BackendToTabStorage(AIChatBackend backend) {
    if (backend == AIChatBackend::None) {
        return -1;
    }
    return (int)backend;
}

AIChatBackend AIChatGetTabPanelOpen(WindowTab* tab) {
    if (!tab || tab->IsAboutTab()) {
        return AIChatBackend::None;
    }
    return BackendFromTabStorage(tab->aiChatPanelOpen);
}

void AIChatSetTabPanelOpen(WindowTab* tab, AIChatBackend backend) {
    if (!tab || tab->IsAboutTab()) {
        return;
    }
    tab->aiChatPanelOpen = BackendToTabStorage(backend);
}

// records the desired panel visibility; RelayoutFrame (via the scheduled UI
// update, which every caller triggers) shows/hides the panel windows
void AIChatSyncPanelsToCurrentTab(MainWindow* win) {
    if (!win) {
        return;
    }
    AIChatBackend open = AIChatGetTabPanelOpen(win->CurrentTab());
    win->uiState.aiChatVisible = open != AIChatBackend::None;
}

void AIChatApplySavedSidebarDx(MainWindow* win) {
    if (!win) {
        return;
    }
    if (gGlobalPrefs->aiChatSidebarDx > 0) {
        win->aiChatDx = gGlobalPrefs->aiChatSidebarDx;
    }
}

void AIChatUpdateSidebarDx(MainWindow* win, int dx, bool persist) {
    if (!win) {
        return;
    }
    win->aiChatDx = dx;
    if (dx > 0) {
        gGlobalPrefs->aiChatSidebarDx = dx;
    }
    if (persist) {
        SaveSettings();
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
