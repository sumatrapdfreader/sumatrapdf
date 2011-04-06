/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
// PDF-source synchronizer based on .pdfsync file

#include "BaseUtil.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "Resource.h"
#include "PdfSync.h"
#include <shlwapi.h>

// Synchronizer based on .sync file generated with the pdfsync tex package
class Pdfsync : public Synchronizer
{
public:
    Pdfsync(const TCHAR* _syncfilename) : Synchronizer(_syncfilename)
    {
        assert(Str::EndsWithI(_syncfilename, PDFSYNC_EXTENSION));
        this->coordsys = BottomLeft;
    }

    int rebuild_index();
    virtual UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    virtual UINT source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI>& rects);

private:
    int get_record_section(int record_index);
    int scan_and_build_index(FILE *fp);
    UINT source_to_record(FILE *fp, const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t>& records);
    FILE *opensyncfile();

private:
    Vec<size_t> pdfsheet_index; // pdfsheet_index[i] contains the index in pline_sections of the first pline section for that sheet
    Vec<plines_section> pline_sections;
    Vec<record_section> record_sections;
    Vec<src_file> srcfiles;
};

#ifdef SYNCTEX_FEATURE
#include <synctex_parser.h>

#define SYNCTEX_EXTENSION       _T(".synctex")
#define SYNCTEXGZ_EXTENSION     _T(".synctex.gz")

// Synchronizer based on .synctex file generated with SyncTex
class SyncTex : public Synchronizer
{
public:
    SyncTex(const TCHAR* _syncfilename) : Synchronizer(_syncfilename)
    {
        assert(Str::EndsWithI(_syncfilename, SYNCTEX_EXTENSION) ||
               Str::EndsWithI(_syncfilename, SYNCTEXGZ_EXTENSION));
        
        scanner = NULL;
        coordsys = TopLeft;
    }
    virtual ~SyncTex()
    {
        synctex_scanner_free(scanner);
    }

    UINT pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col);
    UINT source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects);
    int rebuild_index();

private:
    synctex_scanner_t scanner;
};
#endif

// convert a coordinate from the sync file into a PDF coordinate
#define SYNCCOORDINATE_TO_PDFCOORDINATE(c)          (c/65781.76)

// convert a PDF coordinate into a sync file coordinate
#define PDFCOORDINATE_TO_SYNCCOORDINATE(p)          (p*65781.76)

//
// Create a Synchronizer object for a PDF file.
//
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
//
int Synchronizer::Create(const TCHAR *pdffilename, Synchronizer **sync)
{
    if (!sync)
        return PDFSYNCERR_INVALID_ARGUMENT;

    if (!Str::EndsWithI(pdffilename, PDF_EXTENSION)) {
        DBG_OUT("Bad PDF filename! (%s)\n", pdffilename);
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    size_t baseLen = Str::Len(pdffilename) - Str::Len(PDF_EXTENSION);
    ScopedMem<TCHAR> baseName(Str::DupN(pdffilename, baseLen));

    // Check if a PDFSYNC file is present
    ScopedMem<TCHAR> syncFile(Str::Join(baseName, PDFSYNC_EXTENSION));
    if (File::Exists(syncFile)) {
        *sync = new Pdfsync(syncFile);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

#ifdef SYNCTEX_FEATURE
    // check if SYNCTEX or compressed SYNCTEX file is present
    ScopedMem<TCHAR> texGzFile(Str::Join(baseName, SYNCTEXGZ_EXTENSION));
    ScopedMem<TCHAR> texFile(Str::Join(baseName, SYNCTEX_EXTENSION));

    if (File::Exists(texGzFile) || File::Exists(texFile)) {
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
    Str::Str<TCHAR> cmdline(256);

    while ((perc = Str::FindChar(pattern, '%'))) {
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
int Pdfsync::get_record_section(int record_index)
{
    int leftsection = 0, rightsection = (int)record_sections.Count();
    if (rightsection == 0)
        return -1; // no section in the table
    rightsection--;
    while (1) {
        int n = rightsection - leftsection + 1;
        // a single section?
        if (n == 1)
            return leftsection;
        else {
            int split = leftsection + (n >> 1);
            int splitvalue = record_sections[split].firstrecord;
            if (record_index >= splitvalue)
                leftsection = split;
            else
                rightsection = split - 1;
        }
    }
    assert(0);
    return -1;
}

// TODO: maybe would be easier to load into memory and parse from there
FILE *Pdfsync::opensyncfile()
{
    FILE *fp = _tfopen(syncfilepath, _T("rb"));
    if (!fp)
        DBG_OUT("The syncfile %s cannot be opened\n", syncfilepath);
    return fp;
}

// read a line from a stream (exclude the end-of-line mark)
char* fgetline(char* dst, size_t cchDst, FILE *fp)
{
    if (!fgets(dst, (int)cchDst, fp))
        return NULL;

    Str::TransChars(dst, "\r\n", "\0\0");
    return dst;
}

int Pdfsync::scan_and_build_index(FILE *fp)
{
    char buf[_MAX_PATH];
    
    fgetline(buf, dimof(buf), fp); // get the job name from the first line
    // replace star by spaces (somehow tex replaces spaces by stars in the jobname)
    Str::TransChars(buf, "*", " ");
    ScopedMem<char> jobName(Str::Join(buf, ".tex"));

    UINT versionNumber = 0;
    int ret = fscanf(fp, "version %u\n", &versionNumber);
    if (ret == EOF)
        return 1; // bad line format
    if (versionNumber != 1)
        return 2; // unknown version

    srcfiles.Reset();

    // add the initial tex file to the file stack
    src_file s;
    s.first_recordsection = (size_t)-1;
    s.last_recordsection = (size_t)-1;
    Str::BufSet(s.filename, dimof(s.filename), jobName);
#ifndef NDEBUG    
    s.closeline_pos = -1;
    fgetpos(fp, &s.openline_pos);
#endif
    srcfiles.Push(s);

    Vec<size_t> incstack; // stack of included files
    incstack.Push(srcfiles.Count() - 1);

    UINT cur_sheetNumber = (UINT)-1;
    int cur_plinesec = -1; // index of the p-line-section currently being created.
    int cur_recordsec= -1; // l-line-section currenlty created
    record_sections.Reset();
    pdfsheet_index.Reset();

    fpos_t linepos;
    fgetpos(fp, &linepos);
    char c;
    while ((c = fgetc(fp)) && !feof(fp)) {
        if (c!='l' && cur_recordsec!=-1) { // if a section of contiguous 'l' lines finished then create the corresponding section
#ifndef NDEBUG
            this->record_sections[cur_recordsec].endpos = linepos;
#endif
            cur_recordsec = -1;
        }
        if (c!='p' && cur_plinesec!=-1) { // if a section of contiguous 'p' lines finished then update the size of the corresponding p-line section
#ifndef NDEBUG
            this->pline_sections[cur_plinesec].endpos = linepos;
#endif
            cur_plinesec = -1;
        }
        switch (c) {
        case '(': 
            {
                src_file s;
                s.first_recordsection = (size_t)-1;
                s.last_recordsection = (size_t)-1;

                // read the filename
                fgetline(s.filename, dimof(s.filename), fp);

                CASSERT(sizeof(s.filename) == MAX_PATH, sufficient_path_length);
                // if the filename contains quotes then remove them
                PathUnquoteSpacesA(s.filename);
                // if the file name extension is not specified then add the suffix '.tex'
                if (!Str::FindChar(s.filename, '.'))
                    PathAddExtensionA(s.filename, ".tex");
#ifndef NDEBUG
                s.openline_pos = linepos;
                s.closeline_pos = -1;
#endif
                this->srcfiles.Push(s);
                incstack.Push(this->srcfiles.Count() - 1);
            }
            break;

        case ')':
#ifndef NDEBUG
            if (incstack.Last() != (size_t)-1)
                this->srcfiles[incstack.Last()].closeline_pos = linepos;
#endif
            incstack.Pop();
            fscanf(fp, "\n");
            break;
        case 'l':
            {
                UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
                if (fscanf(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber) <2)
                    DBG_OUT("Bad 'l' line in the pdfsync file\n");
                else {
                    if (cur_recordsec==-1){ // section not initiated yet?
                        record_section sec;
                        sec.srcfile = incstack.Last();
                        sec.startpos = linepos;
                        sec.firstrecord = recordNumber;
                        record_sections.Push(sec);
                        cur_recordsec = (int)record_sections.Count() - 1;
                    }
#ifndef NDEBUG
                    record_sections[cur_recordsec].highestrecord = recordNumber;
#endif
                    assert(incstack.Last() != (size_t)-1);
                    if (this->srcfiles[incstack.Last()].first_recordsection == (size_t)-1)
                        this->srcfiles[incstack.Last()].first_recordsection = cur_recordsec;
                    
                    this->srcfiles[incstack.Last()].last_recordsection = cur_recordsec;
                }
            }
            break;
        case 'p':
            {
                if (fgetc(fp)=='*')
                    fgetc(fp);

                UINT recordNumber = 0, xPosition = 0, yPosition = 0;
                fscanf(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);

                if (cur_plinesec==-1){ // section not initiated yet?
                    plines_section sec;
                    sec.startpos = linepos;
#ifndef NDEBUG
                    sec.endpos = -1;
#endif
                    pline_sections.Push(sec);
                    cur_plinesec = (int)pline_sections.Count() - 1;

                    assert(cur_sheetNumber != (UINT)-1);
                    pdfsheet_index[cur_sheetNumber] = cur_plinesec;
                }
            }
            break;
        case 's':
            {
                fscanf(fp, " %u\n", &cur_sheetNumber);
                size_t maxsheet = pdfsheet_index.Count();
                if (cur_sheetNumber >= maxsheet) {
                    for (size_t s = maxsheet; s <= cur_sheetNumber; s++)
                        pdfsheet_index.Push((size_t)-1);
                }
                break;
            }
        default:
            DBG_OUT("Malformed pdfsync file: unknown command '%c'\n",c);;
            break;
        }
        fgetpos(fp, &linepos);
    }
#ifndef NDEBUG
    if (cur_recordsec!=-1)
        this->record_sections[cur_recordsec].endpos = linepos;
    if (cur_plinesec!=-1)
        this->pline_sections[cur_plinesec].endpos = linepos;
#endif

    assert(incstack.Count() == 1);

    return 0;
}

int Pdfsync::rebuild_index()
{
    FILE *fp = opensyncfile();
    if (!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    scan_and_build_index(fp);
    fclose(fp);

    return Synchronizer::rebuild_index();
}

UINT Pdfsync::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col)
{
    if (this->is_index_discarded())
        rebuild_index();

    FILE *fp = opensyncfile();
    if (!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // distance to the closest pdf location (in the range <PDFSYNC_EPSILON_SQUARE)
    UINT closest_xydist = (UINT)-1;
    UINT closest_xydist_record = (UINT)-1;

    // If no record is found within a distance^2 of PDFSYNC_EPSILON_SQUARE
    // (closest_xydist_record==-1) then we pick up the record that is closest 
    // vertically to the hit-point.
    UINT closest_ydist = (UINT)-1; // vertical distance between the hit point and the vertically-closest record
    UINT closest_xdist = (UINT)-1; // horizontal distance between the hit point and the vertically-closest record
    UINT closest_ydist_record = (UINT)-1; // vertically-closest record

    // find the entry in the index corresponding to this page
    if (sheet >= pdfsheet_index.Count()) {
        fclose(fp);
        return PDFSYNCERR_INVALID_PAGE_NUMBER;
    }

    // read all the sections of 'p' declarations for this pdf sheet
    fpos_t linepos;
    for (size_t cur_psection = pdfsheet_index[sheet];
         (cur_psection < this->pline_sections.Count()) &&
         ((sheet < pdfsheet_index.Count() - 1 && cur_psection < pdfsheet_index[sheet + 1]) ||
          sheet == pdfsheet_index.Count() - 1);
         cur_psection++) {
        
        linepos = this->pline_sections[cur_psection].startpos;
        fsetpos(fp, &linepos);
        int c;
        while ((c = fgetc(fp)) == 'p' && !feof(fp)) {
            // skip the optional star
            if (fgetc(fp)=='*')
                fgetc(fp);
            // read the location
            UINT recordNumber = 0, xPosition = 0, yPosition = 0;
            fscanf(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
            // check whether it is closer that the closest point found so far
            UINT dx = abs((int)x - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition));
            UINT dy = abs((int)y - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition));
            UINT dist = dx * dx + dy * dy;
            if (dist < PDFSYNC_EPSILON_SQUARE && dist < closest_xydist) {
                closest_xydist_record = recordNumber;
                closest_xydist = dist;
            }
            else if ((closest_xydist == (UINT)-1) && dy < PDFSYNC_EPSILON_Y &&
                     (dy < closest_ydist || (dy == closest_ydist && dx < closest_xdist))) {
                closest_ydist_record = recordNumber;
                closest_ydist = dy;
                closest_xdist = dx;
            }
            fgetpos(fp, &linepos);
        }
        assert(linepos == this->pline_sections[cur_psection].endpos);
    }

    UINT selected_record = closest_xydist_record!=(UINT)-1 ? closest_xydist_record : closest_ydist_record;
    if (selected_record == (UINT)-1) {
        fclose(fp);
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point
    }

    // We have a record number, we need to find its declaration ('l ...') in the syncfile

    // get the record section containing the record declaration
    int sec = this->get_record_section(selected_record);

    // get the file name from the record section
    char *srcFilenameA = this->srcfiles[record_sections[sec].srcfile].filename;
    ScopedMem<TCHAR> srcFilename(Str::Conv::FromAnsi(srcFilenameA));
    // Convert the source filepath to an absolute path
    if (PathIsRelative(srcFilename))
        srcFilename.Set(Path::Join(this->dir, srcFilename));
    Str::BufSet(srcfilepath, cchFilepath, srcFilename);

    // find the record declaration in the section
    fsetpos(fp, &record_sections[sec].startpos);
    bool found = false;
    while (!feof(fp) && !found) {
        UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
        int ret = fscanf(fp, "l %u %u %u\n", &recordNumber, &lineNumber, &columnNumber);
        if (ret==EOF || ret<2)
            DBG_OUT("Bad 'l' line in the pdfsync file\n");
        else {
            if (recordNumber == selected_record) {
                *line = lineNumber;
                *col = columnNumber;
                found = true;
            }
        }
    }
    assert(found);

    
    fclose(fp);
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
//
UINT Pdfsync::source_to_record(FILE *fp, const TCHAR* srcfilename, UINT line, UINT col, Vec<size_t> &records)
{
    if (!srcfilename)
        return PDFSYNCERR_INVALID_ARGUMENT;

    char *mb_srcfilename = Str::Conv::ToAnsi(srcfilename);
    if (!mb_srcfilename)
        return PDFSYNCERR_OUTOFMEMORY;

    // find the source file entry
    size_t isrc = (size_t)-1;
    for (size_t i = 0; i < this->srcfiles.Count(); i++) {
        if (Str::EqI(mb_srcfilename, this->srcfiles[i].filename)) {
            isrc = i;
            break;
        }
    }
    free(mb_srcfilename);
    if (isrc == (size_t)-1)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    src_file srcfile = this->srcfiles[isrc];

    if (srcfile.first_recordsection == (size_t)-1)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = (UINT)-1; // distance to the closest record
    UINT closestrec = (UINT)-1; // closest record
    UINT closestrecline = (UINT)-1; // closest record-line
    fpos_t closestrecline_filepos = -1; // position of the closest record-line in the file
    int c;
    for (size_t isec=srcfile.first_recordsection; isec<=srcfile.last_recordsection; isec++ ) {
        record_section &sec = this->record_sections[isec];
        // does this section belong to the desired file?
        if (sec.srcfile == isrc) {
            // scan the 'l' declarations of the section to find the specified line and column
            fpos_t linepos = sec.startpos;
            fsetpos(fp, &linepos);
            while ((c = fgetc(fp)) == 'l' && !feof(fp)) {
                UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
                fscanf(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber);
                UINT d = abs((int)lineNumber - (int)line);
                if (d < EPSILON_LINE && d < min_distance) {
                    min_distance = d;
                    closestrec = recordNumber;
                    closestrecline = lineNumber;
                    closestrecline_filepos = linepos;
                    if (d==0)
                        goto read_linerecords; // We have found a record for the requested line!
                }
                fgetpos(fp, &linepos);
            }
#ifndef NDEBUG
            assert(feof(fp) || (linepos == sec.endpos));
#endif
        }
    }
    if (closestrec == (UINT)-1)
        return PDFSYNCERR_NORECORD_FOR_THATLINE;

read_linerecords:
    // we read all the consecutive records until we reach a record belonging to another line
    UINT recordNumber = closestrec, columnNumber, lineNumber;
    fsetpos(fp, &closestrecline_filepos);
    do {
        records.Push(recordNumber);
        columnNumber = 0;
        lineNumber = 0;
        recordNumber = 0;
        fscanf(fp, "%c %u %u %u\n", &c, &recordNumber, &lineNumber, &columnNumber);
    } while (c == 'l' && !feof(fp) && (lineNumber == closestrecline));
    return PDFSYNCERR_SUCCESS;

}

UINT Pdfsync::source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (this->is_index_discarded())
        rebuild_index();

    FILE *fp = opensyncfile();
    if (!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    Vec<size_t> found_records;
    UINT ret = source_to_record(fp, srcfilename, line, col, found_records);
    if (ret != PDFSYNCERR_SUCCESS || found_records.Count() == 0) {
        DBG_OUT("source->pdf: %s:%u -> no record found, error:%u\n", srcfilename, line, ret);
        fclose(fp);
        return ret;
    }

    // records have been found for the desired source position:
    // we now find the pages and position in the PDF corresponding to the first record in the
    // list of record found
    for (size_t irecord = 0; irecord < found_records.Count(); irecord++) {
        size_t record = found_records[irecord];
        for (size_t sheet = 0; sheet < this->pdfsheet_index.Count(); sheet++) {
            if (this->pdfsheet_index[sheet] != (size_t)-1) {
                fsetpos(fp, &this->pline_sections[this->pdfsheet_index[sheet]].startpos);
                int c;
                while ((c = fgetc(fp)) == 'p' && !feof(fp)) {
                    // skip the optional star
                    if (fgetc(fp) == '*')
                        fgetc(fp);
                    // read the location
                    UINT recordNumber = 0, xPosition = 0, yPosition = 0;
                    fscanf(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
                    if (recordNumber == record) {
                        *page = (UINT)sheet;
                        rects.Reset();
                        RectI rc((int)SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition),
                                 (int)SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition),
                                 MARK_SIZE, MARK_SIZE);
                        rects.Push(rc);
                        DBG_OUT("source->pdf: %s:%u -> record:%u -> page:%u, x:%u, y:%u\n",
                            srcfilename, line, record, sheet, rc.x, rc.y);
                        fclose(fp);
                        return PDFSYNCERR_SUCCESS;
                    }
                }
#ifndef NDEBUG
                fpos_t linepos;
                fgetpos(fp, &linepos);
                assert(feof(fp) || (linepos - 1 == this->pline_sections[this->pdfsheet_index[sheet]].endpos));
#endif

            }
        }
    }

    // the record does not correspond to any point in the PDF: this is possible...  
    fclose(fp);
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}


// SYNCTEX synchronizer

#ifdef SYNCTEX_FEATURE

int SyncTex::rebuild_index() {
    if (this->scanner)
        synctex_scanner_free(this->scanner);

    char *mb_syncfname = Str::Conv::ToAnsi(this->syncfilepath);
    if (mb_syncfname==NULL)
        return PDFSYNCERR_OUTOFMEMORY;

    this->scanner = synctex_scanner_new_with_output_file(mb_syncfname, NULL, 1);
    free(mb_syncfname);

    if (!scanner)
        return 1; // cannot rebuild the index
    return Synchronizer::rebuild_index();
}

UINT SyncTex::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col)
{
    if (this->is_index_discarded())
        if (rebuild_index())
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    if (synctex_edit_query(this->scanner, sheet, (float)x, (float)y) < 0)
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
        
    synctex_node_t node;
    while (node = synctex_next_result(this->scanner)) {
        *line = synctex_node_line(node);
        *col = synctex_node_column(node);
        const char *name = synctex_scanner_get_name(this->scanner,synctex_node_tag(node));
        ScopedMem<TCHAR> srcfilename(Str::Conv::FromAnsi(name));
        if (!srcfilename)
            return PDFSYNCERR_OUTOFMEMORY;

        // undecorate the filepath: replace * by space and / by \ 
        Str::TransChars(srcfilename, _T("*/"), _T(" \\"));
        // Convert the source filepath to an absolute path
        if (PathIsRelative(srcfilename))
            srcfilename.Set(Path::Join(this->dir, srcfilename));

        Str::BufSet(srcfilepath, cchFilepath, srcfilename);
        return PDFSYNCERR_SUCCESS;
    }
    return PDFSYNCERR_NO_SYNC_AT_LOCATION;
}

UINT SyncTex::source_to_pdf(const TCHAR* srcfilename, UINT line, UINT col, UINT *page, Vec<RectI> &rects)
{
    if (this->is_index_discarded())
        if (rebuild_index())
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    ScopedMem<TCHAR> srcfilepath(NULL);

    // convert the source file to an absolute path
    if (PathIsRelative(srcfilename))
        srcfilepath.Set(Path::Join(dir, srcfilename));
    else
        srcfilepath.Set(Str::Dup(srcfilename));

    char *mb_srcfilepath = Str::Conv::ToAnsi(srcfilepath);
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
// [<DDECOMMAND_SYNC>("<pdffile>","<srcfile>",<line>,<col>[,<newwindow>,<setfocus>])]
static const TCHAR *HandleSyncCmd(const TCHAR *cmd, DDEACK& ack)
{
    ScopedMem<TCHAR> pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    const TCHAR *next = Str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%? \"%S\",%u,%u)]"),
                                   &pdfFile, &srcFile, &line, &col);
    if (!next)
        next = Str::Parse(cmd, _T("[") DDECOMMAND_SYNC _T("(\"%S\",%? \"%S\",%u,%u,%u,%u)]"),
                          &pdfFile, &srcFile, &line, &col, &newWindow, &setFocus);
    if (!next)
        return NULL;

    // check if the PDF is already opened
    WindowInfo *win = FindWindowInfoByFile(pdfFile);
    // if not then open it
    if (newWindow || !win)
        win = LoadDocument(pdfFile, !newWindow ? win : NULL);
    else if (win && !win->IsDocLoaded())
        win->Reload();
    
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
    const TCHAR *next = Str::Parse(cmd, _T("[") DDECOMMAND_OPEN _T("(\"%S\")]"), &pdfFile);
    if (!next)
        next = Str::Parse(cmd, _T("[") DDECOMMAND_OPEN _T("(\"%S\",%u,%u,%u)]"),
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
    const TCHAR *next = Str::Parse(cmd, _T("[") DDECOMMAND_GOTO _T("(\"%S\",%? \"%S\")]"),
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

    ScopedMem<char> destNameUtf8(Str::Conv::ToUtf8(destName));
    if (!destNameUtf8)
        return next;

    win->dm->goToNamedDest(destNameUtf8);
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
    const TCHAR *next = Str::Parse(cmd, _T("[") DDECOMMAND_PAGE _T("(\"%S\",%u)]"),
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
    const TCHAR *next = Str::Parse(cmd, _T("[") DDECOMMAND_SETVIEW _T("(\"%S\",%? \"%S\",%f)]"),
                                   &pdfFile, &viewMode, &zoom);
    if (!next)
        next = Str::Parse(cmd, _T("[") DDECOMMAND_SETVIEW _T("(\"%S\",%? \"%S\",%f,%d,%d)]"),
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

    ScopedMem<char> viewModeUtf8(Str::Conv::ToUtf8(viewMode));
    DisplayMode mode;
    if (DisplayModeEnumFromName(viewModeUtf8, &mode) && mode != DM_AUTOMATIC)
        win->SwitchToDisplayMode(mode);

    if (zoom != INVALID_ZOOM)
        win->ZoomToSelection(zoom, false);

    if (scroll.x != -1 || scroll.y != -1) {
        ScrollState ss;
        if (win->dm->getScrollState(&ss)) {
            ss.x = scroll.x;
            ss.y = scroll.y;
            win->dm->setScrollState(&ss);
        }
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
        cmd.Set(Str::Conv::FromWStr((const WCHAR*)command));
    } else {
        DBG_OUT("The client window is ANSI!\n");
        cmd.Set(Str::Conv::FromAnsi((const char*)command));
    }

    const TCHAR *currCmd = cmd;
    while (!Str::IsEmpty(currCmd)) {
        const TCHAR *nextCmd = NULL;
        if (!nextCmd) nextCmd = HandleSyncCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleOpenCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleGotoCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandlePageCmd(currCmd, ack);
        if (!nextCmd) nextCmd = HandleSetViewCmd(currCmd, ack);
        if (!nextCmd) {
            DBG_OUT("WM_DDE_EXECUTE: unknown DDE command or bad command format\n");
            ScopedMem<TCHAR> tmp;
            nextCmd = Str::Parse(currCmd, _T("%S]"), &tmp);
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
