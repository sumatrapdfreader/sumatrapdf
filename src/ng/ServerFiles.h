/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FileState;

void ServerFilesEnsureListed();
void ServerFilesReset();
bool ServerFilesListed();
// a server file with the same name: same size, or a different size (edited
// locally or elsewhere)
enum class ServerFileState {
    None,
    Same,
    Differs,
};

ServerFileState GetServerFileState(Str path, i64 localSize);
void ServerOnlyFileStates(Vec<FileState*>& out);
bool IsServerOnlyFile(Str path);
i64 ServerFileSize(Str path);
void OpenServerFile(MainWindow* win, Str path);
void UploadFileToServer(MainWindow* win, Str path);
void DeleteFileFromServer(MainWindow* win, Str path);
