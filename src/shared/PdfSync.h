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

    // Inverse search: 1-based PDF page and point to source filename, line and column.
    virtual int DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) = 0;

    // Forward search: source position to PDF page and highlight rectangles.
    virtual int SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) = 0;

    // Set on sync-file changes; cleared after a successful rebuild.
    bool needsToRebuildIndex = true;
    // modification time (as FILETIME converted to a number) of sync file when index was last built
    i64 syncfileTimestamp = 0;

    bool NeedsToRebuildIndex();
    int MarkIndexWasRebuilt();
    i64 SyncFileTimestamp() const;
    Str PrependDir(Str filename) const;
    TempStr PrependDirTemp(Str filename) const;

    Str syncFilePath; // path to the synchronization file
    Str pdfPath;

    static int Create(Str pdffilename, EngineBase* engine, Synchronizer** sync);
};

// Deletes the temp files forward search unpacked the .synctex data into.
// Call once, on exit: a synctex scanner reads its file for as long as it lives.
void DeleteSyncTempFiles();
