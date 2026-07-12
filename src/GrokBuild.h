/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;

bool IsGrokBuildAvailable();
bool IsGrokBuildInstalled();
TempStr GrokBuildExecutablePathTemp();
bool IsGrokBuildSupportedForFile(Str filePath, Kind engineKind = nullptr);
bool IsGrokBuildSupportedForTab(WindowTab* tab);

void CreateGrokPanel(MainWindow* win);
void OnAIChatWithGrokBuild(MainWindow* win);
void ToggleGrokPanel(MainWindow* win);
void ShutdownGrokForMainWindow(MainWindow* win);
void DestroyGrokPanel(MainWindow* win);
void OnGrokTabChanged(MainWindow* win);
void UpdateGrokTheme(MainWindow* win);

// called from SumatraPDF.cpp for width change relayout
// reposition children and repaint after the container is moved/resized
void RelayoutGrokPanel(MainWindow* win);