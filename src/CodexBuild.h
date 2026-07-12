/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;

bool IsCodexBuildAvailable();
bool IsCodexBuildInstalled();
TempStr CodexBuildExecutablePathTemp();
bool IsCodexBuildSupportedForFile(Str filePath, Kind engineKind = nullptr);
bool IsCodexBuildSupportedForTab(WindowTab* tab);

void CreateCodexPanel(MainWindow* win);
void OnAIChatWithOpenAICodex(MainWindow* win);
void ToggleCodexPanel(MainWindow* win);
void ShutdownCodexForMainWindow(MainWindow* win);
void DestroyCodexPanel(MainWindow* win);
void OnCodexTabChanged(MainWindow* win);

// called from SumatraPDF.cpp for width change relayout
// reposition children and repaint after the container is moved/resized
void RelayoutCodexPanel(MainWindow* win);