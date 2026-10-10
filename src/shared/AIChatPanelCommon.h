/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AIChatPanelCommon.cpp and each app's AIChatPanel.cpp ---

bool AIChatPathIs(Str path, Str name);
AIChatProvider* CurrentProvider(MainWindow* win);
AIChatTabState* GetTabState(WindowTab* tab, int providerId);
Str kAIChatPendingSessionId();
Str BgColorForProvider(AIChatProvider* p);
void WebViewAppendText(MainWindow* win, Str text);
void WebViewAddUser(MainWindow* win, Str text);
void WebViewAddTool(MainWindow* win, Str text);
void WebViewAddError(MainWindow* win, Str text);
void WebViewFlushBlock(MainWindow* win);
void WebViewClearChat(MainWindow* win);
void WebViewShowUnsupportedFileType(MainWindow* win);
void SetAIChatWorking(MainWindow* win, bool /*working*/);
WindowTab* FindAIChatUpdateTab(MainWindow* win, int pid, Str sessionId);

// implemented by each app
void WebViewEval(MainWindow* win, Str js, bool record = true);
void UpdateAIChatPanelForCurrentTab(MainWindow* win);
bool RunAIChatSync(AIChatBackend backend, Str filePath, Str message, Str& outText, Str& outErr);
