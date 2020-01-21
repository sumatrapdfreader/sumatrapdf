/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <synctex_parser.h>
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "PdfSync.h"

// size of the mark highlighting the location calculated by forward-search
#define MARK_SIZE 10
// maximum error in the source file line number when doing forward-search
#define EPSILON_LINE 5
// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE 800
// Minimal vertical distance
#define PDFSYNC_EPSILON_Y 20

#define PDFSYNC_EXTENSION L".pdfsync"

#define SYNCTEX_EXTENSION L".synctex"
#define SYNCTEXGZ_EXTENSION L".synctex.gz"

struct PdfsyncFileIndex {
    size_t start, end; // first and one-after-last index of lines associated with a file
};

struct PdfsyncLine {
    UINT record; // index for mapping line(s) to point(s)
    size_t file; // index into srcfiles
    UINT line, column;
};

struct PdfsyncPoint {
    UINT record; // index for mapping point(s) to line(s)
    UINT page, x, y;
};

// Synchronizer based on .pdfsync file generated with the pdfsync tex package
class Pdfsync : public Synchronizer {
  public:
    Pdfsync(const WCHAR* syncfilename, EngineBase* engine) : Synchronizer(syncfilename), engine(engine) {
        AssertCrash(str::EndsWithI(syncfilename, PDFSYNC_EXTENSION));
    }

    virtual int DocToSource(UINT pageNo, PointI pt, AutoFreeWstr& filename, UINT* line, UINT* col);
    virtual int SourceToDoc(const WCHAR* srcfilename, UINT line, UINT col, UINT* page, Vec<RectI>& rects);

  private:
    int RebuildIndex();
    UINT SourceToRecord(const WCHAR* srcfilename, UINT line, UINT col, Vec<size_t>& records);

    EngineBase* engine;              // needed for converting between coordinate systems
    WStrVec srcfiles;                // source file names
    Vec<PdfsyncLine> lines;          // record-to-line mapping
    Vec<PdfsyncPoint> points;        // record-to-point mapping
    Vec<PdfsyncFileIndex> fileIndex; // start and end of entries for a file in <lines>
    Vec<size_t> sheetIndex;          // start of entries for a sheet in <points>
};

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer {
  public:
    SyncTex(const WCHAR* syncfilename, EngineBase* engine)
        : Synchronizer(syncfilename), engine(engine), scanner(nullptr) {
        AssertCrash(str::EndsWithI(syncfilename, SYNCTEX_EXTENSION));
    }
    virtual ~SyncTex() {
        synctex_scanner_free(scanner);
    }

    virtual int DocToSource(UINT pageNo, PointI pt, AutoFreeWstr& filename, UINT* line, UINT* col);
    virtual int SourceToDoc(const WCHAR* srcfilename, UINT line, UINT col, UINT* page, Vec<RectI>& rects);

  private:
    int RebuildIndex();

    EngineBase* engine; // needed for converting between coordinate systems
    synctex_scanner_t scanner;
};

Synchronizer::Synchronizer(const WCHAR* syncfilepath) : indexDiscarded(true), syncfilepath(str::Dup(syncfilepath)) {
    _wstat(syncfilepath, &syncfileTimestamp);
}

bool Synchronizer::IsIndexDiscarded() const {
    // was the index manually discarded?
    if (indexDiscarded)
        return true;

    // has the synchronization file been changed on disk?
    struct _stat newstamp;
    if (_wstat(syncfilepath, &newstamp) == 0 && difftime(newstamp.st_mtime, syncfileTimestamp.st_mtime) > 0) {
        // update time stamp
        memcpy((void*)&syncfileTimestamp, &newstamp, sizeof(syncfileTimestamp));
        return true; // the file has changed!
    }

    return false;
}

int Synchronizer::RebuildIndex() {
    indexDiscarded = false;
    // save sync file timestamp
    _wstat(syncfilepath, &syncfileTimestamp);
    return PDFSYNCERR_SUCCESS;
}

WCHAR* Synchronizer::PrependDir(const WCHAR* filename) const {
    AutoFreeWstr dir(path::GetDir(syncfilepath));
    return path::Join(dir, filename);
}

// Create a Synchronizer object for a PDF file.
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
int Synchronizer::Create(const WCHAR* pdffilename, EngineBase* engine, Synchronizer** sync) {
    if (!sync || !engine) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    const WCHAR* fileExt = path::GetExtNoFree(pdffilename);
    if (!str::EqI(fileExt, L".pdf")) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    AutoFreeWstr baseName(str::DupN(pdffilename, fileExt - pdffilename));

    // Check if a PDFSYNC file is present
    AutoFreeWstr syncFile(str::Join(baseName, PDFSYNC_EXTENSION));
    if (file::Exists(syncFile)) {
        *sync = new Pdfsync(syncFile, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    // check if SYNCTEX or compressed SYNCTEX file is present
    AutoFreeWstr texGzFile(str::Join(baseName, SYNCTEXGZ_EXTENSION));
    AutoFreeWstr texFile(str::Join(baseName, SYNCTEX_EXTENSION));

    if (file::Exists(texGzFile) || file::Exists(texFile)) {
        // due to a bug with synctex_parser.c, this must always be
        // the path to the .synctex file (even if a .synctex.gz file is used instead)
        *sync = new SyncTex(texFile, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    return PDFSYNCERR_SYNCFILE_NOTFOUND;
}

// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the caller must free() the result
WCHAR* Synchronizer::PrepareCommandline(const WCHAR* pattern, const WCHAR* filename, UINT line, UINT col) {
    const WCHAR* perc;
    str::WStr cmdline(256);

    while ((perc = str::FindChar(pattern, '%')) != nullptr) {
        cmdline.Append(pattern, perc - pattern);
        pattern = perc + 2;
        perc++;

        if (*perc == 'f')
            cmdline.AppendAndFree(path::Normalize(filename));
        else if (*perc == 'l')
            cmdline.AppendFmt(L"%u", line);
        else if (*perc == 'c')
            cmdline.AppendFmt(L"%u", col);
        else if (*perc == '%')
            cmdline.Append('%');
        else
            cmdline.Append(perc - 1, 2);
    }
    cmdline.Append(pattern);

    return cmdline.StealData();
}

// PDFSYNC synchronizer

// move to the next line in a list of zero-terminated lines
static char* Advance0Line(char* line, char* end) {
    line += str::Len(line);
    // skip all zeroes until the next non-empty line
    for (; line < end && !*line; line++)
        ;
    return line < end ? line : nullptr;
}

// see http://itexmac.sourceforge.net/pdfsync.html for the specification
int Pdfsync::RebuildIndex() {
    AutoFree data(file::ReadFile(syncfilepath));
    if (!data.data) {
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }
    // convert the file data into a list of zero-terminated strings
    str::TransChars(data.data, "\r\n", "\0\0");

    // parse preamble (jobname and version marker)
    char* line = data.data;
    char* dataEnd = data.data + data.size();

    // replace star by spaces (TeX uses stars instead of spaces in filenames)
    str::TransChars(line, "*/", " \\");
    AutoFreeWstr jobName(strconv::FromAnsi(line));
    jobName.Set(str::Join(jobName, L".tex"));
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
    UINT page = 1;
    sheetIndex.Append(0);

    // add the initial tex file to the source file stack
    filestack.Push(srcfiles.size());
    srcfiles.Append(jobName.StealData());
    PdfsyncFileIndex findex = {0};
    fileIndex.Append(findex);

    PdfsyncLine psline;
    PdfsyncPoint pspoint;

    // parse data
    UINT maxPageNo = engine->PageCount();
    while ((line = Advance0Line(line, dataEnd)) != nullptr) {
        if (!line)
            break;
        switch (*line) {
            case 'l':
                psline.file = filestack.Last();
                if (str::Parse(line, "l %u %u %u", &psline.record, &psline.line, &psline.column))
                    lines.Append(psline);
                else if (str::Parse(line, "l %u %u", &psline.record, &psline.line)) {
                    psline.column = 0;
                    lines.Append(psline);
                }
                // else dbg("Bad 'l' line in the pdfsync file");
                break;

            case 's':
                if (str::Parse(line, "s %u", &page))
                    sheetIndex.Append(points.size());
                // else dbg("Bad 's' line in the pdfsync file");
                // if (0 == page || page > maxPageNo)
                //     dbg("'s' line with invalid page number in the pdfsync file");
                break;

            case 'p':
                pspoint.page = page;
                if (0 == page || page > maxPageNo)
                    /* ignore point for invalid page number */;
                else if (str::Parse(line, "p %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y))
                    points.Append(pspoint);
                else if (str::Parse(line, "p* %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y))
                    points.Append(pspoint);
                // else dbg("Bad 'p' line in the pdfsync file");
                break;

            case '(': {
                AutoFreeWstr filename(strconv::FromAnsi(line + 1));
                // if the filename contains quotes then remove them
                // TODO: this should never happen!?
                if (filename[0] == '"' && filename[str::Len(filename) - 1] == '"') {
                    filename.Set(str::DupN(filename + 1, str::Len(filename) - 2));
                }
                // undecorate the filepath: replace * by space and / by \ (backslash)
                str::TransChars(filename, L"*/", L" \\");
                // if the file name extension is not specified then add the suffix '.tex'
                if (str::IsEmpty(path::GetExtNoFree(filename))) {
                    filename.Set(str::Join(filename, L".tex"));
                }
                // ensure that the path is absolute
                if (PathIsRelative(filename)) {
                    filename.Set(PrependDir(filename));
                }

                filestack.Push(srcfiles.size());
                srcfiles.Append(filename.StealData());
                findex.start = findex.end = lines.size();
                fileIndex.Append(findex);
            } break;

            case ')':
                if (filestack.size() > 1)
                    fileIndex.at(filestack.Pop()).end = lines.size();
                // else dbg("Unbalanced ')' line in the pdfsync file");
                break;

            default:
                // dbg("Ignoring invalid pdfsync line starting with '%c'", *line);
                break;
        }
    }

    fileIndex.at(0).end = lines.size();
    SubmitCrashIf(filestack.size() != 1);

    return Synchronizer::RebuildIndex();
}

// convert a coordinate from the sync file into a PDF coordinate
#define SYNC_TO_PDF_COORDINATE(c) (c / 65781.76)

static int cmpLineRecords(const void* a, const void* b) {
    return ((PdfsyncLine*)a)->record - ((PdfsyncLine*)b)->record;
}

int Pdfsync::DocToSource(UINT pageNo, PointI pt, AutoFreeWstr& filename, UINT* line, UINT* col) {
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // find the entry in the index corresponding to this page
    if (pageNo <= 0 || pageNo >= sheetIndex.size() || pageNo > (UINT)engine->PageCount())
        return PDFSYNCERR_INVALID_PAGE_NUMBER;

    // PdfSync coordinates are y-inversed
    RectI mbox = engine->PageMediabox(pageNo).Round();
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
    for (size_t i = sheetIndex.at((size_t)pageNo); i < points.size() && points.at(i).page == pageNo; i++) {
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

    if (selected_record == UINT_MAX)
        selected_record = closest_ydist_record;
    if (selected_record == UINT_MAX)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point

    // We have a record number, we need to find its declaration ('l ...') in the syncfile
    PdfsyncLine cmp;
    cmp.record = selected_record;
    PdfsyncLine* found =
        (PdfsyncLine*)bsearch(&cmp, lines.LendData(), lines.size(), sizeof(PdfsyncLine), cmpLineRecords);
    AssertCrash(found);
    if (!found)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    filename.SetCopy(srcfiles.at(found->file));
    *line = found->line;
    *col = found->column;

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
UINT Pdfsync::SourceToRecord(const WCHAR* srcfilename, UINT line, UINT col, Vec<size_t>& records) {
    UNUSED(col);
    if (!srcfilename)
        return PDFSYNCERR_INVALID_ARGUMENT;

    AutoFreeWstr srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(PrependDir(srcfilename));
    else
        srcfilepath.SetCopy(srcfilename);
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    // find the source file entry
    size_t isrc;
    for (isrc = 0; isrc < srcfiles.size(); isrc++)
        if (path::IsSame(srcfilepath, srcfiles.at(isrc)))
            break;
    if (isrc == srcfiles.size())
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    if (fileIndex.at(isrc).start == fileIndex.at(isrc).end)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = EPSILON_LINE; // distance to the closest record
    size_t lineIx = (size_t)-1;       // closest record-line index

    for (size_t isec = fileIndex.at(isrc).start; isec < fileIndex.at(isrc).end; isec++) {
        // does this section belong to the desired file?
        if (lines.at(isec).file != isrc)
            continue;

        UINT d = abs((int)lines.at(isec).line - (int)line);
        if (d < min_distance) {
            min_distance = d;
            lineIx = isec;
            if (0 == d)
                break; // We have found a record for the requested line!
        }
    }
    if (lineIx == (size_t)-1)
        return PDFSYNCERR_NORECORD_FOR_THATLINE;

    // we read all the consecutive records until we reach a record belonging to another line
    for (size_t i = lineIx; i < lines.size() && lines.at(i).line == lines.at(lineIx).line; i++)
        records.Push(lines.at(i).record);

    return PDFSYNCERR_SUCCESS;
}

int Pdfsync::SourceToDoc(const WCHAR* srcfilename, UINT line, UINT col, UINT* page, Vec<RectI>& rects) {
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    Vec<size_t> found_records;
    UINT ret = SourceToRecord(srcfilename, line, col, found_records);
    if (ret != PDFSYNCERR_SUCCESS || found_records.size() == 0)
        return ret;

    rects.Reset();

    // records have been found for the desired source position:
    // we now find the page and positions in the PDF corresponding to these found records
    UINT firstPage = UINT_MAX;
    for (size_t i = 0; i < points.size(); i++) {
        if (!found_records.Contains(points.at(i).record))
            continue;
        if (firstPage != UINT_MAX && firstPage != points.at(i).page)
            continue;
        firstPage = *page = points.at(i).page;
        RectD rc(SYNC_TO_PDF_COORDINATE(points.at(i).x), SYNC_TO_PDF_COORDINATE(points.at(i).y), MARK_SIZE, MARK_SIZE);
        // PdfSync coordinates are y-inversed
        RectD mbox = engine->PageMediabox(firstPage);
        rc.y = mbox.dy - (rc.y + rc.dy);
        rects.Push(rc.Round());
    }

    if (rects.size() > 0)
        return PDFSYNCERR_SUCCESS;
    // the record does not correspond to any point in the PDF: this is possible...
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}

// SYNCTEX synchronizer

int SyncTex::RebuildIndex() {
    synctex_scanner_free(scanner);
    scanner = nullptr;

    AutoFree syncfname(strconv::WstrToAnsi(syncfilepath));
    if (!syncfname.Get()) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    scanner = synctex_scanner_new_with_output_file(syncfname.Get(), nullptr, 1);
    if (!scanner) {
        return PDFSYNCERR_SYNCFILE_NOTFOUND; // cannot rebuild the index
    }

    return Synchronizer::RebuildIndex();
}

int SyncTex::DocToSource(UINT pageNo, PointI pt, AutoFreeWstr& filename, UINT* line, UINT* col) {
    if (IsIndexDiscarded()) {
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }
    CrashIf(!this->scanner);

    // Coverity: at this point, this->scanner->flags.has_parsed == 1 and thus
    // synctex_scanner_parse never gets the chance to freeing the scanner
    if (synctex_edit_query(this->scanner, pageNo, (float)pt.x, (float)pt.y) <= 0)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    synctex_node_t node = synctex_next_result(this->scanner);
    if (!node)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    const char* name = synctex_scanner_get_name(this->scanner, synctex_node_tag(node));
    if (!name)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    bool isUtf8 = true;
    filename.Set(strconv::Utf8ToWstr(name));
TryAgainAnsi:
    if (!filename)
        return PDFSYNCERR_OUTOFMEMORY;

    // undecorate the filepath: replace * by space and / by \ (backslash)
    str::TransChars(filename, L"*/", L" \\");
    // Convert the source filepath to an absolute path
    if (PathIsRelative(filename))
        filename.Set(PrependDir(filename));

    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && !file::Exists(filename)) {
        isUtf8 = false;
        filename.Set(strconv::FromAnsi(name));
        goto TryAgainAnsi;
    }

    *line = synctex_node_line(node);
    *col = synctex_node_column(node);

    return PDFSYNCERR_SUCCESS;
}

int SyncTex::SourceToDoc(const WCHAR* srcfilename, UINT line, UINT col, UINT* page, Vec<RectI>& rects) {
    if (IsIndexDiscarded()) {
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }
    AssertCrash(this->scanner);

    AutoFreeWstr srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(PrependDir(srcfilename));
    else
        srcfilepath.SetCopy(srcfilename);
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    bool isUtf8 = true;
    const char* mb_srcfilepath = strconv::WstrToUtf8(srcfilepath).data();
TryAgainAnsi:
    if (!mb_srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;
    int ret = synctex_display_query(this->scanner, mb_srcfilepath, line, col);
    str::Free(mb_srcfilepath);
    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && -1 == ret) {
        isUtf8 = false;
        mb_srcfilepath = (char*)strconv::WstrToAnsi(srcfilepath).data();
        goto TryAgainAnsi;
    }

    if (-1 == ret)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    if (0 == ret)
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;

    synctex_node_t node;
    int firstpage = -1;
    rects.Reset();

    while ((node = synctex_next_result(this->scanner)) != nullptr) {
        if (firstpage == -1) {
            firstpage = synctex_node_page(node);
            if (firstpage <= 0 || firstpage > engine->PageCount())
                continue;
            *page = (UINT)firstpage;
        }
        if (synctex_node_page(node) != firstpage)
            continue;

        RectD rc;
        rc.x = synctex_node_box_visible_h(node);
        rc.y = (double)synctex_node_box_visible_v(node) - (double)synctex_node_box_visible_height(node);
        rc.dx = synctex_node_box_visible_width(node),
        rc.dy = (double)synctex_node_box_visible_height(node) + (double)synctex_node_box_visible_depth(node);
        rects.Push(rc.Round());
    }

    if (firstpage <= 0)
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    return PDFSYNCERR_SUCCESS;
}
