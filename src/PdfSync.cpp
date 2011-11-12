/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfSync.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "DisplayModel.h"

#include <shlwapi.h>
#include <time.h>
#include <synctex_parser.h>

// size of the mark highlighting the location calculated by forward-search
#define MARK_SIZE               10 
// maximum error in the source file line number when doing forward-search
#define EPSILON_LINE            5  
// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE  800
// Minimal vertical distance
#define PDFSYNC_EPSILON_Y       20

#define PDFSYNC_EXTENSION       _T(".pdfsync")

#define SYNCTEX_EXTENSION       _T(".synctex")
#define SYNCTEXGZ_EXTENSION     _T(".synctex.gz")

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
class Pdfsync : public Synchronizer
{
public:
    Pdfsync(const TCHAR* syncfilename) : Synchronizer(syncfilename) { }

    virtual int DocToSource(UINT pageNo, PointI pt, ScopedMem<TCHAR>& filename, UINT *line, UINT *col);
    virtual int SourceToDoc(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI>& rects);

private:
    int RebuildIndex();
    UINT SourceToRecord(const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t>& records);

private:
    StrVec srcfiles;            // source file names
    Vec<PdfsyncLine> lines;     // record-to-line mapping
    Vec<PdfsyncPoint> points;   // record-to-point mapping
    Vec<PdfsyncFileIndex> fileIndex; // start and end of entries for a file in <lines>
    Vec<size_t> sheetIndex;     // start of entries for a sheet in <points>
};

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer
{
public:
    SyncTex(const TCHAR* syncfilename, DisplayModel *dm) :
        Synchronizer(syncfilename), dm(dm), scanner(NULL)
    {
        assert(str::EndsWithI(syncfilename, SYNCTEX_EXTENSION) ||
               str::EndsWithI(syncfilename, SYNCTEXGZ_EXTENSION));
    }
    virtual ~SyncTex()
    {
        synctex_scanner_free(scanner);
    }

    virtual int DocToSource(UINT pageNo, PointI pt, ScopedMem<TCHAR>& filename, UINT *line, UINT *col);
    virtual int SourceToDoc(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects);

private:
    int RebuildIndex();

    DisplayModel *dm; // needed for converting between coordinate systems
    synctex_scanner_t scanner;
};

Synchronizer::Synchronizer(const TCHAR* syncfilepath) :
    indexDiscarded(true), syncfilepath(str::Dup(syncfilepath))
{
    _tstat(syncfilepath, &syncfileTimestamp);
}

bool Synchronizer::IsIndexDiscarded() const
{
    // was the index manually discarded?
    if (indexDiscarded)
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

int Synchronizer::RebuildIndex()
{
    indexDiscarded = false;
    // save sync file timestamp
    _tstat(syncfilepath, &syncfileTimestamp);
    return PDFSYNCERR_SUCCESS;
}

TCHAR * Synchronizer::PrependDir(const TCHAR* filename) const
{
    ScopedMem<TCHAR> dir(path::GetDir(syncfilepath));
    return path::Join(dir, filename);
}

// Create a Synchronizer object for a PDF file.
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
int Synchronizer::Create(const TCHAR *pdffilename, DisplayModel *dm, Synchronizer **sync)
{
    if (!sync || !dm)
        return PDFSYNCERR_INVALID_ARGUMENT;

    const TCHAR *fileExt = path::GetExt(pdffilename);
    if (!str::EqI(fileExt, _T(".pdf"))) {
        DBG_OUT("Bad PDF filename! (%s)\n", pdffilename);
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    ScopedMem<TCHAR> baseName(str::DupN(pdffilename, fileExt - pdffilename));

    // Check if a PDFSYNC file is present
    ScopedMem<TCHAR> syncFile(str::Join(baseName, PDFSYNC_EXTENSION));
    if (file::Exists(syncFile)) {
        *sync = new Pdfsync(syncFile);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    // check if SYNCTEX or compressed SYNCTEX file is present
    ScopedMem<TCHAR> texGzFile(str::Join(baseName, SYNCTEXGZ_EXTENSION));
    ScopedMem<TCHAR> texFile(str::Join(baseName, SYNCTEX_EXTENSION));

    if (file::Exists(texGzFile) || file::Exists(texFile)) {
        // due to a bug with synctex_parser.c, this must always be 
        // the path to the .synctex file (even if a .synctex.gz file is used instead)
        *sync = new SyncTex(texFile, dm);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    return PDFSYNCERR_SYNCFILE_NOTFOUND;
}

// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the caller must free() the result
TCHAR * Synchronizer::PrepareCommandline(const TCHAR* pattern, const TCHAR* filename, UINT line, UINT col)
{
    const TCHAR* perc;
    str::Str<TCHAR> cmdline(256);

    while ((perc = str::FindChar(pattern, '%'))) {
        cmdline.Append(pattern, perc - pattern);
        pattern = perc + 2;
        perc++;

        if (*perc == 'f')
            cmdline.AppendAndFree(path::Normalize(filename));
        else if (*perc == 'l')
            cmdline.AppendFmt(_T("%u"), line);
        else if (*perc == 'c')
            cmdline.AppendFmt(_T("%u"), col);
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
static char *Advance0Line(char *line, char *end)
{
    line += str::Len(line);
    // skip all zeroes until the next non-empty line
    for (; line < end && !*line; line++);
    return line < end ? line : NULL;
}

// see http://itexmac.sourceforge.net/pdfsync.html for the specification
int Pdfsync::RebuildIndex()
{
    size_t len;
    ScopedMem<char> data(file::ReadAll(syncfilepath, &len));
    if (!data)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    // convert the file data into a list of zero-terminated strings
    str::TransChars(data, "\r\n", "\0\0");

    // parse preamble (jobname and version marker)
    char *line = data;
    char *dataEnd = data + len;

    // replace star by spaces (TeX uses stars instead of spaces in filenames)
    str::TransChars(line, "*/", " \\");
    ScopedMem<TCHAR> jobName(str::conv::FromAnsi(line));
    jobName.Set(str::Join(jobName, _T(".tex")));
    jobName.Set(PrependDir(jobName));

    line = Advance0Line(line, dataEnd);
    UINT versionNumber = 0;
    if (!line || sscanf(line, "version %u", &versionNumber) != 1 || versionNumber != 1)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

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
    filestack.Push(srcfiles.Count());
    srcfiles.Append(jobName.StealData());
    PdfsyncFileIndex findex = { 0 };
    fileIndex.Append(findex);

    PdfsyncLine psline;
    PdfsyncPoint pspoint;

    // parse data
    while ((line = Advance0Line(line, dataEnd))) {
        switch (*line) {
        case 'l':
            psline.file = filestack.Last();
            psline.column = 0;
            if (sscanf(line, "l %u %u %u", &psline.record, &psline.line, &psline.column) >= 2)
                lines.Append(psline);
            else
                DBG_OUT("Bad 'l' line in the pdfsync file\n");
            break;

        case 's':
            if (sscanf(line, "s %u", &page) == 1)
                sheetIndex.Append(points.Count());
            else
                DBG_OUT("Bad 's' line in the pdfsync file\n");
            break;

        case 'p':
            pspoint.page = page;
            if (sscanf(line, "p %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y) == 3)
                points.Append(pspoint);
            else if (sscanf(line, "p* %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y) == 3)
                points.Append(pspoint);
            else
                DBG_OUT("Bad 'p' line in the pdfsync file\n");
            break;

        case '(':
            {
                ScopedMem<TCHAR> filename(str::conv::FromAnsi(line + 1));
                // if the filename contains quotes then remove them
                // TODO: this should never happen!?
                if (filename[0] == '"' && filename[str::Len(filename) - 1] == '"')
                    filename.Set(str::DupN(filename + 1, str::Len(filename) - 2));
                // undecorate the filepath: replace * by space and / by \ 
                str::TransChars(filename, _T("*/"), _T(" \\"));
                // if the file name extension is not specified then add the suffix '.tex'
                if (str::IsEmpty(path::GetExt(filename)))
                    filename.Set(str::Join(filename, _T(".tex")));
                // ensure that the path is absolute
                if (PathIsRelative(filename))
                    filename.Set(PrependDir(filename));

                filestack.Push(srcfiles.Count());
                srcfiles.Append(filename.StealData());
                findex.start = findex.end = lines.Count();
                fileIndex.Append(findex);
            }
            break;

        case ')':
            if (filestack.Count() > 1)
                fileIndex.At(filestack.Pop()).end = lines.Count();
            else
                DBG_OUT("Unbalanced ')' line in the pdfsync file\n");
            break;

        default:
            DBG_OUT("Ignoring invalid pdfsync line starting with '%c'\n", *line);
            break;
        }
    }

    fileIndex.At(0).end = lines.Count();
    assert(filestack.Count() == 1);

    return Synchronizer::RebuildIndex();;
}

// convert a coordinate from the sync file into a PDF coordinate
#define SYNC_TO_PDF_COORDINATE(c)  (c/65781.76)

static int cmpLineRecords(const void *a, const void *b)
{
    return ((PdfsyncLine *)a)->record - ((PdfsyncLine *)b)->record;
}

int Pdfsync::DocToSource(UINT pageNo, PointI pt, ScopedMem<TCHAR>& filename, UINT *line, UINT *col)
{
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // find the entry in the index corresponding to this page
    if (pageNo < 1 || sheetIndex.Count() <= pageNo)
        return PDFSYNCERR_INVALID_PAGE_NUMBER;

    // distance to the closest pdf location (in the range <PDFSYNC_EPSILON_SQUARE)
    UINT closest_xydist = (UINT)-1;
    UINT selected_record = (UINT)-1;
    // If no record is found within a distance^2 of PDFSYNC_EPSILON_SQUARE
    // (selected_record == -1) then we pick up the record that is closest 
    // vertically to the hit-point.
    UINT closest_ydist = (UINT)-1; // vertical distance between the hit point and the vertically-closest record
    UINT closest_xdist = (UINT)-1; // horizontal distance between the hit point and the vertically-closest record
    UINT closest_ydist_record = (UINT)-1; // vertically-closest record

    // read all the sections of 'p' declarations for this pdf sheet
    for (size_t i = sheetIndex.At(pageNo); i < points.Count() && points.At(i).page == pageNo; i++) {
        // check whether it is closer than the closest point found so far
        UINT dx = abs(pt.x - (int)SYNC_TO_PDF_COORDINATE(points.At(i).x));
        UINT dy = abs(pt.y - (int)SYNC_TO_PDF_COORDINATE(points.At(i).y));
        UINT dist = dx * dx + dy * dy;
        if (dist < PDFSYNC_EPSILON_SQUARE && dist < closest_xydist) {
            selected_record = points.At(i).record;
            closest_xydist = dist;
        }
        else if ((closest_xydist == (UINT)-1) && dy < PDFSYNC_EPSILON_Y &&
                 (dy < closest_ydist || (dy == closest_ydist && dx < closest_xdist))) {
            closest_ydist_record = points.At(i).record;
            closest_ydist = dy;
            closest_xdist = dx;
        }
    }

    if (selected_record == (UINT)-1)
        selected_record = closest_ydist_record;
    if (selected_record == (UINT)-1)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point

    // We have a record number, we need to find its declaration ('l ...') in the syncfile
    PdfsyncLine cmp; cmp.record = selected_record;
    PdfsyncLine *found = (PdfsyncLine *)bsearch(&cmp, lines.LendData(), lines.Count(), sizeof(PdfsyncLine), cmpLineRecords);
    assert(found);
    if (!found)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    filename.Set(str::Dup(srcfiles.At(found->file)));
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
UINT Pdfsync::SourceToRecord(const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t> &records)
{
    if (!srcfilename)
        return PDFSYNCERR_INVALID_ARGUMENT;

    ScopedMem<TCHAR> srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(PrependDir(srcfilename));
    else
        srcfilepath.Set(str::Dup(srcfilename));
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    // find the source file entry
    size_t isrc;
    for (isrc = 0; isrc < srcfiles.Count(); isrc++)
        if (path::IsSame(srcfilepath, srcfiles.At(isrc)))
            break;
    if (isrc == srcfiles.Count())
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    if (fileIndex.At(isrc).start == fileIndex.At(isrc).end)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = EPSILON_LINE; // distance to the closest record
    size_t lineIx = (size_t)-1; // closest record-line index

    for (size_t isec = fileIndex.At(isrc).start; isec < fileIndex.At(isrc).end; isec++) {
        // does this section belong to the desired file?
        if (lines.At(isec).file != isrc)
            continue;

        UINT d = abs((int)lines.At(isec).line - (int)line);
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
    for (size_t i = lineIx; i < lines.Count() && lines.At(i).line == lines.At(lineIx).line; i++)
        records.Push(lines.At(i).record);

    return PDFSYNCERR_SUCCESS;
}

int Pdfsync::SourceToDoc(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    Vec<size_t> found_records;
    UINT ret = SourceToRecord(srcfilename, line, col, found_records);
    if (ret != PDFSYNCERR_SUCCESS || found_records.Count() == 0) {
        DBG_OUT("source->pdf: %s:%u -> no record found, error:%u\n", srcfilename, line, ret);
        return ret;
    }

    rects.Reset();

    // records have been found for the desired source position:
    // we now find the pages and position in the PDF corresponding to these found records
    UINT firstPage = (UINT)-1;
    for (size_t irecord = 0; irecord < found_records.Count(); irecord++) {
        for (size_t i = 0; i < points.Count(); i++) {
            if (points.At(i).record != found_records.At(irecord) ||
                firstPage != (UINT)-1 && firstPage != points.At(i).page) {
                continue;
            }
            firstPage = *page = points.At(i).page;
            RectI rc((int)SYNC_TO_PDF_COORDINATE(points.At(i).x),
                     (int)SYNC_TO_PDF_COORDINATE(points.At(i).y),
                     MARK_SIZE, MARK_SIZE);
            rects.Push(rc);
            DBG_OUT("source->pdf: %s:%u -> record:%u -> page:%u, x:%u, y:%u\n",
                    srcfilename, line, points.At(i).record, firstPage, rc.x, rc.y);
        }
    }

    if (rects.Count() > 0)
        return PDFSYNCERR_SUCCESS;
    // the record does not correspond to any point in the PDF: this is possible...  
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}


// SYNCTEX synchronizer

int SyncTex::RebuildIndex() {
    synctex_scanner_free(this->scanner);
    this->scanner = NULL;

    ScopedMem<char> syncfname(str::conv::ToAnsi(syncfilepath));
    if (!syncfname)
        return PDFSYNCERR_OUTOFMEMORY;

    scanner = synctex_scanner_new_with_output_file(syncfname, NULL, 1);
    if (!scanner)
        return PDFSYNCERR_SYNCFILE_NOTFOUND; // cannot rebuild the index

    return Synchronizer::RebuildIndex();
}

int SyncTex::DocToSource(UINT pageNo, PointI pt, ScopedMem<TCHAR>& filename, UINT *line, UINT *col)
{
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    assert(this->scanner);

    if (!dm->ValidPageNo(pageNo))
        return PDFSYNCERR_INVALID_PAGE_NUMBER;
    PageInfo *pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo)
        return PDFSYNCERR_OUTOFMEMORY;

    // convert from BottomLeft to TopLeft coordinates
    pt.y = pageInfo->page.Convert<int>().dy - pt.y;

    if (synctex_edit_query(this->scanner, pageNo, (float)pt.x, (float)pt.y) < 0)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    synctex_node_t node = synctex_next_result(this->scanner);
    if (!node)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    const char *name = synctex_scanner_get_name(this->scanner, synctex_node_tag(node));
    bool isUtf8 = true;
    filename.Set(str::conv::FromUtf8(name));
TryAgainAnsi:
    if (!filename)
        return PDFSYNCERR_OUTOFMEMORY;

    // undecorate the filepath: replace * by space and / by \ 
    str::TransChars(filename, _T("*/"), _T(" \\"));
    // Convert the source filepath to an absolute path
    if (PathIsRelative(filename))
        filename.Set(PrependDir(filename));

    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && !file::Exists(filename)) {
        isUtf8 = false;
        filename.Set(str::conv::FromAnsi(name));
        goto TryAgainAnsi;
    }

    *line = synctex_node_line(node);
    *col = synctex_node_column(node);

    return PDFSYNCERR_SUCCESS;
}

int SyncTex::SourceToDoc(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (IsIndexDiscarded())
        if (RebuildIndex() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    assert(this->scanner);

    ScopedMem<TCHAR> srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(PrependDir(srcfilename));
    else
        srcfilepath.Set(str::Dup(srcfilename));
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    bool isUtf8 = true;
    char *mb_srcfilepath = str::conv::ToUtf8(srcfilepath);
TryAgainAnsi:
    if (!mb_srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;
    int ret = synctex_display_query(this->scanner, mb_srcfilepath, line, col);
    free(mb_srcfilepath);
    // recent SyncTeX versions encode in UTF-8 instead of ANSI
    if (isUtf8 && -1 == ret) {
        isUtf8 = false;
        mb_srcfilepath = str::conv::ToAnsi(srcfilepath);
        goto TryAgainAnsi;
    }

    if (-1 == ret)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;    
    if (0 == ret)
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;

    synctex_node_t node;
    PageInfo *pageInfo = NULL;
    int firstpage = -1;
    rects.Reset();

    while (node = synctex_next_result(this->scanner)) {
        if (firstpage == -1) {
            firstpage = synctex_node_page(node);
            *page = (UINT)firstpage;
            if (dm->ValidPageNo(firstpage))
                pageInfo = dm->GetPageInfo(firstpage);
            if (!pageInfo)
                continue;
        }
        if (synctex_node_page(node) != firstpage)
            continue;

        RectI rc;
        rc.x  = (int)synctex_node_box_visible_h(node);
        rc.y  = (int)(synctex_node_box_visible_v(node) - synctex_node_box_visible_height(node));
        rc.dx = (int)synctex_node_box_visible_width(node),
        rc.dy = (int)(synctex_node_box_visible_height(node) + synctex_node_box_visible_depth(node));
        // convert from TopLeft to BottomLeft coordinates
        if (pageInfo)
            rc.y = pageInfo->page.Convert<int>().dy - (rc.y + rc.dy);
        rects.Push(rc);
    }

    if (firstpage <= 0)
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    return PDFSYNCERR_SUCCESS;
}
