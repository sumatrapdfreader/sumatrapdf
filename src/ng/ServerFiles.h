/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void ServerFilesEnsureListed();
void ServerFilesReset();
bool ServerFilesListed();
bool IsFileOnServer(Str path);
void UploadFileToServer(MainWindow* win, Str path);
void DeleteFileFromServer(MainWindow* win, Str path);
