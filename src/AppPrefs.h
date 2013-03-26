/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

bool ParseViewMode(DisplayMode *mode, const WCHAR *txt);

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

/* Most of the global settings that we persist in preferences file. */
class SerializableGlobalPrefs {
public:
    bool globalPrefsOnly;
    /* pointer to a static string that is part of LangDef, don't free */
    const char *currLangCode;

    bool toolbarVisible;
    bool favVisible;

    /* If false, we won't ask the user if he wants Sumatra to handle PDF files */
    bool pdfAssociateDontAskAgain;
    /* If pdfAssociateDontAskAgain is TRUE, says whether we should
       silently associate or not */
    bool pdfAssociateShouldAssociate;

    bool enableAutoUpdate;

    /* if true, we remember which files we opened and their settings */
    bool rememberOpenedFiles;

    /* used for the Start page, About page and Properties dialog
       (negative values indicate that the default color will be used) */
    int  bgColor;
    bool escToExit;
    /* whether to display documents black-on-white or in system colors */
    bool useSysColors;

    /* pattern used to launch the editor when doing inverse search */
    ScopedMem<WCHAR> inverseSearchCmdLine;
    /* whether to expose the SyncTeX enhancements to the user */
    bool enableTeXEnhancements;

    /* When we show 'new version available', user has an option to check
       'skip this version'. This remembers which version is to be skipped.
       If NULL - don't skip */
    ScopedMem<WCHAR> versionToSkip;

    FILETIME lastUpdateTime;

    DisplayMode defaultDisplayMode;
    float defaultZoom;
    /* Default state of Sumatra window */
    int   windowState;
    /* Default position (can be on any monitor) */
    RectI windowPos;

    bool tocVisible;
    // if sidebar (favorites and/or bookmarks) is visible, this is
    // the width of the left sidebar panel containing them
    int  sidebarDx;
    // if both favorites and bookmarks parts of sidebar are
    // visible, this is the height of bookmarks (table of contents) part
    int  tocDy;

    /* Forward search highlighting settings  */
    struct {
        int  offset;    /* if <=0 then use the standard (inline) highlighting style, otherwise use the
                           margin highlight (i.e. coloured block on the left side of the page) */
        int  color;     /* highlight color of the forward-search for both the standard and margin style */
        int  width;     /* width of the coloured blocks for the margin style */
        bool permanent; /* if false then highlights are hidden automatically after a short period of time,
                           if true then highlights remain until the next forward search */
    } fwdSearch;

    bool showStartPage; /* whether to display Frequently Read documents or the About page in an empty window */
    int  openCountWeek; /* week count since 2011-01-01 needed to "age" openCount values in file history */

    FILETIME lastPrefUpdate; /* modification time of the preferences file when it was last read */

    bool cbxR2L; /* display CBX double pages from right to left */

    ScopedMem<char> prevSerialization; /* serialization of what was loaded (needed to prevent discarding unknown options) */
};

// All values in this structure are read from SumatraPDF-user.ini and
// can't be changed from within the UI
class AdvancedSettings {
public:

    /* ***** fields for section AdvancedOptions ***** */

    // whether the UI used for PDF documents will be used for ebooks as
    // well (enables printing and searching, disables automatic reflow)
    bool traditionalEbookUI;
    // whether opening a new document should happen in an already running
    // SumatraPDF instance so that there's only one process and documents
    // aren't opend twice
    bool reuseInstance;
    // background color of the non-document windows, traditionally yellow
    COLORREF mainWindowBackground;
    // whether the Esc key will exit SumatraPDF same as 'q'
    bool escToExit;
    // color value with which black (text) will be substituted
    COLORREF textColor;
    // color value with which white (background) will be substituted
    COLORREF pageColor;

    /* ***** fields for section PrinterDefaults ***** */

    // default value for scaling (shrink, fit, none or NULL)
    ScopedMem<WCHAR> printScale;
    // default value for the compatibility option
    bool printAsImage;

    /* ***** fields for section PagePadding ***** */

    // size of the left/right margin between window and document
    int outerX;
    // size of the top/bottom margin between window and document
    int outerY;
    // size of the horizontal margin between two pages
    int innerX;
    // size of the vertical margin between two pages
    int innerY;

    /* ***** fields for section BackgroundGradient ***** */

    // whether to draw a gradient behind the pages
    bool enabled; // TODO: issue with combining structs
    // color at the top of the document (first page)
    COLORREF colorTop;
    // color at the center of the document (middlest page)
    COLORREF colorMiddle;
    // color at the bottom of the document (last page)
    COLORREF colorBottom;

    /* ***** fields for section ForwardSearch ***** */

    // when set to a positive value, the forward search highlight style
    // will be changed to a rectangle at the left of the page (with the
    // indicated amount of margin from the page margin)
    int highlightOffset;
    // the width of the highlight rectangle for when HighlightOffset is set
    int highlightWidth;
    // the color used for the forward search highlight
    COLORREF highlightColor;
    // whether the forward search highlight will remain visible until the
    // next mouse click instead of fading away instantly
    bool highlightPermanent;

    /* ***** fields for array section ExternalViewer ***** */

    // command line with which to call the external viewer, may contain %p
    // for page numer and %1 for the file name
    WStrVec vecCommandLine;
    // name of the external viewer to be shown in the menu (implied by
    // CommandLine if missing)
    WStrVec vecName;
    // filter for which file types the menu item is to be shown (e.g.
    // "*.pdf;*.xps"; "*" if missing)
    WStrVec vecFilter;

    AdvancedSettings() : traditionalEbookUI(false), reuseInstance(false), mainWindowBackground(0xfff200),
        escToExit(false), textColor(0x000000), pageColor(0xffffff),
        printAsImage(false), outerX(4), outerY(2),
        innerX(4), innerY(4), enabled(false),
        colorTop(0xaa2828), colorMiddle(0x28aa28), colorBottom(0x2828aa),
        highlightOffset(0), highlightWidth(15), highlightColor(0x6581ff),
        highlightPermanent(false) {
    }
};

extern SerializableGlobalPrefs gGlobalPrefs;
extern AdvancedSettings gUserPrefs;

namespace DisplayModeConv {

const WCHAR *   NameFromEnum(DisplayMode var);
bool            EnumFromName(const WCHAR *txt, DisplayMode *resOut);

}

bool LoadPrefs();
bool SavePrefs();
bool ReloadPrefs();

#endif
