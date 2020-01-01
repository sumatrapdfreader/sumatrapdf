/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

class Synchronizer {
  public:
    explicit Synchronizer(const WCHAR* syncfilepath);
    virtual ~Synchronizer() {
    }

    // Inverse-search:
    //  - pageNo: page number in the PDF (starting from 1)
    //  - pt: user-specified PDF-coordinates.
    // The result is returned in filename, line, col
    //  - filename: receives the name of the source file
    //  - line: receives the line number
    //  - col: receives the column number
    virtual int DocToSource(UINT pageNo, PointI pt, AutoFreeWstr& filename, UINT* line, UINT* col) = 0;

    // Forward-search:
    // The result is returned in page and rects (list of rectangles to highlight).
    virtual int SourceToDoc(const WCHAR* srcfilename, UINT line, UINT col, UINT* page, Vec<RectI>& rects) = 0;

    // the caller must free() the command line
    WCHAR* PrepareCommandline(const WCHAR* pattern, const WCHAR* filename, UINT line, UINT col);

  private:
    bool indexDiscarded; // true if the index needs to be recomputed (needs to be set to true when a change to the
                         // pdfsync file is detected)
    struct _stat syncfileTimestamp; // time stamp of sync file when index was last built

  protected:
    bool IsIndexDiscarded() const;
    int RebuildIndex();
    WCHAR* PrependDir(const WCHAR* filename) const;

    AutoFreeWstr syncfilepath; // path to the synchronization file

  public:
    static int Create(const WCHAR* pdffilename, EngineBase* engine, Synchronizer** sync);
};
