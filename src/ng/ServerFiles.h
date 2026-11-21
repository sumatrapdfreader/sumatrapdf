/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

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
void UploadFileToServer(MainWindow* win, Str path);
void DeleteFileFromServer(MainWindow* win, Str path);
