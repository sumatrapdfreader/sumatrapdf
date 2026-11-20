/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- a link dropped on the canvas: shared by orig's Canvas.cpp and ng's gui/DragDrop.cpp ---

TempStr FileNameFromUrlTemp(Str url);

struct DownloadAndOpenUrlData {
    Str url;
};
void DownloadAndOpenUrl(DownloadAndOpenUrlData* data);

// implemented by each app
void OpenDownloadedPath(Str* path);
