/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
// PDF-source synchronizer based on .pdfsync file

// Error codes returned by the synchronization functions
enum {
    PDFSYNCERR_SUCCESS,                    // the synchronization succeeded
    PDFSYNCERR_SYNCFILE_NOTFOUND,          // no sync file found
    PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED,  // sync file cannot be opened
    PDFSYNCERR_INVALID_PAGE_NUMBER,        // the given page number does not exist in the sync file
    PDFSYNCERR_NO_SYNC_AT_LOCATION,        // no synchronization found at this location
    PDFSYNCERR_UNKNOWN_SOURCEFILE,         // the source file is not present in the sync file
    PDFSYNCERR_NORECORD_IN_SOURCEFILE,     // there is not any record declaration for that particular source file
    PDFSYNCERR_NORECORD_FOR_THATLINE,      // no record found for the requested line
    PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD, // a record is found for the given source line but there is not point in the
                                           // PDF that corresponds to it
    PDFSYNCERR_OUTOFMEMORY,
    PDFSYNCERR_INVALID_ARGUMENT
};

class EngineBase;

struct Synchronizer {
    explicit Synchronizer(Str syncfilepath, Str pdffilename);
    virtual ~Synchronizer();

    // Inverse-search:
    //  - pageNo: page number in the PDF (starting from 1)
    //  - pt: user-specified PDF-coordinates.
    // The result is returned in filename, line, col
    //  - filename: receives the name of the source file
    //  - line: receives the line number
    //  - col: receives the column number
    virtual int DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) = 0;

    // Forward-search:
    // The result is returned in page and rects (list of rectangles to highlight).
    virtual int SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) = 0;

    // true if the index needs to be recomputed (needs to be set to true when a change to the
    // pdfsync file is detected)
    bool needsToRebuildIndex = true;
    // modification time (as FILETIME converted to a number) of sync file when index was last built
    i64 syncfileTimestamp = 0;

    bool NeedsToRebuildIndex();
    int MarkIndexWasRebuilt();
    Str PrependDir(Str filename) const;
    TempStr PrependDirTemp(Str filename) const;

    Str syncFilePath; // path to the synchronization file
    Str pdfPath;

    static int Create(Str pdffilename, EngineBase* engine, Synchronizer** sync);
};
