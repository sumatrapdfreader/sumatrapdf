/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// type of path information retrieved from the registy
enum class RegType {
    BinaryPath,  // full path to the editor's binary file
    BinaryDir,   // directory containing the editor's binary file
    SiblingPath, // full path to a sibling file of the editor's binary file
    None,
};

struct TextEditor {
    // Editor's binary file name
    const char* binaryFilename = nullptr;
    // Parameters to be passed to the editor;
    // use placeholder '%f' for path to source file and '%l' for line number.
    const char* inverseSearchArgs = nullptr;
    // Type of the path information obtained from the registry
    RegType type;
    // Registry key path
    const char* regKey = nullptr;
    // Registry value name
    const char* regValue = nullptr;
    const char* fullPath = nullptr;
    const char* openFileCmd = nullptr;
};

bool HasBeenInstalled();
bool IsRunningInPortableMode();
bool IsDllBuild();

void SetAppDataDir(const char* path);
TempStr GetAppDataDirTemp();
TempStr GetPathInAppDataDirTemp(const char* fileName);

void DetectTextEditors(Vec<TextEditor*>&);
char* BuildOpenFileCmd(const char* pattern, const char* path, int line, int col);

bool ExtendedEditWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void EnsureAreaVisibility(Rect& rect);
Rect GetDefaultWindowPos();
void SaveCallstackLogs();

TempStr FormatFileSizeTransTemp(i64);

bool LaunchFileIfExists(const char* path);

bool AdjustVariableDriveLetter(char* path);

bool IsUntrustedFile(const char* filePath, const char* fileUrl = nullptr);
void DrawCloseButton(HDC hdc, Rect& r, bool isHover);

bool IsSumatraSigned();
