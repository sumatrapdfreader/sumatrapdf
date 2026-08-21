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
    Str binaryFilename;
    // Parameters to be passed to the editor;
    // use placeholder '%f' for path to source file and '%l' for line number.
    Str inverseSearchArgs;
    // Type of the path information obtained from the registry
    RegType type;
    // Registry key path
    Str regKey;
    // Registry value name
    Str regValue;
    Str fullPath;
    Str openFileCmd;
};

bool IsRunningInPortableMode();
bool IsDllBuild();
bool IsInstallerOrUninstallerExe();

void DeleteAppTools();

void SetAppDataDir(Str dir);
TempStr GetAppDataDirTemp();
TempStr GetPathInAppDataDirTemp(Str fileName);

void DetectTextEditors(Vec<TextEditor*>&);
void CollectInverseSearchCommands(StrVec& out, Str cmdLine);

void EnsureAreaVisibility(Rect& rect);
Rect GetDefaultWindowPos();
void SaveCallstackLogs();

Str Sha1OfAppExe();
TempStr GetWebViewDataDirTemp();

TempStr FormatFileSizeShortTransTemp(i64);
TempStr FormatFileSizeTransTemp(i64);

bool LaunchFileIfExists(Str path);

bool AdjustVariableDriveLetter(Str& path);

bool IsUntrustedFile(Str filePath, Str fileUrl = {});
