/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FileState;

void ServerFilesEnsureListed();
void ServerFilesReset();
void SetServerRelistMsForTesting(int ms);
bool ServerFilesListed();
// a server file with the same content (or, until it's hashed, the same name
// and size), or one with the same name and different content
enum class ServerFileState {
    None,
    Same,
    Differs,
};

ServerFileState GetServerFileState(Str path, i64 localSize);
TempStr ServerNameOfTemp(Str path);
TempStr ServerNameNoteTemp(Str path);
void ServerOnlyFileStates(Vec<FileState*>& out);
bool IsServerOnlyFile(Str path);
i64 ServerFileSize(Str path);
void OpenServerFile(MainWindow* win, Str path);
void UploadFileToServer(MainWindow* win, Str path);
void DeleteFileFromServer(MainWindow* win, Str path);
enum class ServerFileAction {
    Open,
    Upload,
    Delete
};
void RunServerFilesBatch(MainWindow* win, const StrVec& paths, ServerFileAction action);

// implemented by each app
void ServerFilesChanged();
void OpenDownloadedServerFile(MainWindow* win, Str path);
void AskServerYesNo(MainWindow* win, Str question, Str caption, Func1<bool> onAnswer);
