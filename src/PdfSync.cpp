/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
// PDF-source synchronizer based on .pdfsync file

#include "PdfSync.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include <shlwapi.h>

// size of the mark highlighting the location calculated by forward-search
#define MARK_SIZE               10 
// maximum error in the source file line number when doing forward-search
#define EPSILON_LINE            5  
// Minimal error distance^2 between a point clicked by the user and a PDF mark
#define PDFSYNC_EPSILON_SQUARE  800
// Minimal vertical distance
#define PDFSYNC_EPSILON_Y       20

#define PDFSYNC_EXTENSION       _T(".pdfsync")

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
    Pdfsync(const TCHAR* _syncfilename) : Synchronizer(_syncfilename, BottomLeft) { }

    int rebuild_index();
    virtual UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    virtual UINT source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI>& rects);

private:
    UINT source_to_record(const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t>& records);

private:
    StrVec srcfiles;            // source file names
    Vec<PdfsyncLine> lines;     // record-to-line mapping
    Vec<PdfsyncPoint> points;   // record-to-point mapping
    Vec<PdfsyncFileIndex> file_index;   // start and end of entries for a file in <lines>
    Vec<size_t> sheet_index;    // start of entries for a sheet in <points>
};

#ifdef SYNCTEX_FEATURE
#include <synctex_parser.h>

#define SYNCTEX_EXTENSION       _T(".synctex")
#define SYNCTEXGZ_EXTENSION     _T(".synctex.gz")

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer
{
public:
    SyncTex(const TCHAR* _syncfilename) : Synchronizer(_syncfilename, TopLeft)
    {
        assert(str::EndsWithI(_syncfilename, SYNCTEX_EXTENSION) ||
               str::EndsWithI(_syncfilename, SYNCTEXGZ_EXTENSION));
        scanner = NULL;
    }
    virtual ~SyncTex()
    {
        synctex_scanner_free(scanner);
    }

    int rebuild_index();
    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    UINT source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects);

private:
    synctex_scanner_t scanner;
};
#endif

// Create a Synchronizer object for a PDF file.
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
int Synchronizer::Create(const TCHAR *pdffilename, Synchronizer **sync)
{
    if (!sync)
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

#ifdef SYNCTEX_FEATURE
    // check if SYNCTEX or compressed SYNCTEX file is present
    ScopedMem<TCHAR> texGzFile(str::Join(baseName, SYNCTEXGZ_EXTENSION));
    ScopedMem<TCHAR> texFile(str::Join(baseName, SYNCTEX_EXTENSION));

    if (file::Exists(texGzFile) || file::Exists(texFile)) {
        // due to a bug with synctex_parser.c, this must always be 
        // the path to the .synctex file (even if a .synctex.gz file is used instead)
        *sync = new SyncTex(texFile);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }
#endif

    return PDFSYNCERR_SYNCFILE_NOTFOUND;
}

// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the caller must free() the result
TCHAR * Synchronizer::prepare_commandline(const TCHAR* pattern, const TCHAR* filename, UINT line, UINT col)
{
    const TCHAR* perc;
    str::Str<TCHAR> cmdline(256);

    while ((perc = str::FindChar(pattern, '%'))) {
        cmdline.Append(pattern, perc - pattern);
        pattern = perc + 2;
        perc++;

        if (*perc == 'f')
            cmdline.Append(filename);
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
int Pdfsync::rebuild_index()
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
    jobName.Set(path::Join(dir, jobName));

    line = Advance0Line(line, dataEnd);
    UINT versionNumber = 0;
    if (!line || sscanf(line, "version %u", &versionNumber) != 1 || versionNumber != 1)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // reset synchronizer database
    srcfiles.Reset();
    lines.Reset();
    points.Reset();
    file_index.Reset();
    sheet_index.Reset();

    Vec<size_t> filestack;
    UINT page = 1;
    sheet_index.Append(0);

    // add the initial tex file to the source file stack
    filestack.Push(srcfiles.Count());
    srcfiles.Append(jobName.StealData());
    PdfsyncFileIndex findex = { 0 };
    file_index.Append(findex);

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
                sheet_index.Append(points.Count());
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
                TCHAR buf[MAX_PATH];
                ScopedMem<TCHAR> filename(str::conv::FromAnsi(line + 1));
                str::BufSet(buf, dimof(buf), filename);
                // if the filename contains quotes then remove them
                PathUnquoteSpaces(buf);
                // undecorate the filepath: replace * by space and / by \ 
                str::TransChars(buf, _T("*/"), _T(" \\"));
                // if the file name extension is not specified then add the suffix '.tex'
                if (str::IsEmpty(path::GetExt(buf)))
                    PathAddExtension(buf, _T(".tex"));
                // ensure that the path is absolute
                if (PathIsRelative(buf)) {
                    ScopedMem<TCHAR> absPath(path::Join(dir, buf));
                    if (absPath)
                        str::BufSet(buf, dimof(buf), absPath);
                }

                filestack.Push(srcfiles.Count());
                srcfiles.Append(str::Dup(buf));
                findex.start = findex.end = lines.Count();
                file_index.Append(findex);
            }
            break;

        case ')':
            if (filestack.Count() > 1)
                file_index[filestack.Pop()].end = lines.Count();
            else
                DBG_OUT("Unbalanced ')' line in the pdfsync file\n");
            break;

        default:
            DBG_OUT("Ignoring invalid pdfsync line starting with '%c'\n", *line);
            break;
        }
    }

    file_index[0].end = lines.Count();
    assert(filestack.Count() == 1);

    return Synchronizer::rebuild_index();;
}

// convert a coordinate from the sync file into a PDF coordinate
#define SYNC_TO_PDF_COORDINATE(c)  (c/65781.76)

static int cmpLineRecords(const void *a, const void *b)
{
    return ((PdfsyncLine *)a)->record - ((PdfsyncLine *)b)->record;
}

UINT Pdfsync::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col)
{
    if (is_index_discarded())
        if (rebuild_index() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // find the entry in the index corresponding to this page
    if (sheet < 1 || sheet_index.Count() <= sheet)
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
    for (size_t i = sheet_index[sheet]; i < points.Count() && points[i].page == sheet; i++) {
        // check whether it is closer than the closest point found so far
        UINT dx = abs((int)x - (int)SYNC_TO_PDF_COORDINATE(points[i].x));
        UINT dy = abs((int)y - (int)SYNC_TO_PDF_COORDINATE(points[i].y));
        UINT dist = dx * dx + dy * dy;
        if (dist < PDFSYNC_EPSILON_SQUARE && dist < closest_xydist) {
            selected_record = points[i].record;
            closest_xydist = dist;
        }
        else if ((closest_xydist == (UINT)-1) && dy < PDFSYNC_EPSILON_Y &&
                 (dy < closest_ydist || (dy == closest_ydist && dx < closest_xdist))) {
            closest_ydist_record = points[i].record;
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

    str::BufSet(srcfilepath, cchFilepath, srcfiles[found->file]);
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
UINT Pdfsync::source_to_record(const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t> &records)
{
    if (!srcfilename)
        return PDFSYNCERR_INVALID_ARGUMENT;

    ScopedMem<TCHAR> srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(path::Join(dir, srcfilename));
    else
        srcfilepath.Set(str::Dup(srcfilename));
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    // find the source file entry
    size_t isrc;
    for (isrc = 0; isrc < srcfiles.Count(); isrc++)
        if (path::IsSame(srcfilepath, srcfiles[isrc]))
            break;
    if (isrc == srcfiles.Count())
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    if (file_index[isrc].start == file_index[isrc].end)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = EPSILON_LINE; // distance to the closest record
    size_t lineIx = (size_t)-1; // closest record-line index

    for (size_t isec = file_index[isrc].start; isec < file_index[isrc].end; isec++) {
        // does this section belong to the desired file?
        if (lines[isec].file != isrc)
            continue;

        UINT d = abs((int)lines[isec].line - (int)line);
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
    for (size_t i = lineIx; i < lines.Count() && lines[i].line == lines[lineIx].line; i++)
        records.Push(lines[i].record);

    return PDFSYNCERR_SUCCESS;
}

UINT Pdfsync::source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (is_index_discarded())
        if (rebuild_index() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    Vec<size_t> found_records;
    UINT ret = source_to_record(srcfilename, line, col, found_records);
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
            if (points[i].record != found_records[irecord] ||
                firstPage != (UINT)-1 && firstPage != points[i].page) {
                continue;
            }
            firstPage = *page = points[i].page;
            RectI rc((int)SYNC_TO_PDF_COORDINATE(points[i].x),
                     (int)SYNC_TO_PDF_COORDINATE(points[i].y),
                     MARK_SIZE, MARK_SIZE);
            rects.Push(rc);
            DBG_OUT("source->pdf: %s:%u -> record:%u -> page:%u, x:%u, y:%u\n",
                    srcfilename, line, points[i].record, firstPage, rc.x, rc.y);
        }
    }

    if (rects.Count() > 0)
        return PDFSYNCERR_SUCCESS;
    // the record does not correspond to any point in the PDF: this is possible...  
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}


// SYNCTEX synchronizer

#ifdef SYNCTEX_FEATURE

int SyncTex::rebuild_index() {
    if (this->scanner)
        synctex_scanner_free(this->scanner);

    ScopedMem<char> syncfname(str::conv::ToAnsi(syncfilepath));
    if (!syncfname)
        return PDFSYNCERR_OUTOFMEMORY;

    scanner = synctex_scanner_new_with_output_file(syncfname, NULL, 1);
    if (!scanner)
        return 1; // cannot rebuild the index
    return Synchronizer::rebuild_index();
}

UINT SyncTex::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col)
{
    if (this->is_index_discarded())
        if (rebuild_index() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    if (synctex_edit_query(this->scanner, sheet, (float)x, (float)y) < 0)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;

    synctex_node_t node;
    while (node = synctex_next_result(this->scanner)) {
        *line = synctex_node_line(node);
        *col = synctex_node_column(node);
        const char *name = synctex_scanner_get_name(this->scanner,synctex_node_tag(node));
        ScopedMem<TCHAR> srcfilename(str::conv::FromAnsi(name));
        if (!srcfilename)
            return PDFSYNCERR_OUTOFMEMORY;

        // undecorate the filepath: replace * by space and / by \ 
        str::TransChars(srcfilename, _T("*/"), _T(" \\"));
        // Convert the source filepath to an absolute path
        if (PathIsRelative(srcfilename))
            srcfilename.Set(path::Join(this->dir, srcfilename));

        str::BufSet(srcfilepath, cchFilepath, srcfilename);
        return PDFSYNCERR_SUCCESS;
    }
    return PDFSYNCERR_NO_SYNC_AT_LOCATION;
}

UINT SyncTex::source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (this->is_index_discarded())
        if (rebuild_index() != PDFSYNCERR_SUCCESS)
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    ScopedMem<TCHAR> srcfilepath;
    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(path::Join(dir, srcfilename));
    else
        srcfilepath.Set(str::Dup(srcfilename));
    if (!srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;

    char *mb_srcfilepath = str::conv::ToAnsi(srcfilepath);
    if (!mb_srcfilepath)
        return PDFSYNCERR_OUTOFMEMORY;
    int ret = synctex_display_query(this->scanner,mb_srcfilepath,line,col);
    free(mb_srcfilepath);
    switch (ret) {
        case -1:
            return PDFSYNCERR_UNKNOWN_SOURCEFILE;    
        case 0:
            return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
        default:
            synctex_node_t node;
            int firstpage = -1;
            rects.Reset();
            while (node = synctex_next_result(this->scanner)) {
                if (firstpage == -1) {
                    firstpage = synctex_node_page(node);
                    *page = (UINT)firstpage;
                }
                if (synctex_node_page(node) != firstpage)
                    continue;

                RectI rc;
                rc.x  = (int)synctex_node_box_visible_h(node);
                rc.y  = (int)(synctex_node_box_visible_v(node) - synctex_node_box_visible_height(node));
                rc.dx = (int)synctex_node_box_visible_width(node),
                rc.dy = (int)(synctex_node_box_visible_height(node) + synctex_node_box_visible_depth(node));
                rects.Push(rc);
            }
            return ( firstpage > 0 ) ? PDFSYNCERR_SUCCESS : PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }
}
#endif


// DDE commands handling

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("received WM_DDE_INITIATE from %p with %08lx\n", (HWND)wparam, lparam);

    ATOM aServer = GlobalAddAtom(PDFSYNC_DDE_SERVICE);
    ATOM aTopic = GlobalAddAtom(PDFSYNC_DDE_TOPIC);

    if (LOWORD(lparam) == aServer && HIWORD(lparam) == aTopic) {
        if (!IsWindowUnicode((HWND)wparam))
            DBG_OUT("The client window is ANSI!\n");
        DBG_OUT("Sending WM_DDE_ACK to %p\n", (HWND)wparam);
        SendMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, MAKELPARAM(aServer, 0));
    }
    else {
        GlobalDeleteAtom(aServer);
        GlobalDeleteAtom(aTopic);
    }
    return 0;
}

// DDE commands

static void SetFocusHelper(HWND hwnd)
{
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    SetFocus(hwnd);
}

// Synchronization command format:
// [<DDECOMMAND_SYNC>(["<pdffile>",]"<srcfile>",<line>,<col>[,<newwindow>,<setfocus>])]
static const TCHAR *HandleSyncCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    const TCHAR *next = str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%? \"%S\",%u,%u)]"),
                                   &pdfFile, &srcFile, &line, &col);
    if (!next)
        next = str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%? \"%S\",%u,%u,%u,%u)]"),
                          &pdfFile, &srcFile, &line, &col, &newWindow, &setFocus);
    // allow to omit the pdffile path, so that editors don't have to know about
    // multi-file projects (requires that the PDF has already been opened)
    if (!next) {
        pdfFile.Set(NULL);
        next = str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%u,%u)]"),
                          &srcFile, &line, &col);
        if (!next)
            next = str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%u,%u,%u,%u)]"),
                              &srcFile, &line, &col, &newWindow, &setFocus);
    }

    if (!next)
        return NULL;

    WindowInfo *win = NULL;
    if (pdfFile) {
        // check if the PDF is already opened
        win = FindWindowInfoByFile(pdfFile);
        // if not then open it
        if (newWindow || !win)
            win = LoadDocument(pdfFile, !newWindow ? win : NULL);
        else if (win && !win->IsDocLoaded())
            win->Reload();
    }
    else {
        // check if any opened PDF has sync information for the source file
        win = FindWindowInfoBySyncFile(srcFile);
        if (!win)
            DBG_OUT("PdfSync: No open PDF file found for %s!", srcFile);
        else if (newWindow)
            win = LoadDocument(win->loadedFilePath);
    }
    
    if (!win || !win->IsDocLoaded())
        return next;
    if (!win->pdfsync) {
        DBG_OUT("PdfSync: No sync file loaded!\n");
        return next;
    }

    ack.fAck = 1;
    assert(win->IsDocLoaded());
    UINT page;
    Vec<RectI> rects;
    UINT ret = win->pdfsync->source_to_pdf(srcFile, line, col, &page, rects);
    win->ShowForwardSearchResult(srcFile, line, col, ret, page, rects);
    if (setFocus)
        SetFocusHelper(win->hwndFrame);

    return next;
}

// Open file DDE command, format:
// [<DDECOMMAND_OPEN>("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
static const TCHAR *HandleOpenCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile;
    BOOL newWindow = 0, setFocus = 0, forceRefresh = 0;
    const TCHAR *next = str::Parse(cmd, _T("[") DDECOMMAND_OPEN _T("(\"%S\")]"), &pdfFile);
    if (!next)
        next = str::Parse(cmd, _T("[") DDECOMMAND_OPEN _T("(\"%S\",%u,%u,%u)]"),
                          &pdfFile, &newWindow, &setFocus, &forceRefresh);
    if (!next)
        return NULL;
    
    WindowInfo *win = FindWindowInfoByFile(pdfFile);
    if (newWindow || !win)
        win = LoadDocument(pdfFile, !newWindow ? win : NULL);
    else if (win && !win->IsDocLoaded()) {
        win->Reload();
        forceRefresh = 0;
    }
    
    assert(!win || !win->IsAboutWindow());
    if (!win)
        return next;

    ack.fAck = 1;
    if (forceRefresh)
        win->Reload(true);
    if (setFocus)
        SetFocusHelper(win->hwndFrame);

    return next;
}

// Jump to named destination DDE command. Command format:
// [<DDECOMMAND_GOTO>("<pdffilepath>", "<destination name>")]
static const TCHAR *HandleGotoCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile, destName;
    const TCHAR *next = str::Parse(cmd, _T("[") DDECOMMAND_GOTO _T("(\"%S\",%? \"%S\")]"),
                                   &pdfFile, &destName);
    if (!next)
        return NULL;

    WindowInfo *win = FindWindowInfoByFile(pdfFile);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        win->Reload();
        if (!win->IsDocLoaded())
            return next;
    }

    win->linkHandler->GotoNamedDest(destName);
    ack.fAck = 1;
    SetFocusHelper(win->hwndFrame);
    return next;
}

// Jump to page DDE command. Format:
// [<DDECOMMAND_PAGE>("<pdffilepath>", <page number>)]
static const TCHAR *HandlePageCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile;
    UINT page;
    const TCHAR *next = str::Parse(cmd, _T("[") DDECOMMAND_PAGE _T("(\"%S\",%u)]"),
                                   &pdfFile, &page);
    if (!next)
        return false;

    // check if the PDF is already opened
    WindowInfo *win = FindWindowInfoByFile(pdfFile);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        win->Reload();
        if (!win->IsDocLoaded())
            return next;
    }

    if (!win->dm->validPageNo(page))
        return next;

    win->dm->goToPage(page, 0, true);
    ack.fAck = 1;
    SetFocusHelper(win->hwndFrame);
    return next;
}

// Set view mode and zoom level. Format:
// [<DDECOMMAND_SETVIEW>("<pdffilepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]
static const TCHAR *HandleSetViewCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile, viewMode;
    float zoom = INVALID_ZOOM;
    PointI scroll(-1, -1);
    const TCHAR *next = str::Parse(cmd, _T("[") DDECOMMAND_SETVIEW _T("(\"%S\",%? \"%S\",%f)]"),
                                   &pdfFile, &viewMode, &zoom);
    if (!next)
        next = str::Parse(cmd, _T("[") DDECOMMAND_SETVIEW _T("(\"%S\",%? \"%S\",%f,%d,%d)]"),
                          &pdfFile, &viewMode, &zoom, &scroll.x, &scroll.y);
    if (!next)
        return NULL;

    WindowInfo *win = FindWindowInfoByFile(pdfFile);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        win->Reload();
        if (!win->IsDocLoaded())
            return next;
    }

    DisplayMode mode;
    if (DisplayModeConv::EnumFromName(viewMode, &mode) && mode != DM_AUTOMATIC)
        win->SwitchToDisplayMode(mode);

    if (zoom != INVALID_ZOOM)
        win->ZoomToSelection(zoom, false);

    if (scroll.x != -1 || scroll.y != -1) {
        ScrollState ss = win->dm->GetScrollState();
        ss.x = scroll.x;
        ss.y = scroll.y;
        win->dm->SetScrollState(ss);
    }

    ack.fAck = 1;
    return next;
}

LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("Received WM_DDE_EXECUTE from %p with %08lx\n", (HWND)wparam, lparam);

    UINT_PTR lo, hi;
    UnpackDDElParam(WM_DDE_EXECUTE, lparam, &lo, &hi);
    DBG_OUT("%08lx => lo %04x hi %04x\n", lparam, lo, hi);

    ScopedMem<TCHAR> cmd;
    DDEACK ack;
    ack.bAppReturnCode = 0;
    ack.reserved = 0;
    ack.fBusy = 0;
    ack.fAck = 0;

    LPVOID command = GlobalLock((HGLOBAL)hi);
    if (!command) {
        DBG_OUT("WM_DDE_EXECUTE: No command specified\n");
        goto Exit;
    }

    if (IsWindowUnicode((HWND)wparam)) {
        DBG_OUT("The client window is UNICODE!\n");
        cmd.Set(str::conv::FromWStr((const WCHAR*)command));
    } else {
        DBG_OUT("The client window is ANSI!\n");
        cmd.Set(str::conv::FromAnsi((const char*)command));
    }

    const TCHAR *currCmd = cmd;
    while (!str::IsEmpty(currCmd)) {
        const TCHAR *nextCmd = NULL;
        if (!nextCmd) nextCmd = HandleSyncCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleOpenCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleGotoCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandlePageCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleSetViewCmd(currCmd, ack);
        if (!nextCmd) {
            DBG_OUT("WM_DDE_EXECUTE: unknown DDE command or bad command format\n");
            ScopedMem<TCHAR> tmp;
            nextCmd = str::Parse(currCmd, _T("%S]"), &tmp);
        }
        currCmd = nextCmd;
    }

Exit:
    GlobalUnlock((HGLOBAL)hi);

    DBG_OUT("Posting %s WM_DDE_ACK to %p\n", ack.fAck ? _T("ACCEPT") : _T("REJECT"), (HWND)wparam);
    lparam = ReuseDDElParam(lparam, WM_DDE_EXECUTE, WM_DDE_ACK, *(WORD *)&ack, hi);
    PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, lparam);
    return 0;
}

LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("Received WM_DDE_TERMINATE from %p with %08lx\n", (HWND)wparam, lparam);

    // Respond with another WM_DDE_TERMINATE message
    PostMessage((HWND)wparam, WM_DDE_TERMINATE, (WPARAM)hwnd, 0L);
    return 0;
}
