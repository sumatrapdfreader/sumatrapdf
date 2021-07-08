/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
#define FILE_HISTORY_MAX_RECENT 10

// maximum number of most frequently used files that will be shown on the
// Frequent Read list (space permitting)
#define FILE_HISTORY_MAX_FREQUENT 10

struct FileHistory {
    // owned by gGlobalPrefs->fileStates
    Vec<FileState*>* states = nullptr;

    FileHistory() = default;
    ~FileHistory() = default;

    void Clear(bool keepFavorites);
    void Append(FileState* state);
    void Remove(FileState* state);
    FileState* Get(size_t index) const;
    FileState* Find(const WCHAR* filePath, size_t* idxOut) const;
    FileState* MarkFileLoaded(const WCHAR* filePath);
    bool MarkFileInexistent(const WCHAR* filePath, bool hide = false);
    void GetFrequencyOrder(Vec<FileState*>& list) const;
    void Purge(bool alwaysUseDefaultState = false);
    void UpdateStatesSource(Vec<FileState*>* states);
};
