/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;

bool IsClaudeCodeAvailable();
bool IsClaudeCodeInstalled();
TempStr ClaudeCodeExecutablePathTemp();
bool IsClaudeCodeSupportedForFile(Str filePath, Kind engineKind = nullptr);
bool IsClaudeCodeSupportedForTab(WindowTab* tab);

void CreateClaudePanel(MainWindow* win);
void OnAIChatWithClaudeCode(MainWindow* win);
void ToggleClaudePanel(MainWindow* win);
void ShutdownClaudeForMainWindow(MainWindow* win);
void DestroyClaudePanel(MainWindow* win);
void OnClaudeTabChanged(MainWindow* win);
void UpdateClaudeTheme(MainWindow* win);

// called from SumatraPDF.cpp for width change relayout
// reposition children and repaint after the container is moved/resized
void RelayoutClaudePanel(MainWindow* win);
