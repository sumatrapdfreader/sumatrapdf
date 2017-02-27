/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on WindowInfo, so that a TabInfo could
   be moved between windows once this is supported) */
class TabInfo {
  public:
    ScopedMem<WCHAR> filePath;
    Controller* ctrl = nullptr;
    // text of win->hwndFrame when the tab is selected
    ScopedMem<WCHAR> frameTitle;
    // state of the table of contents
    bool showToc = false;
    bool showTocPresentation = false;
    DocTocItem* tocRoot = nullptr;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    // canvas dimensions when the document was last visible
    RectI canvasRc;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus = 0;
    // FileWatcher token for unsubscribing
    WatchedFile* watcher = nullptr;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage = nullptr;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual = INVALID_ZOOM;
    DisplayMode prevDisplayMode = DM_AUTOMATIC;

    TabInfo(const WCHAR* filePath = nullptr);
    ~TabInfo();

    DisplayModel* AsFixed() const { return ctrl ? ctrl->AsFixed() : nullptr; }
    ChmModel* AsChm() const { return ctrl ? ctrl->AsChm() : nullptr; }
    EbookController* AsEbook() const { return ctrl ? ctrl->AsEbook() : nullptr; }
    // returns Engine_None if !AsFixed()
    EngineType GetEngineType() const;

    const WCHAR* GetTabTitle() const;
};

class LinkSaver : public LinkSaverUI {
    TabInfo* tab;
    HWND parentHwnd;
    const WCHAR* fileName;

  public:
    LinkSaver(TabInfo* tab, HWND parentHwnd, const WCHAR* fileName)
        : tab(tab), parentHwnd(parentHwnd), fileName(fileName) {}

    virtual bool SaveEmbedded(const unsigned char* data, size_t cbCount);
};
