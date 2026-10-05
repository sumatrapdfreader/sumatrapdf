/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct AIChatProvider;

bool IsClaudeCodeInstalled();
TempStr ClaudeCodeExecutablePathTemp();
AIChatProvider* GetClaudeCodeProvider();

bool IsGrokBuildInstalled();
TempStr GrokBuildExecutablePathTemp();
AIChatProvider* GetGrokBuildProvider();

bool IsCodexBuildInstalled();
TempStr CodexBuildExecutablePathTemp();
AIChatProvider* GetCodexBuildProvider();

constexpr char kAntiGravityDefaultModel[] = "gemini-3.8-flash-medium";
bool IsAntiGravityInstalled();
TempStr AntiGravityExecutablePathTemp();
AIChatProvider* GetAntiGravityProvider();

AIChatProvider* GetAIChatProvider(int providerId);

void CreateAIChatPanel(MainWindow* win);
void DestroyAIChatPanel(MainWindow* win);
void ShutdownAIChatForMainWindow(MainWindow* win);

void OnAIChatToggle(MainWindow* win, int providerId);
void OnAIChatTabChanged(MainWindow* win);
void UpdateAIChatTheme(MainWindow* win);

// ng: orig's RelayoutAIChatPanel / UpdateAIChatDpi are win32 relayouts; the
// shell rebuilds the panel every frame instead. What is left is the element
// and the deferred "pick the most recent session" the panel used a timer for
gpui::El* AIChatPanelBuild(MainWindow* win, gpui::Ctx* cx);
void AIChatTick(MainWindow* win, int elapsedMs);
// the width of the panel column, splitter included, or 0 when it is hidden
int AIChatPanelDx(MainWindow* win);
// what the panel shows right now, for the scripted tests
TempStr AIChatPanelStateTemp(MainWindow* win);

void AIChatHistoryAddUser(MainWindow* win, Str text);
void AIChatHistoryAppendText(MainWindow* win, Str text);
void AIChatHistoryAddTool(MainWindow* win, Str text);
void AIChatHistoryFlushBlock(MainWindow* win);
