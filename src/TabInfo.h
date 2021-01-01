/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
struct VbkmFile;
struct EditAnnotationsWindow;
struct WindowInfo;

enum class TocSort { None, TagSmallFirst, TagBigFirst, Color };

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on WindowInfo, so that a TabInfo could
   be moved between windows once this is supported) */
struct TabInfo {
    AutoFreeWstr filePath;
    WindowInfo* win = nullptr;
    Controller* ctrl = nullptr;
    // text of win->hwndFrame when the tab is selected
    AutoFreeWstr frameTitle;
    // state of the table of contents
    bool showToc = false;
    bool showTocPresentation = false;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    // canvas dimensions when the document was last visible
    Rect canvasRc;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus = 0;
    // FileWatcher token for unsubscribing
    WatchedFile* watcher = nullptr;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage = nullptr;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual = INVALID_ZOOM;
    DisplayMode prevDisplayMode{DisplayMode::Automatic};
    Vec<VbkmFile*> altBookmarks;
    TocSort tocSort = TocSort::None;
    TocTree* currToc = nullptr; // not owned by us
    // if sortTag is != SortTag::None, this is a sorted toc tree to be displayed
    TocTree* tocSorted = nullptr;
    EditAnnotationsWindow* editAnnotsWindow = nullptr;

    TabInfo(WindowInfo* win, const WCHAR* filePath = nullptr);
    ~TabInfo();

    DisplayModel* AsFixed() const;

    // only if AsFixed()
    EngineBase* GetEngine() const;
    Kind GetEngineType() const;

    ChmModel* AsChm() const;
    EbookController* AsEbook() const;

    const WCHAR* GetTabTitle() const;
    bool IsDocLoaded() const;
    void MoveDocBy(int dx, int dy);
    void ToggleZoom();
};

bool SaveDataToFile(HWND hwndParent, WCHAR* fileName, std::span<u8> data);
