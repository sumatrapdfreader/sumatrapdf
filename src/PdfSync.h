/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
// PDF-source synchronizer based on .pdfsync file

#ifndef PdfSync_h
#define PdfSync_h

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "GeomUtil.h"
#include "Vec.h"
#include <time.h>

// Error codes returned by the synchronization functions
enum {  PDFSYNCERR_SUCCESS,                   // the synchronization succeeded
        PDFSYNCERR_SYNCFILE_NOTFOUND,         // no sync file found
        PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED, // sync file cannot be opened
        PDFSYNCERR_INVALID_PAGE_NUMBER,       // the given page number does not exist in the sync file
        PDFSYNCERR_NO_SYNC_AT_LOCATION,       // no synchronization found at this location
        PDFSYNCERR_UNKNOWN_SOURCEFILE,        // the source file is not present in the sync file
        PDFSYNCERR_NORECORD_IN_SOURCEFILE,    // there is not any record declaration for that particular source file
        PDFSYNCERR_NORECORD_FOR_THATLINE,     // no record found for the requested line
        PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD,// a record is found for the given source line but there is not point in the PDF that corresponds to it
        PDFSYNCERR_OUTOFMEMORY,
        PDFSYNCERR_INVALID_ARGUMENT
};

// System of point coordinates
enum CoordSystem {
    TopLeft,    // origin at the top-left corner
    BottomLeft, // origin at the bottom-left corner
};

class Synchronizer
{
public:
    Synchronizer(const TCHAR* _syncfilepath, CoordSystem coordsys=BottomLeft) :
        index_discarded(true), coordsys(coordsys),
        syncfilepath(str::Dup(_syncfilepath)), dir(path::GetDir(_syncfilepath)) {
        _tstat(_syncfilepath, &syncfileTimestamp);
    }
    virtual ~Synchronizer() {
        free(dir);
        free(syncfilepath);
    }

    // conversion from one coordinate system to another
    void convert_coord_to_internal(int *x, int *y, int pageHeight, CoordSystem src)
    {
        if (src != this->coordsys)
            *y = pageHeight - *y;
    }
    void convert_coord_from_internal(RectI *rc, int pageHeight, CoordSystem dst)
    {
        if (dst != this->coordsys)
            rc->y = pageHeight - (rc->y + rc->dy);
    }
    
    // Inverse-search:
    //  - sheet: page number in the PDF (starting from 1)
    //  - x,y: user-specified PDF-coordinates. They must be given in the system used internally by the synchronizer.
    //  - maxy: contains the height of the page. this is necessary to convert into the coordinate-system used internally by the syncrhonizer.
    //    For an A4 paper it is approximately equal to maxy=842.
    //  - cchFilename: size of the buffer 'filename'
    // The result is returned in filename, line, col
    //  - filename: receives the name of the source file
    //  - line: receives the line number
    //  - col: receives the column number
    virtual UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col) = 0;
    
    // Forward-search:
    // The result is returned in (page,x,y). The coordinates x,y are specified in the internal 
    // coordinate system.
    virtual UINT source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI>& rects) = 0;

    void discard_index() { index_discarded = true; }
    bool is_index_discarded() const
    {
        // was the index manually discarded?
        if (index_discarded)
            return true;

        // has the synchronization file been changed on disk?
        struct _stat newstamp;
        if (_tstat(syncfilepath, &newstamp) == 0 &&
            difftime(newstamp.st_mtime, syncfileTimestamp.st_mtime) > 0) {
            DBG_OUT("PdfSync:sync file has changed, rebuilding index: %s\n", syncfilepath);
            // update time stamp
            memcpy((void *)&syncfileTimestamp, &newstamp, sizeof(syncfileTimestamp));
            return true; // the file has changed!
        }

        return false;
    }

    int rebuild_index()
    {
        index_discarded = false;
        // save sync file timestamp
        _tstat(syncfilepath, &syncfileTimestamp);
        return PDFSYNCERR_SUCCESS;
    }

    // the caller must free() the command line
    TCHAR * prepare_commandline(const TCHAR* pattern, const TCHAR* filename, UINT line, UINT col);

private:
    bool index_discarded; // true if the index needs to be recomputed (needs to be set to true when a change to the pdfsync file is detected)
    struct _stat syncfileTimestamp; // time stamp of sync file when index was last built

protected:
    TCHAR * syncfilepath; // path to the synchronization file
    CoordSystem coordsys; // system used internally by the syncfile for the PDF coordinates
    TCHAR * dir;          // directory where the syncfile lies

public:
    static int Create(const TCHAR *pdffilename, Synchronizer **sync);
};


#define PDFSYNC_DDE_SERVICE   _T("SUMATRA")
#define PDFSYNC_DDE_TOPIC     _T("control")

// forward-search command
//  format: [ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
//    if pdffilepath is provided, the file will be opened if no open window can be found for it
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]
#define DDECOMMAND_SYNC       _T("ForwardSearch")

// open file command
//  format: [Open("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [Open("c:\file.pdf", 1, 1, 0)]
#define DDECOMMAND_OPEN       _T("Open")

// jump to named destination command
//  format: [GoToNamedDest("<pdffilepath>","<destination name>")]
//  eg: [GoToNamedDest("c:\file.pdf", "chapter.1")]. pdf file must be already opened
#define DDECOMMAND_GOTO       _T("GotoNamedDest")

// jump to page command
//  format: [GoToPage("<pdffilepath>",<page number>)]
//  eg: [GoToPage("c:\file.pdf", 37)]. pdf file must be already opened
#define DDECOMMAND_PAGE       _T("GotoPage")

// set view mode and zoom level
//  format: [SetView("<pdffilepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]
//  eg: [SetView("c:\file.pdf", "book view", -2)]
//  note: use -1 for ZOOM_FIT_PAGE, -2 for ZOOM_FIT_WIDTH and -3 for ZOOM_FIT_CONTENT
#define DDECOMMAND_SETVIEW    _T("SetView")

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam);

#endif
