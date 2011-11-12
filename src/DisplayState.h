/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DisplayState_h
#define DisplayState_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include "BaseEngine.h"

enum DisplayMode {
    DM_FIRST = 0,
    // automatic means: the continuous form of single page, facing or
    // book view - depending on the document's desired PageLayout
    DM_AUTOMATIC = DM_FIRST,
    DM_SINGLE_PAGE,
    DM_FACING,
    DM_BOOK_VIEW,
    DM_CONTINUOUS,
    DM_CONTINUOUS_FACING,
    DM_CONTINUOUS_BOOK_VIEW
};

#define ZOOM_FIT_PAGE       -1.f
#define ZOOM_FIT_WIDTH      -2.f
#define ZOOM_FIT_CONTENT    -3.f
#define ZOOM_ACTUAL_SIZE    100.0f
#define ZOOM_MAX            6400.1f /* max zoom in % */
#define ZOOM_MIN            8.0f    /* min zoom in % */
#define INVALID_ZOOM        -99.0f

class DisplayState {
public:
    DisplayState() :
        filePath(NULL), useGlobalValues(false), index(0), openCount(0),
        displayMode(DM_AUTOMATIC), pageNo(1), zoomVirtual(100.0),
        rotation(0), windowState(0), thumbnail(NULL), isPinned(false),
        decryptionKey(NULL), tocVisible(true),
        sidebarDx(0), tocState(NULL) { }

    ~DisplayState() {
        free(filePath);
        free(decryptionKey);
        delete tocState;
        delete thumbnail;
    }

    TCHAR *             filePath;

    // in order to prevent documents that haven't been opened
    // for a while but used to be opened very frequently
    // constantly remain in top positions, the openCount
    // will be cut in half after every week, so that the
    // Frequently Read list hopefully better reflects the
    // currently relevant documents
    int                 openCount;
    size_t              index;     // temporary value needed for FileHistory::cmpOpenCount
    RenderedBitmap *    thumbnail; // persisted separately
    // a user can "pin" a preferred document to the Frequently Read list
    // so that the document isn't replaced by more frequently used ones
    bool                isPinned;

    bool                useGlobalValues;

    enum DisplayMode    displayMode;
    PointI              scrollPos;
    int                 pageNo;
    float               zoomVirtual;
    int                 rotation;
    int                 windowState;
    RectI               windowPos;

    // hex encoded MD5 fingerprint of file content (32 chars) 
    // followed by crypt key (64 chars) - only applies for PDF documents
    char *              decryptionKey;

    bool                tocVisible;
    int                 sidebarDx;

    // tocState is an array of ids for ToC items that have been
    // toggled by the user (i.e. aren't in their default expansion state)
    // Note: We intentionally track toggle state as opposed to expansion state
    //       so that we only have to save a diff instead of all states for the whole
    //       tree (which can be quite large) - and also due to backwards compatibility
    Vec<int> *          tocState;
};

#endif
