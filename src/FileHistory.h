/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
#define FILE_HISTORY_MAX_RECENT 10

// maximum number of most frequently used files that will be shown on the
// Frequent Read list (space permitting)
#define FILE_HISTORY_MAX_FREQUENT 10

class FileHistory {
    // owned by gGlobalPrefs->fileStates
    Vec<DisplayState*>* states;

  public:
    FileHistory() : states(nullptr) {
    }
    ~FileHistory() {
    }

    void Clear(bool keepFavorites);
    void Append(DisplayState* state) {
        states->Append(state);
    }
    void Remove(DisplayState* state) {
        states->Remove(state);
    }
    DisplayState* Get(size_t index) const;
    DisplayState* Find(const WCHAR* filePath, size_t* idxOut) const;
    DisplayState* MarkFileLoaded(const WCHAR* filePath);
    bool MarkFileInexistent(const WCHAR* filePath, bool hide = false);
    void GetFrequencyOrder(Vec<DisplayState*>& list) const;
    void Purge(bool alwaysUseDefaultState = false);
    void UpdateStatesSource(Vec<DisplayState*>* states) {
        this->states = states;
    }
};
