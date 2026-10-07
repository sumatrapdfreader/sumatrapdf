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
void UpdateAIChatDpi(MainWindow* win, int dpi);
void RelayoutAIChatPanel(MainWindow* win);

gpui::El* AIChatPanelBuild(MainWindow* win, gpui::Ctx* cx);
void AIChatTick(MainWindow* win, int elapsedMs);
int AIChatPanelDx(MainWindow* win);
TempStr AIChatPanelStateTemp(MainWindow* win);

void AIChatHistoryAddUser(MainWindow* win, Str text);
void AIChatHistoryAppendText(MainWindow* win, Str text);
void AIChatHistoryAddTool(MainWindow* win, Str text);
void AIChatHistoryFlushBlock(MainWindow* win);
