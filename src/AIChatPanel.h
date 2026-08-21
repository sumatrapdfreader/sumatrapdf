/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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

void AIChatHistoryAddUser(MainWindow* win, Str text);
void AIChatHistoryAppendText(MainWindow* win, Str text);
void AIChatHistoryAddTool(MainWindow* win, Str text);
void AIChatHistoryFlushBlock(MainWindow* win);
