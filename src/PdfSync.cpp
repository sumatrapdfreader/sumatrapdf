/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <synctex_parser.h>
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "utils/ZipUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "PdfSync.h"

#include "utils/Log.h"

// size of the mark highlighting the location calculated by forward-search
#define MARK_SIZE 10
// maximum error in the source file line number when doing forward-search
#define EPSILON_LINE 5
// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE 800
// Minimal vertical distance
#define PDFSYNC_EPSILON_Y 20

struct PdfsyncFileIndex {
    size_t start, end; // first and one-after-last index of lines associated with a file
};

struct PdfsyncLine {
    UINT record = 0; // index for mapping line(s) to point(s)
    size_t file = 0; // index into srcfiles
    UINT line = 0;
    UINT column = 0;
};

struct PdfsyncPoint {
    UINT record; // index for mapping point(s) to line(s)
    UINT page, x, y;
};

// Synchronizer based on .pdfsync file generated with the pdfsync tex package
class Pdfsync : public Synchronizer {
  public:
    Pdfsync(const char* syncfilename, EngineBase* engine) : Synchronizer(syncfilename), engine(engine) {
        CrashIf(!str::EndsWithI(syncfilename, ".pdfsync"));
    }

    int DocToSource(int pageNo, Point pt, AutoFreeStr& filename, int* line, int* col) override;
    int SourceToDoc(const char* srcfilename, int line, int col, int* page, Vec<Rect>& rects) override;

  private:
    int RebuildIndexIfNeeded();
    UINT SourceToRecord(const char* srcfilename, int line, int col, Vec<size_t>& records);

    EngineBase* engine;              // needed for converting between coordinate systems
    StrVec srcfiles;                 // source file names
    Vec<PdfsyncLine> lines;          // record-to-line mapping
    Vec<PdfsyncPoint> points;        // record-to-point mapping
    Vec<PdfsyncFileIndex> fileIndex; // start and end of entries for a file in <lines>
    Vec<size_t> sheetIndex;          // start of entries for a sheet in <points>
};

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer {
  public:
    SyncTex(const char* syncfilename, EngineBase* engineIn) : Synchronizer(syncfilename) {
        engine = engineIn;
        scanner = nullptr;
        CrashIf(!str::EndsWithI(syncfilename, ".synctex"));
    }

    ~SyncTex() override {
        synctex_scanner_free(scanner);
    }

    int DocToSource(int pageNo, Point pt, AutoFreeStr& filename, int* line, int* col) override;
    int SourceToDoc(const char* srcfilename, int line, int col, int* page, Vec<Rect>& rects) override;

  private:
    int RebuildIndexIfNeeded();

    EngineBase* engine; // needed for converting between coordinate systems
    synctex_scanner_t scanner;
};

Synchronizer::Synchronizer(const char* syncFilePathIn) {
    syncFilePath = str::Dup(syncFilePathIn);
    WCHAR* path = ToWStrTemp(syncFilePathIn);
    _wstat(path, &syncfileTimestamp);
}

bool Synchronizer::NeedsToRebuildIndex() const {
    // was the index manually discarded?
    if (needsToRebuildIndex) {
        return true;
    }

    // has the synchronization file been changed on disk?
    struct _stat newstamp;
    WCHAR* path = ToWStrTemp(syncFilePath);
    if (_wstat(path, &newstamp) == 0 && difftime(newstamp.st_mtime, syncfileTimestamp.st_mtime) > 0) {
        // update time stamp
        memcpy((void*)&syncfileTimestamp, &newstamp, sizeof(syncfileTimestamp));
        return true; // the file has changed!
    }

    return false;
}

int Synchronizer::MarkIndexWasRebuilt() {
    needsToRebuildIndex = false;
    WCHAR* path = ToWStrTemp(syncFilePath);
    _wstat(path, &syncfileTimestamp);
    return PDFSYNCERR_SUCCESS;
}

char* Synchronizer::PrependDir(const char* filename) const {
    char* dir = path::GetDirTemp(syncFilePath);
    return path::Join(dir, filename);
}

// Create a Synchronizer object for a PDF file.
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
int Synchronizer::Create(const char* path, EngineBase* engine, Synchronizer** sync) {
    if (!sync || !engine) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    if (!str::EndsWithI(path, ".pdf")) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    char* basePath = path::GetPathNoExtTemp(path);

    // Check if a PDFSYNC file is present
    char* syncFile = str::JoinTemp(basePath, ".pdfsync");
    if (file::Exists(syncFile)) {
        *sync = new Pdfsync(syncFile, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    // check if SYNCTEX or compressed SYNCTEX file is present
    char* texGzFile = str::JoinTemp(basePath, ".synctex.gz");
    char* texFile = str::JoinTemp(basePath, ".synctex");

    if (file::Exists(texGzFile) || file::Exists(texFile)) {
        // due to a bug with synctex_parser.c, this must always be
        // the path to the .synctex file (even if a .synctex.gz file is used instead)
        *sync = new SyncTex(texFile, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    return PDFSYNCERR_SYNCFILE_NOTFOUND;
}

// PDFSYNC synchronizer

// move to the next line in a list of zero-terminated lines
static char* Advance0Line(char* line, char* end) {
    line += str::Len(line);
    // skip all zeroes until the next non-empty line
    for (; line < end && !*line; line++) {
        ;
    }
    return line < end ? line : nullptr;
}

// see http://itexmac.sourceforge.net/pdfsync.html for the specification
int Pdfsync::RebuildIndexIfNeeded() {
    if (!NeedsToRebuildIndex()) {
        return PDFSYNCERR_SUCCESS;
    }

    ByteSlice data = file::ReadFile(syncFilePath);
    if (!data) {
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }

    char* line = (char*)data.Get();
    // convert the file data into a list of zero-terminated strings
    str::TransCharsInPlace(line, "\r\n", "\0\0");

    // parse preamble (jobname and version marker)
    char* dataEnd = line + data.size();

    // replace star by spaces (TeX uses stars instead of spaces in filenames)
    str::TransCharsInPlace(line, "*/", " \\");
    AutoFreeStr jobName = strconv::AnsiToUtf8(line);
    jobName.Set(str::Join(jobName, ".tex"));
    jobName.Set(PrependDir(jobName));

    line = Advance0Line(line, dataEnd);
    UINT versionNumber = 0;
    if (!line || !str::Parse(line, "version %u", &versionNumber) || versionNumber != 1) {
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }

    // reset synchronizer database
    srcfiles.Reset();
    lines.Reset();
    points.Reset();
    fileIndex.Reset();
    sheetIndex.Reset();

    Vec<size_t> filestack;
    int page = 1;
    sheetIndex.Append(0);

    // add the initial tex file to the source file stack
    filestack.Append(srcfiles.size());
    srcfiles.Append(jobName);
    PdfsyncFileIndex findex{};
    fileIndex.Append(findex);

    PdfsyncLine psline;
    PdfsyncPoint pspoint;

    // parse data
    int maxPageNo = engine->PageCount();
    while (true) {
        line = Advance0Line(line, dataEnd);
        if (!line) {
            break;
        }
        switch (*line) {
            case 'l':
                psline.file = filestack.Last();
                if (str::Parse(line, "l %u %u %u", &psline.record, &psline.line, &psline.column)) {
                    lines.Append(psline);
                } else if (str::Parse(line, "l %u %u", &psline.record, &psline.line)) {
                    psline.column = 0;
                    lines.Append(psline);
                }
                // else dbg("Bad 'l' line in the pdfsync file");
                break;

            case 's':
                if (str::Parse(line, "s %u", &page)) {
                    sheetIndex.Append(points.size());
                }
                // else dbg("Bad 's' line in the pdfsync file");
                // if (0 == page || page > maxPageNo)
                //     dbg("'s' line with invalid page number in the pdfsync file");
                break;

            case 'p':
                pspoint.page = page;
                if (0 == page || page > maxPageNo) {
                    /* ignore point for invalid page number */;
                } else if (str::Parse(line, "p %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y)) {
                    points.Append(pspoint);
                } else if (str::Parse(line, "p* %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y)) {
                    points.Append(pspoint);
                }
                // else dbg("Bad 'p' line in the pdfsync file");
                break;

            case '(': {
                AutoFreeStr filename(strconv::AnsiToUtf8(line + 1));
                // if the filename contains quotes then remove them
                // TODO: this should never happen!?
                if (filename[0] == '"' && filename[str::Len(filename) - 1] == '"') {
                    size_t n = str::Len(filename) - 2;
                    filename = str::Dup(filename + 1, n);
                }
                // undecorate the filepath: replace * by space and / by \ (backslash)
                str::TransCharsInPlace(filename, "*/", " \\");
                // if the file name extension is not specified then add the suffix '.tex'
                if (str::IsEmpty(path::GetExtTemp(filename))) {
                    filename = str::Join(filename, ".tex");
                }
                // ensure that the path is absolute
                if (!path::IsAbsolute(filename)) {
                    filename = PrependDir(filename);
                }

                filestack.Append(srcfiles.size());
                srcfiles.Append(filename);
                findex.start = findex.end = lines.size();
                fileIndex.Append(findex);
            } break;

            case ')':
                if (filestack.size() > 1) {
                    fileIndex.at(filestack.Pop()).end = lines.size();
                }
                // else dbg("Unbalanced ')' line in the pdfsync file");
                break;

            default:
                // dbg("Ignoring invalid pdfsync line starting with '%c'", *line);
                break;
        }
    }

    fileIndex.at(0).end = lines.size();
    ReportIf(filestack.size() != 1);

    return MarkIndexWasRebuilt();
}

// convert a coordinate from the sync file into a PDF coordinate
#define SYNC_TO_PDF_COORDINATE(c) (c / 65781.76)

static int cmpLineRecords(const void* a, const void* b) {
    return ((PdfsyncLine*)a)->record - ((PdfsyncLine*)b)->record;
}

int Pdfsync::DocToSource(int pageNo, Point pt, AutoFreeStr& filename, int* line, int* col) {
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        return res;
    }

    // find the entry in the index corresponding to this page
    int nPages = engine->PageCount();
    if (pageNo == 0 || pageNo >= sheetIndex.isize() || pageNo > nPages) {
        return PDFSYNCERR_INVALID_PAGE_NUMBER;
    }

    // PdfSync coordinates are y-inversed
    Rect mbox = engine->PageMediabox(pageNo).Round();
    pt.y = mbox.dy - pt.y;

    // distance to the closest pdf location (in the range <PDFSYNC_EPSILON_SQUARE)
    UINT closest_xydist = UINT_MAX;
    UINT selected_record = UINT_MAX;
    // If no record is found within a distance^2 of PDFSYNC_EPSILON_SQUARE
    // (selected_record == -1) then we pick up the record that is closest
    // vertically to the hit-point.
    UINT closest_ydist = UINT_MAX;        // vertical distance between the hit point and the vertically-closest record
    UINT closest_xdist = UINT_MAX;        // horizontal distance between the hit point and the vertically-closest record
    UINT closest_ydist_record = UINT_MAX; // vertically-closest record

    // read all the sections of 'p' declarations for this pdf sheet
    for (size_t i = sheetIndex.at((size_t)pageNo); i < points.size() && points.at(i).page == (uint)pageNo; i++) {
        // check whether it is closer than the closest point found so far
        UINT dx = abs(pt.x - (int)SYNC_TO_PDF_COORDINATE(points.at(i).x));
        UINT dy = abs(pt.y - (int)SYNC_TO_PDF_COORDINATE(points.at(i).y));
        UINT dist = dx * dx + dy * dy;
        if (dist < PDFSYNC_EPSILON_SQUARE && dist < closest_xydist) {
            selected_record = points.at(i).record;
            closest_xydist = dist;
        } else if ((closest_xydist == UINT_MAX) && dy < PDFSYNC_EPSILON_Y &&
                   (dy < closest_ydist || (dy == closest_ydist && dx < closest_xdist))) {
            closest_ydist_record = points.at(i).record;
            closest_ydist = dy;
            closest_xdist = dx;
        }
    }

    if (selected_record == UINT_MAX) {
        selected_record = closest_ydist_record;
    }
    if (selected_record == UINT_MAX) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point
    }

    // We have a record number, we need to find its declaration ('l ...') in the syncfile
    PdfsyncLine cmp;
    cmp.record = selected_record;
    PdfsyncLine* found =
        (PdfsyncLine*)bsearch(&cmp, lines.LendData(), lines.size(), sizeof(PdfsyncLine), cmpLineRecords);
    CrashIf(!found);
    if (!found) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    char* path = srcfiles[found->file];
    filename.SetCopy(path);
    *line = (int)found->line;
    *col = (int)found->column;
    if (*col < 0) {
        *col = 0;
    }

    return PDFSYNCERR_SUCCESS;
}

// Find a record corresponding to the given source file, line number and optionally column number.
// (at the moment the column parameter is ignored)
//
// If there are several *consecutively declared* records for the same line then they are all returned.
// The list of records is added to the vector 'records'
//
// If there is no record for that line, the record corresponding to the nearest line is selected
// (within a range of EPSILON_LINE)
//
// The function returns PDFSYNCERR_SUCCESS if a matching record was found.
UINT Pdfsync::SourceToRecord(const char* srcfilename, int line, int, Vec<size_t>& records) {
    if (!srcfilename) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    AutoFreeStr srcfilepath;
    // convert the source file to an absolute path
    if (!path::IsAbsolute(srcfilename)) {
        srcfilepath.Set(PrependDir(srcfilename));
    } else {
        srcfilepath.SetCopy(srcfilename);
    }
    if (!srcfilepath) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    // find the source file entry
    size_t isrc;
    for (isrc = 0; isrc < srcfiles.size(); isrc++) {
        char* path = srcfiles[isrc];
        if (path::IsSame(srcfilepath, path)) {
            break;
        }
    }
    if (isrc == srcfiles.size()) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }

    if (fileIndex.at(isrc).start == fileIndex.at(isrc).end) {
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file
    }

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = EPSILON_LINE; // distance to the closest record
    size_t lineIx = (size_t)-1;       // closest record-line index

    for (size_t isec = fileIndex.at(isrc).start; isec < fileIndex.at(isrc).end; isec++) {
        // does this section belong to the desired file?
        if (lines.at(isec).file != isrc) {
            continue;
        }

        UINT d = abs((int)lines.at(isec).line - (int)line);
        if (d < min_distance) {
            min_distance = d;
            lineIx = isec;
            if (0 == d) {
                break; // We have found a record for the requested line!
            }
        }
    }
    if (lineIx == (size_t)-1) {
        return PDFSYNCERR_NORECORD_FOR_THATLINE;
    }

    // we read all the consecutive records until we reach a record belonging to another line
    for (size_t i = lineIx; i < lines.size() && lines.at(i).line == lines.at(lineIx).line; i++) {
        records.Append(lines.at(i).record);
    }

    return PDFSYNCERR_SUCCESS;
}

int Pdfsync::SourceToDoc(const char* srcfilename, int line, int col, int* page, Vec<Rect>& rects) {
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        return res;
    }

    Vec<size_t> found_records;
    UINT ret = SourceToRecord(srcfilename, line, col, found_records);
    if (ret != PDFSYNCERR_SUCCESS || found_records.size() == 0) {
        return ret;
    }

    rects.Reset();

    // records have been found for the desired source position:
    // we now find the page and positions in the PDF corresponding to these found records
    int firstPage = UINT_MAX;
    for (PdfsyncPoint& p : points) {
        if (!found_records.Contains(p.record)) {
            continue;
        }
        if (firstPage != UINT_MAX && firstPage != (int)p.page) {
            continue;
        }
        firstPage = *page = (int)p.page;
        RectF rc(SYNC_TO_PDF_COORDINATE(p.x), SYNC_TO_PDF_COORDINATE(p.y), MARK_SIZE, MARK_SIZE);
        // PdfSync coordinates are y-inversed
        RectF mbox = engine->PageMediabox(firstPage);
        rc.y = mbox.dy - (rc.y + rc.dy);
        rects.Append(rc.Round());
    }

    if (rects.size() > 0) {
        return PDFSYNCERR_SUCCESS;
    }
    // the record does not correspond to any point in the PDF: this is possible...
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}

// returns path of ungzipped file
TempStr ungzipToFile(char* path) {
    // to see if we can read when gzopen in synctex_scanner_new_with_output_file cannot
    ByteSlice compr = file::ReadFile(path);
    if (compr.IsEmpty()) {
        logf("ungzip: file::ReadFile() '%s' failed\n", path);
        return nullptr;
    }
    logf("ungzip: file::ReadFile() did read '%s'\n", path);
    ByteSlice uncompr = Ungzip(compr);
    compr.Free();
    if (uncompr.IsEmpty()) {
        return nullptr;
    }
    TempStr destPath = str::JoinTemp(path, ".sum.synctex");
    bool ok = file::WriteFile(destPath, uncompr);
    uncompr.Free();
    if (!ok) {
        return nullptr;
    }
    return destPath;
}

// SYNCTEX synchronizer

int SyncTex::RebuildIndexIfNeeded() {
    if (!NeedsToRebuildIndex()) {
        logfa("SyncTex::RebuildIndexIfNeeded: no need to rebuild\n");
        return PDFSYNCERR_SUCCESS;
    }
    synctex_scanner_free(scanner);
    scanner = nullptr;
    i64 fsize;
    TempStr pathNoExt;
    TempStr pathSyncGz;
    bool didRepeat = false;

    TempStr syncPathTemp = str::DupTemp(syncFilePath.Get());
Repeat:
    WCHAR* ws = ToWStrTemp(syncPathTemp);
    AutoFreeStr pathAnsi = strconv::WstrToAnsi(ws);
    scanner = synctex_scanner_new_with_output_file(pathAnsi, nullptr, 1);
    if (scanner) {
        logfa("synctex_scanner_new_with_output_file: ok for pathAnsi '%s'\n", pathAnsi.Get());
        goto Exit;
    }
    if (!str::Eq(syncPathTemp, pathAnsi)) {
        logfa("synctex_scanner_new_with_output_file: retrying for syncFilePath '%s'\n", syncPathTemp);
        scanner = synctex_scanner_new_with_output_file(syncPathTemp, nullptr, 1);
    }
    if (scanner) {
        logfa("synctex_scanner_new_with_output_file: ok forsyncFilePath '%s'\n", syncPathTemp);
        goto Exit;
    }
    if (didRepeat) {
        logfa("synctex_scanner_new_with_output_file: failed for '%s'\n", pathAnsi.Get());
        return PDFSYNCERR_SYNCFILE_NOTFOUND;
    }

    // Note: https://github.com/sumatrapdfreader/sumatrapdf/discussions/2640#discussioncomment-2861368
    // reported failure to parse a large (12 MB) .synctex.gz even though file exists
    pathNoExt = path::GetPathNoExtTemp(syncFilePath);
    pathSyncGz = str::JoinTemp(pathNoExt, ".synctex.gz");
    if (!file::Exists(pathSyncGz)) {
        logfa("synctex_scanner_new_with_output_file: failed for '%s'\n", pathAnsi.Get());
        return PDFSYNCERR_SYNCFILE_NOTFOUND;
    }
    fsize = file::GetSize(pathSyncGz);
    logf("SyncTex::RebuildIndexIfNeeded: trying to uncompress %s, size: %d\n", pathSyncGz, (int)fsize);

    syncPathTemp = ungzipToFile(pathSyncGz);
    if (!syncPathTemp) {
        logfa("SyncTex::RebuildIndexIfNeeded: ungzipToFile('%s') failecd\n", pathSyncGz);
        goto Exit;
    }
    fsize = file::GetSize(syncPathTemp);
    logfa("SyncTex::RebuildIndexIfNeeded: retrying with uncompressed version '%s' of size %d\n", syncPathTemp, fsize);

    didRepeat = true;
    goto Repeat;

Exit:
    return MarkIndexWasRebuilt();
}

int SyncTex::DocToSource(int pageNo, Point pt, AutoFreeStr& filename, int* line, int* col) {
    logfa("SyncTex::DocToSource: '%s', pageNo: %d\n", syncFilePath.Get(), pageNo);
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        ReportIf(true);
        return res;
    }
    ReportIf(!scanner);

    // Coverity: at this point, this->scanner->flags.has_parsed == 1 and thus
    // synctex_scanner_parse never gets the chance to freeing the scanner
    if (synctex_edit_query(scanner, pageNo, (float)pt.x, (float)pt.y) <= 0) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    synctex_node_t node = synctex_next_result(this->scanner);
    if (!node) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    const char* name = synctex_scanner_get_name(this->scanner, synctex_node_tag(node));
    if (!name) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }

    bool isUtf8 = true;
    filename.Set(str::Dup(name));
TryAgainAnsi:
    if (!filename) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    // undecorate the filepath: replace * by space and / by \ (backslash)
    str::TransCharsInPlace(filename, "*/", " \\");
    // Convert the source filepath to an absolute path
    if (!path::IsAbsolute(filename)) {
        filename.Set(PrependDir(filename));
    }

    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && !file::Exists(filename)) {
        isUtf8 = false;
        filename.Set(strconv::AnsiToUtf8(name));
        goto TryAgainAnsi;
    }

    *line = synctex_node_line(node);
    *col = synctex_node_column(node);
    if (*col < 0) {
        *col = 0;
    }

    return PDFSYNCERR_SUCCESS;
}

int SyncTex::SourceToDoc(const char* srcfilename, int line, int col, int* page, Vec<Rect>& rects) {
    logfa("SyncTex::SourceToDoc: '%s', line: %d, col: %d\n", srcfilename, line, col);
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        ReportIf(true);
        return res;
    }
    ReportIf(!scanner);

    TempStr srcfilepath = (TempStr)srcfilename;
    // convert the source file to an absolute path
    if (!path::IsAbsolute(srcfilename)) {
        char* tmp = PrependDir(srcfilename);
        srcfilepath = str::DupTemp(tmp);
        str::Free(tmp);
    }
    if (!srcfilepath) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    bool isUtf8 = true;
    TempStr mb_srcfilepath = srcfilepath;
TryAgainAnsi:
    if (!mb_srcfilepath) {
        return PDFSYNCERR_OUTOFMEMORY;
    }
    int ret = synctex_display_query(this->scanner, mb_srcfilepath, line, col);
    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && -1 == ret) {
        isUtf8 = false;
        char* tmp = strconv::Utf8ToAnsi(srcfilepath);
        mb_srcfilepath = str::DupTemp(tmp);
        str::Free(tmp);
        goto TryAgainAnsi;
    }

    if (-1 == ret) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }
    if (0 == ret) {
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }

    synctex_node_t node;
    int firstpage = -1;
    rects.Reset();

    while ((node = synctex_next_result(this->scanner)) != nullptr) {
        if (firstpage == -1) {
            firstpage = synctex_node_page(node);
            if (firstpage <= 0 || firstpage > engine->PageCount()) {
                continue;
            }
            *page = (UINT)firstpage;
        }
        if (synctex_node_page(node) != firstpage) {
            continue;
        }

        RectF rc;
        rc.x = synctex_node_box_visible_h(node);
        rc.y = (double)synctex_node_box_visible_v(node) - (double)synctex_node_box_visible_height(node);
        rc.dx = synctex_node_box_visible_width(node),
        rc.dy = (double)synctex_node_box_visible_height(node) + (double)synctex_node_box_visible_depth(node);
        rects.Append(rc.Round());
    }

    if (firstpage <= 0) {
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }
    return PDFSYNCERR_SUCCESS;
}

/* moved synctex logging here so that we can log it to our logs */
extern "C" int _synctex_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeStr s = str::FmtV(fmt, args);
    char* s2 = str::JoinTemp(s, "\n"); // synctex doesn't use '\n'
    bool logAlways = true;
    log(s2, logAlways);
    va_end(args);
    return 0;
}
