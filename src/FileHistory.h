/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
#define kFileHistoryMaxRecent 10

struct FileHistory {
    // owned by gGlobalPrefs->fileStates
    Vec<FileState*>* states = nullptr;

    FileHistory() = default;
    ~FileHistory() = default;

    void Clear(bool keepFavorites) const;
    void Append(FileState* state) const;
    void Remove(FileState* state) const;
    FileState* Get(int index) const;
    FileState* FindByPath(Str filePath) const;
    FileState* FindByName(Str filePath, int* idxOut) const;
    FileState* MarkFileLoaded(Str filePath) const;
    bool MarkFileInexistent(Str filePath, bool hide = false) const;
    void GetFrequencyOrder(Vec<FileState*>& list) const;
    void GetRecentlyOpenedOrder(Vec<FileState*>& list) const;
    void Purge(bool alwaysUseDefaultState = false) const;
    void UpdateStatesSource(Vec<FileState*>* states);
};

extern FileHistory gFileHistory;

int RecentlyCloseDocumentsCount();
void RememberRecentlyClosedDocument(Str path);
Str PopRecentlyClosedDocument();
void RemoveNonExistentFilesAsync();
bool DocumentPathExists(Str path);
void CleanUpThumbnailCache();
