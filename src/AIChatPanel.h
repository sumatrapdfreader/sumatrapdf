/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct AIChatProvider;

// the providers (implemented in ClaudeCode.cpp, GrokBuild.cpp, CodexBuild.cpp)
bool IsClaudeCodeInstalled();
TempStr ClaudeCodeExecutablePathTemp();
AIChatProvider* GetClaudeCodeProvider();

bool IsGrokBuildInstalled();
TempStr GrokBuildExecutablePathTemp();
AIChatProvider* GetGrokBuildProvider();

bool IsCodexBuildInstalled();
TempStr CodexBuildExecutablePathTemp();
AIChatProvider* GetCodexBuildProvider();

// providerId is an AIChatBackend value (0=Claude, 1=Grok, 2=Codex)
AIChatProvider* GetAIChatProvider(int providerId);

void CreateAIChatPanel(MainWindow* win);
void DestroyAIChatPanel(MainWindow* win);
void ShutdownAIChatForMainWindow(MainWindow* win);

// command entry point: toggle the panel for the given provider
void OnAIChatToggle(MainWindow* win, int providerId);
void OnAIChatTabChanged(MainWindow* win);
void UpdateAIChatTheme(MainWindow* win);

// called from SumatraPDF.cpp for width change relayout
// reposition children and repaint after the container is moved/resized
void RelayoutAIChatPanel(MainWindow* win);

// used by providers to replay session history into the chat
void AIChatHistoryAddUser(MainWindow* win, Str text);
void AIChatHistoryAppendText(MainWindow* win, Str text);
void AIChatHistoryAddTool(MainWindow* win, Str text);
void AIChatHistoryFlushBlock(MainWindow* win);
