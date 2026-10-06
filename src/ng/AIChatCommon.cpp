/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AIChatCommon.cpp. The chat page html, the json helpers, the model
// list helpers and the session bookkeeping are orig's. What changed: launching
// the provider CLI and reading its stdout has POSIX plumbing too, the "not
// installed" TaskDialog is a gpui dialog, and the chat html asks the gpui theme
// for its colors.

#include "gui/GpuiBridge.h"

#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

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
#include "Commands.h"
#include "gui/AppShell.h"
#include "gui/BrowserView.h"
#include "gui/DialogWidgets.h"

#include "base/GuessFileType.h"

#include "EngineAll.h"
#include "AIChatCommon.h"

#include "SumatraLog.h"

bool IsAIChatAvailable() {
    // the chat UI is a WebView
    return BrowserViewAvailable();
}

bool IsAIChatSupportedForFile(Str filePath, Kind engineKind) {
    if (len(filePath) == 0) {
        return false;
    }
    // Comics, image folders, single images, and DjVu have no useful text/agent
    // payload for chat (menu/context hide these via IsAIChatSupportedForTab).
    if (engineKind == kindEngineComicBooks || engineKind == kindEngineImageDir || engineKind == kindEngineImage ||
        engineKind == kindEngineDjVu) {
        return false;
    }
    FileType kind = GuessFileTypeFromName(filePath);
    return kind == FileType::PDF;
}

bool IsAIChatSupportedForTab(WindowTab* tab) {
    if (!tab || tab->IsAboutTab() || len(tab->filePath) == 0) {
        return false;
    }
    return IsAIChatSupportedForFile(tab->filePath, tab->GetEngineType());
}

TempStr AIChatJsEscapeTemp(Str s) {
    if (len(s) == 0) {
        return str::DupTemp(StrL(""));
    }
    str::Builder buf;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        switch (c) {
            case '\\':
                buf.Append(StrL("\\\\"));
                break;
            case '\'':
                buf.Append(StrL("\\'"));
                break;
            case '\n':
                buf.Append(StrL("\\n"));
                break;
            case '\r':
                buf.Append(StrL("\\r"));
                break;
            case '\t':
                buf.Append(StrL("\\t"));
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

TempStr AIChatHomeDirTemp() {
#if OS_WIN
    return GetSpecialFolderTemp(CSIDL_PROFILE);
#else
    const char* home = getenv("HOME");
    return str::DupTemp(Str((char*)(home && *home ? home : "")));
#endif
}

void AIChatFreeSessions(Vec<AIChatSessionInfo>& sessions) {
    for (int i = 0; i < len(sessions); i++) {
        str::Free(sessions[i].sessionId);
        str::Free(sessions[i].display);
        str::Free(sessions[i].project);
    }
    VecReset(sessions);
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
    return (i64)(FileTimeToU64(ft) / 10000);
}

// In-memory record of the most recent chat traffic (sent commands + received
// stream), for post-mortem debugging when a chat fails. Fed from AIChatLog, so
// every ">>>"/"<<<" line the app already logs is captured here too. Bounded: the
// whole buffer is dropped once it grows past the cap.
static Mutex gAIChatDbgMu;
static str::Builder gAIChatDbgLog;
constexpr int kAIChatDbgMaxBytes = 256 * 1024;

void AIChatDebugReset() {
    ScopedMutex lk(&gAIChatDbgMu);
    gAIChatDbgLog.Reset();
}

TempStr AIChatDebugGetTemp() {
    ScopedMutex lk(&gAIChatDbgMu);
    return str::DupTemp(ToStr(gAIChatDbgLog));
}

// ng: orig stamps each entry with win32's GetLocalTime()
static TempStr LocalTimeStampTemp() {
    time_t t = time(nullptr);
    struct tm lt{};
#if OS_WIN
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return fmt("%04d-%02d-%02d %02d:%02d:%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min,
               lt.tm_sec);
}

void AIChatLog(AIChatLogger* logger, Str direction, Str text) {
    if (len(text) == 0) {
        text = StrL("");
    }

    str::Builder entry;
    entry.Append(fmt("[%s] %s: ", LocalTimeStampTemp(), direction));
    entry.Append(text);
    if (entry.LastChar() != '\n') {
        entry.AppendChar('\n');
    }

    {
        ScopedMutex lk(&gAIChatDbgMu);
        if (len(gAIChatDbgLog) > kAIChatDbgMaxBytes) {
            gAIChatDbgLog.Reset();
        }
        gAIChatDbgLog.Append(ToStr(entry));
    }

    if (!logger) {
        return;
    }
    if (logger->logTag) {
        logf("%s %s: %s", logger->logTag, direction, text);
    }

    TempStr dir = GetSumatraDataDirTemp();
    if (len(dir) == 0 || len(logger->logFileName) == 0) {
        return;
    }
    TempStr path = path::JoinTemp(dir, logger->logFileName);
    if (len(path) == 0 || !logger->mutex) {
        return;
    }

    logger->mutex->Lock();
    file::FileHandle h = file::OpenReadWrite(path, true);
    if (h != file::kInvalidFileHandle) {
        file::SeekEnd(h);
        file::WriteAll(h, ToStr(entry));
        file::Close(h);
    }
    logger->mutex->Unlock();
}

// --- the "not installed" dialog ---------------------------------------------

// ng: orig shows a TaskDialog with an OK and a "Learn more" button and a
// hyperlink in its content. gpui has no task dialog, so it is one of the port's
// Dialogs, with the same title, instruction, text and buttons.
struct NotInstalledDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str title;       // owned
    Str instruction; // owned
    Str docUri;      // owned
};

static NotInstalledDlg gNotInstalled;

struct NotInstalledView {
    static void OnOk(NotInstalledView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnLearnMore(NotInstalledView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<NotInstalledView> gNotInstalledView;

bool IsAIChatNotInstalledDialogVisible() {
    return gNotInstalled.visible;
}

void CloseAIChatNotInstalledDialog() {
    if (!gNotInstalled.visible) {
        return;
    }
    gNotInstalled.visible = false;
    str::FreePtr(&gNotInstalled.title);
    str::FreePtr(&gNotInstalled.instruction);
    str::FreePtr(&gNotInstalled.docUri);
    AppShellInvalidate(gNotInstalled.win);
}

void NotInstalledView::OnOk(NotInstalledView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseAIChatNotInstalledDialog();
    gp::Notify(cx);
}

void NotInstalledView::OnLearnMore(NotInstalledView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr uri = str::DupTemp(gNotInstalled.docUri);
    CloseAIChatNotInstalledDialog();
    LaunchDocumentation(uri);
    gp::Notify(cx);
}

void AIChatShowNotInstalledDialog(MainWindow* win, const AIChatNotInstalledDialogArgs& args) {
    CloseAIChatNotInstalledDialog();
    gNotInstalled.win = win;
    gNotInstalled.title = str::Dup(args.windowTitle);
    gNotInstalled.instruction = str::Dup(args.mainInstruction);
    gNotInstalled.docUri = str::Dup(args.docUri);
    gNotInstalled.visible = true;
    logf("AIChatShowNotInstalledDialog: '%s'\n", gNotInstalled.instruction);
    AppShellInvalidate(win);
}

gp::El* AIChatNotInstalledDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gNotInstalled.visible || gNotInstalled.win != win) {
        return nullptr;
    }
    if (!gNotInstalledView.IsValid()) {
        gNotInstalledView = gp::EntityNewState<NotInstalledView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    TempStr content = fmt(Tr("See %s for setup instructions.").s, Tr("AI Chat documentation"));

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);
    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gNotInstalled.instruction))->Font(14)->Fg(th.foreground));
    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, content))->Font(13)->Fg(th.mutedFg));

    gp::El* learnMore = gpc::Button::New(cx, GStrL("aichat-learn-more"))
                            ->Label(ToGpui(Tr("Learn more")))
                            ->WithSize(gp::UiSize::Small)
                            ->OnClick(gp::ListenTo(gNotInstalledView, &NotInstalledView::OnLearnMore))
                            ->IntoEl();
    gp::El* footer = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    footer->Child(learnMore);
    footer->Child(gp::Div(cx->a)->Flex1());
    footer->Child(gpc::Button::New(cx, GStrL("aichat-ok"))
                      ->Label(ToGpui(Tr("OK")))
                      ->Primary()
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gNotInstalledView, &NotInstalledView::OnOk))
                      ->IntoEl());

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(GpuiDup(cx->a, gNotInstalled.title))
        ->Body(body)
        ->Footer(footer)
        ->W(460)
        ->OnClose(gp::ListenTo(gNotInstalledView, &NotInstalledView::OnOk))
        ->IntoEl(gp::WindowSize(cx->win));
}

void AIChatAppendModelUnique(StrVec& models, Str model) {
    str::TrimWsBoth(model);
    if (len(model) == 0) {
        return;
    }
    TempStr norm = str::DupTemp(model);
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
function sanitizedMarkdown(markdown) {
  var t = document.createElement('template');
  t.innerHTML = marked.parse(markdown);
  var allowed = new Set(['A', 'BLOCKQUOTE', 'BR', 'CODE', 'DEL', 'EM', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6',
                         'HR', 'LI', 'OL', 'P', 'PRE', 'STRONG', 'TABLE', 'TBODY', 'TD', 'TH', 'THEAD', 'TR', 'UL']);
  var nodes = Array.from(t.content.querySelectorAll('*'));
  for (var el of nodes) {
    if (!allowed.has(el.tagName)) {
      el.replaceWith(document.createTextNode(el.textContent || ''));
      continue;
    }
    for (var attr of Array.from(el.attributes)) {
      var keep = el.tagName === 'A' && (attr.name === 'href' || attr.name === 'title');
      if (!keep) {
        el.removeAttribute(attr.name);
      }
    }
    if (el.tagName === 'A' && el.hasAttribute('href')) {
      try {
        var u = new URL(el.getAttribute('href'), location.href);
        if (!['http:', 'https:', 'mailto:'].includes(u.protocol)) el.removeAttribute('href');
      } catch (_) {
        el.removeAttribute('href');
      }
    }
  }
  return t.content;
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
    currentBlock.replaceChildren(sanitizedMarkdown(currentRaw));
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

// bgColor is the per-backend BgColor setting; "#ffffff" is its default value
// and means "follow the theme". An explicitly different color keeps the
// classic light chat colors on top of that background.
TempStr AIChatFormatChatHtmlTemp(Str virtualHost, Str bgColor) {
    Str host = virtualHost ? virtualHost : StrL("");
    bool followTheme = str::IsEmptyOrWhiteSpace(bgColor) || str::EqI(bgColor, StrL("#ffffff"));
    Color themeBg = ThemeControlBackgroundColor();
    bool dark = followTheme && !IsLightColor(themeBg);
    TempStr bg = followTheme ? ColorToCssTemp(themeBg) : str::DupTemp(bgColor);
    TempStr fg = dark ? ColorToCssTemp(ThemeWindowTextColor()) : str::DupTemp(StrL("#222222"));
    Str muted = dark ? StrL("#a0a0a0") : StrL("#555555");
    Str user = dark ? StrL("#7fb3d5") : StrL("#1a5276");
    Str border = dark ? StrL("#4a4a4a") : StrL("#cccccc");
    Str codeBg = dark ? StrL("#3a3a3a") : StrL("#f0f0f0");
    Str error = dark ? StrL("#e74c3c") : StrL("#c0392b");
    TempStr cssVars = fmt("--bg:%s; --fg:%s; --muted:%s; --user:%s; --border:%s; --code-bg:%s; --error:%s;", bg, fg,
                          muted, user, border, codeBg, error);
    return fmt(kAIChatHtmlFmt, host, cssVars);
}

TempStr AIChatGenerateSessionIdTemp() {
    u8 b[16];
    if (!gpui::shell::SecureRandom(b, sizeof(b))) {
        return {};
    }
    return AIChatFormatSessionIdTemp(b);
}

static AIChatBackend BackendFromTabStorage(int v) {
    if (v < 0 || v >= kAIChatProviderCount) {
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

// records the desired panel visibility; the shell's frame shows / hides the
// panel from it on the next paint
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
    if (gSettings->aiChatSidebarDx > 0) {
        win->aiChatDx = gSettings->aiChatSidebarDx;
    }
}

void AIChatUpdateSidebarDx(MainWindow* win, int dx, bool persist) {
    if (!win) {
        return;
    }
    win->aiChatDx = dx;
    if (dx > 0) {
        gSettings->aiChatSidebarDx = dx;
    }
    if (persist) {
        ScheduleSaveSettings();
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
        SleepInMs(10);
    }
    uitask::DrainQueue();
}
