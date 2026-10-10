/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"
#include "EmbeddedResources.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "AIChatPanelCommon.h"

// path is host-relative, without a leading slash (e.g. "index.html")
bool AIChatPathIs(Str path, Str name) {
    if (str::EqI(path, name)) {
        return true;
    }
    return path.len > 0 && path.s[0] == '/' && str::EqI(Str(path.s + 1, path.len - 1), name);
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

AIChatProvider* CurrentProvider(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    return GetAIChatProvider(win->aiChatProvider);
}

AIChatTabState* GetTabState(WindowTab* tab, int providerId) {
    if (!tab || providerId < 0 || providerId >= kAIChatProviderCount) {
        return nullptr;
    }
    return &tab->aiChat[providerId];
}

Str kAIChatPendingSessionId() {
    return StrL("pending");
}

Str BgColorForProvider(AIChatProvider* p) {
    Str bg = p->GetBgColor();
    if (len(bg) == 0) {
        return StrL("#ffffff");
    }
    return bg;
}

void WebViewAppendText(MainWindow* win, Str text) {
    TempStr js = fmt("appendText('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddUser(MainWindow* win, Str text) {
    TempStr js = fmt("addUser('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddTool(MainWindow* win, Str text) {
    TempStr js = fmt("addTool('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewAddError(MainWindow* win, Str text) {
    AIChatProvider* p = CurrentProvider(win);
    if (p) {
        AIChatLog(p->logger, StrL("error"), text);
    }
    TempStr js = fmt("addError('%s')", AIChatJsEscapeTemp(text));
    WebViewEval(win, js);
}

void WebViewFlushBlock(MainWindow* win) {
    WebViewEval(win, StrL("flushBlock()"));
}

void WebViewClearChat(MainWindow* win) {
    WebViewEval(win, StrL("clearChat()"), false); // don't record clear
}

void WebViewShowUnsupportedFileType(MainWindow* win) {
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

void SetAIChatWorking(MainWindow* win, bool /*working*/) {
    UpdateAIChatPanelForCurrentTab(win);
}

// the tab an update belongs to; prefer tabs with a running process
WindowTab* FindAIChatUpdateTab(MainWindow* win, int pid, Str sessionId) {
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

// record a session id the provider assigned mid-stream
void AIChatStreamSetSessionId(AIChatStreamCtx* ctx, Str sessionId) {
    AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, sessionId);
    str::ReplaceWithCopy(&ctx->sessionId, sessionId);
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
