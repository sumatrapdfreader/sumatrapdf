/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
class WindowInfo;

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on WindowInfo, so that a TabInfo could
   be moved between windows once this is supported) */
class TabInfo
{
public:
    ScopedMem<WCHAR> filePath;
    Controller *ctrl;
    const WCHAR *tabTitle;
    // text of win->hwndFrame when the tab is selected
    ScopedMem<WCHAR> frameTitle;
    // state of the table of contents
    bool showToc, showTocPresentation;
    DocTocItem *tocRoot;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    // canvas dimensions when the document was last visible
    RectI canvasRc;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus;
    // FileWatcher token for unsubscribing
    WatchedFile *watcher;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage> *selectionOnPage;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual;
    DisplayMode prevDisplayMode;

    TabInfo();
    ~TabInfo();

    DisplayModel *AsFixed() const { return ctrl ? ctrl->AsFixed() : nullptr; }
    ChmModel *AsChm() const { return ctrl ? ctrl->AsChm() : nullptr; }
    EbookController *AsEbook() const { return ctrl ? ctrl->AsEbook() : nullptr; }
    EngineType GetEngineType() const;
};

class LinkSaver : public LinkSaverUI {
    WindowInfo *owner;
    const WCHAR *fileName;

public:
    LinkSaver(WindowInfo *win, const WCHAR *fileName) : owner(win), fileName(fileName) { }

    virtual bool SaveEmbedded(const unsigned char *data, size_t cbCount);
};
