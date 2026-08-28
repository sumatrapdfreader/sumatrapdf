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
bool FileHistoryMarkFileInexistent(Str filePath, bool hide = false);
void FileHistoryGetFrequencyOrder(Vec<FileState*>& list);
void FileHistoryGetRecentlyOpenedOrder(Vec<FileState*>& list);
void FileHistoryPurge(bool alwaysUseDefaultState = false);

int RecentlyCloseDocumentsCount();
void RememberRecentlyClosedDocument(Str path);
Str PopRecentlyClosedDocument();
void RemoveNonExistentFilesAsync();
bool DocumentPathExists(Str path);
void CleanUpThumbnailCache();
