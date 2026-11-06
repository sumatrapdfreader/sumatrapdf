/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
constexpr int kFileHistoryMaxRecent = 10;

// the file history list, owned by gSettings->fileStates (can be null)
Vec<FileState*>* FileHistoryStates();
void FileHistorySetStates(Vec<FileState*>* states);

void FileHistoryClear(bool keepFavorites);
void FileHistoryAppend(FileState* fs);
void FileHistoryRemove(FileState* fs);
FileState* FileHistoryGet(int index);
FileState* FileHistoryFindByPath(Str filePath);
FileState* FileHistoryMarkFileLoaded(Str filePath);
void FileHistoryDemote(Str filePath, bool hide = false);
void FileHistoryGetFrequencyOrder(Vec<FileState*>& list);
void FileHistoryGetRecentlyOpenedOrder(Vec<FileState*>& list);
void FileHistoryPurge(bool alwaysUseDefaultState = false);

// what Reopen Last Closed brings back: one closed tab, or a closed window's tabs
struct ClosedDocuments {
    StrVec paths;
    // the window's selected tab, an index into paths
    int activeIdx = 0;
    bool isWindow = false;
};

int RecentlyCloseDocumentsCount();
void RememberRecentlyClosedDocument(Str path);
void RememberRecentlyClosedWindow(const StrVec& paths, int activeIdx);
ClosedDocuments* PopRecentlyClosed();
void RemoveNonExistentFilesAsync();
bool DocumentPathExists(Str path);
void CleanUpThumbnailCache();
