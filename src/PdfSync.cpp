// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source synchronizer based on .pdfsync file
// License: GPLv2
#include "SumatraPDF.h"
#include "PdfSync.h"
#include <assert.h>
#include <stdio.h>
#include "tstr_util.h"
#include "str_util.h"
#include <sys/stat.h>
#include <shlwapi.h>

// convert a coordinate from the sync file into a PDF coordinate
#define SYNCCOORDINATE_TO_PDFCOORDINATE(c)          (c/65781.76)

// convert a PDF coordinate into a sync file coordinate
#define PDFCOORDINATE_TO_SYNCCOORDINATE(p)          (p*65781.76)

// Test if the file 'filename' exists
bool FileExists( LPCTSTR filename ) {
    struct _stat buffer ;
    return 0 == _tstat( filename, &buffer );
}

Synchronizer *CreateSynchronizer(LPCTSTR pdffilename)
{
    TCHAR syncfile[_MAX_PATH];
    size_t n = _tcslen(pdffilename);
    size_t u = dimof(PDF_EXTENSION)-1;
    if (n>u && _tcsicmp(pdffilename+(n-u), PDF_EXTENSION) == 0 ) {
        // Check if a PDFSYNC file is present
        tstr_copyn(syncfile, dimof(syncfile), pdffilename, n-u);
        tstr_cat_s(syncfile, dimof(syncfile), PDFSYNC_EXTENSION);
        if (FileExists(syncfile)) 
            return new Pdfsync(syncfile);

        #ifdef SYNCTEX_FEATURE
            // check if a compressed SYNCTEX file is present
            tstr_copyn(syncfile, dimof(syncfile), pdffilename, n-u);
            tstr_cat_s(syncfile, dimof(syncfile), SYNCTEXGZ_EXTENSION);
            bool exist = FileExists(syncfile);

            // check if a SYNCTEX file is present
            tstr_copyn(syncfile, dimof(syncfile), pdffilename, n-u);
            tstr_cat_s(syncfile, dimof(syncfile), SYNCTEX_EXTENSION);
            exist |= FileExists(syncfile);

            if(exist)
                return new SyncTex(syncfile); // due to a bug with synctex_parser.c, this must always be 
                                              // the path to the .synctex file (even if a .synctex.gz file is used instead)
        #endif
        return NULL;
    }
    else {
        DBG_OUT("Bad PDF filename! (%s)\n", pdffilename);
        return NULL;
    }
}

// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the result is stored in cmdline
UINT Synchronizer::prepare_commandline(LPCTSTR pattern, LPCTSTR filename, UINT line, UINT col, PTSTR cmdline, UINT cchCmdline)
{
    LPCTSTR perc;
    size_t len = 0;
    cmdline[0] = '\0';
    LPTSTR out = cmdline;
    size_t cchOut = cchCmdline;
    while (perc = tstr_find_char(pattern, '%')) {
        int u = perc-pattern;
        
        tstr_copyn(out, cchOut, pattern, u);
        len = tstr_len(out);
        out += len;
        cchOut -= len;

        perc++;
        if (*perc == 'f') {
            tstr_copy(out, cchOut, filename);
        }
        else if (*perc == 'l') {
            _sntprintf(out, cchOut, "%d", line);
        }
        else if (*perc == 'c') {
            _sntprintf(out, cchOut, "%d", col);
        }
        else {
            tstr_copyn(out, cchOut, perc-1, 2);
        }
        len = tstr_len(out);
        out += len;
        cchOut -= len;

        pattern = perc+1;
    }
    
    tstr_cat_s(cmdline, cchCmdline, pattern);

    return 1;
}

// PDFSYNC synchronizer
int Pdfsync::get_record_section(int record_index)
{
    int leftsection = 0,
        rightsection = record_sections.size()-1;
    if (rightsection < 0)
        return -1; // no section in the table
    while (1) {
        int n = rightsection-leftsection+1;
        // a single section?
        if (n == 1)
            return leftsection;
        else {
            int split = leftsection + (n>>1);
            int splitvalue = record_sections[split].firstrecord;
            if (record_index >= splitvalue)
                leftsection=split;
            else
                rightsection = split-1;
        }
    }
    assert(0);
    return -1;
}

FILE *Pdfsync::opensyncfile()
{
    FILE *fp;
    fp = fopen(syncfilename, "rb");
    if (NULL == fp) {
        DBG_OUT("The syncfile %s cannot be opened\n", syncfilename);
        return NULL;
    }
    return fp;
}

// read a line from a stream (exclude the end-of-line mark)
LPTSTR ftgetline(LPTSTR dst, size_t cchDst, FILE *fp)
{
    if (!_fgetts(dst, cchDst, fp))
        return NULL;

    LPTSTR end =  dst+tstr_len(dst)-1;
    while (*end == _T('\n') || *end == _T('\r'))
        *(end--) = 0;
    return dst;
}

int Pdfsync::scan_and_build_index(FILE *fp)
{
    TCHAR jobname[_MAX_PATH];
    
    ftgetline(jobname, dimof(jobname), fp); // get the job name from the first line
    // replace star by spaces (somehow tex replaces spaces by stars in the jobname)
    for(PTSTR rep = jobname; *rep; rep++) {
        if (*rep==_T('*'))
            *rep=_T(' ');
    }
    tstr_cat_s(jobname, dimof(jobname), _T(".tex")); 

    UINT versionNumber = 0;
    int ret = _ftscanf(fp, "version %u\n", &versionNumber);
    if (ret==EOF)
        return 1; // bad line format
    else if (versionNumber != 1)
        return 2; // unknown version

    srcfiles.clear();

    // add the initial tex file to the file stack
    src_file s;
    s.first_recordsection = (size_t)-1;
    s.last_recordsection = (size_t)-1;
    tstr_copy(s.filename, dimof(s.filename), jobname);
#ifndef NDEBUG    
    s.closeline_pos = -1;
    fgetpos(fp, &s.openline_pos);
#endif
    srcfiles.push_back(s);

    stack<size_t> incstack; // stack of included files
    incstack.push(srcfiles.size()-1);

    UINT cur_sheetNumber = (UINT)-1;
    int cur_plinesec = -1; // index of the p-line-section currently being created.
    int cur_recordsec=-1; // l-line-section currenlty created
    record_sections.clear();
    pdfsheet_index.clear();


    CHAR buff[_MAX_PATH];
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
                // read the filename
                ftgetline(buff, dimof(buff), fp);
                PTSTR pfilename = buff;
                int len = tstr_len(buff);
                // if the filename contains quotes then remove them
                if (str_startswithi(buff, "\"") && str_endswith_char(buff,'"')) {
                    pfilename++;
                    len-=2;
                }

                src_file s;
                s.first_recordsection = (size_t)-1;
                s.last_recordsection = (size_t)-1;
                tstr_copyn(s.filename, dimof(s.filename), pfilename, len);
                // if the file name extension is not specified then add the suffix '.tex'
                if (tstr_find_char(pfilename, '.') == NULL) {
                     tstr_cat_s(s.filename, dimof(s.filename), _T(".tex"));
                }
#ifndef NDEBUG
                s.openline_pos = linepos;
                s.closeline_pos = -1;
#endif
                this->srcfiles.push_back(s);
                incstack.push(this->srcfiles.size()-1);
            }
            break;

        case ')':
#ifndef NDEBUG
            if (incstack.top() != (size_t)-1)
                this->srcfiles[incstack.top()].closeline_pos = linepos;
#endif
            incstack.pop();
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
                        sec.srcfile = incstack.top();
                        sec.startpos = linepos;
                        sec.firstrecord = recordNumber;
                        record_sections.push_back(sec);
                        cur_recordsec = record_sections.size()-1;
                    }
#ifndef NDEBUG
                    record_sections[cur_recordsec].highestrecord = recordNumber;
#endif
                    assert(incstack.top() != (size_t)-1);
                    if (this->srcfiles[incstack.top()].first_recordsection == (size_t)-1)
                        this->srcfiles[incstack.top()].first_recordsection = cur_recordsec;
                    
                    this->srcfiles[incstack.top()].last_recordsection = cur_recordsec;
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
                    pline_sections.push_back(sec);
                    cur_plinesec = pline_sections.size()-1;

                    assert(cur_sheetNumber != (UINT)-1);
                    pdfsheet_index[cur_sheetNumber] = cur_plinesec;
                }
            }
            break;
        case 's':
            {
                fscanf(fp, " %u\n", &cur_sheetNumber);
                size_t maxsheet = pdfsheet_index.size();
                if (cur_sheetNumber>=maxsheet) {
                    pdfsheet_index.resize(cur_sheetNumber+1);
                    for(size_t s=maxsheet;s<=cur_sheetNumber;s++)
                        pdfsheet_index[s] = (size_t)-1;
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

    assert(incstack.size()==1);

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
    UINT closest_xydist = (UINT)-1,
        closest_xydist_record = (UINT)-1;

    // If no record is found within a distance^2 of PDFSYNC_EPSILON_SQUARE
    // (closest_xydist_record==-1) then we pick up the record that is closest 
    // vertically to the hit-point.
    UINT closest_ydist = (UINT)-1, // vertical distance between the hit point and the vertically-closest record
        closest_xdist = (UINT)-1, // horizontal distance between the hit point and the vertically-closest record
        closest_ydist_record = (UINT)-1; // vertically-closest record

    // find the entry in the index corresponding to this page
    if (sheet>=pdfsheet_index.size()) {
        fclose(fp);
        return PDFSYNCERR_INVALID_PAGE_NUMBER;
    }

    // read all the sections of 'p' declarations for this pdf sheet
    fpos_t linepos;
    for(size_t cur_psection = pdfsheet_index[sheet];
        (cur_psection<this->pline_sections.size())
        && ((sheet<pdfsheet_index.size()-1 && cur_psection < pdfsheet_index[sheet+1])
                || sheet==pdfsheet_index.size()-1) ;
        cur_psection++) {
        
        linepos = this->pline_sections[cur_psection].startpos;
        fsetpos(fp, &linepos);
        int c;
        while ((c = fgetc(fp))=='p' && !feof(fp)) {
            // skip the optional star
            if (fgetc(fp)=='*')
                fgetc(fp);
            // read the location
            UINT recordNumber = 0, xPosition = 0, yPosition = 0;
            fscanf(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
            // check whether it is closer that the closest point found so far
            UINT dx = abs((int)x - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition));
            UINT dy = abs((int)y - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition));
            UINT dist = dx*dx + dy*dy;
            if (dist<PDFSYNC_EPSILON_SQUARE && dist<closest_xydist) {
                closest_xydist_record = recordNumber;
                closest_xydist = dist;
            }
            else if ((closest_xydist == (UINT)-1) && ( dy < PDFSYNC_EPSILON_Y ) && (dy < closest_ydist || (dy==closest_ydist && dx<closest_xdist))) {                closest_ydist_record = recordNumber;
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
    char srcfilename[_MAX_PATH];
    tstr_copy(srcfilename, dimof(srcfilename), this->srcfiles[record_sections[sec].srcfile].filename);

    // Convert the source filepath to an absolute path
    if (PathIsRelative(srcfilename))
        _snprintf(srcfilepath, cchFilepath, "%s\\%s", this->dir, srcfilename, dimof(srcfilename));
    else
        str_copy(srcfilepath, cchFilepath, srcfilename);

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
UINT Pdfsync::source_to_record(FILE *fp, LPCTSTR srcfilename, UINT line, UINT col, vector<size_t> &records)
{
    // find the source file entry
    size_t isrc = (size_t)-1;
    for(size_t i=0; i<this->srcfiles.size();i++) {
        if (tstr_ieq(srcfilename, this->srcfiles[i].filename)) {
            isrc = i;
            break;
        }
    }
    if (isrc == (size_t)-1)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    src_file srcfile=this->srcfiles[isrc];

    if (srcfile.first_recordsection == (size_t)-1)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = (UINT)-1, // distance to the closest record
         closestrec = (UINT)-1, // closest record
         closestrecline = (UINT)-1; // closest record-line
    fpos_t closestrecline_filepos = -1; // position of the closest record-line in the file
    int c;
    for(size_t isec=srcfile.first_recordsection; isec<=srcfile.last_recordsection; isec++ ) {
        record_section &sec = this->record_sections[isec];
        // does this section belong to the desired file?
        if (sec.srcfile == isrc) {
            // scan the 'l' declarations of the section to find the specified line and column
            fpos_t linepos = sec.startpos;
            fsetpos(fp, &linepos);
            while ((c = fgetc(fp))=='l' && !feof(fp)) {
                UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
                fscanf(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber);
                UINT d = abs((int)lineNumber-(int)line);
                if (d<EPSILON_LINE && d<min_distance) {
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
        records.push_back(recordNumber);
        columnNumber = 0;
        lineNumber = 0;
        recordNumber = 0;
        fscanf(fp, "%c %u %u %u\n", &c, &recordNumber, &lineNumber, &columnNumber);
    } while (c =='l' && !feof(fp) && (lineNumber==closestrecline) );
    return PDFSYNCERR_SUCCESS;

}

UINT Pdfsync::source_to_pdf(LPCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y)
{
    if (this->is_index_discarded())
        rebuild_index();

    FILE *fp = opensyncfile();
    if (!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    vector<size_t> found_records;
    UINT ret = source_to_record(fp, srcfilename, line, col, found_records);
    if (ret!=PDFSYNCERR_SUCCESS || found_records.size() == 0 ) {
        DBG_OUT("source->pdf: %s:%u -> no record found, error:%u\n", srcfilename, line, ret);
        fclose(fp);
        return ret;
    }

    // records have been found for the desired source position:
    // we now find the pages and position in the PDF corresponding to the first record in the
    // list of record found
    for(size_t irecord=0;irecord<found_records.size();irecord++) {
        size_t record = found_records[irecord];
        for(size_t sheet=0;sheet<this->pdfsheet_index.size();sheet++) {
            if (this->pdfsheet_index[sheet] != (size_t)-1) {
                fsetpos(fp, &this->pline_sections[this->pdfsheet_index[sheet]].startpos);
                int c;
                while ((c = fgetc(fp))=='p' && !feof(fp)) {
                    // skip the optional star
                    if (fgetc(fp)=='*')
                        fgetc(fp);
                    // read the location
                    UINT recordNumber = 0, xPosition = 0, yPosition = 0;
                    fscanf(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
                    if (recordNumber == record) {
                        *page = sheet;
                        *x = (UINT)SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition);
                        *y = (UINT)SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition);
                        DBG_OUT("source->pdf: %s:%u -> record:%u -> page:%u, x:%u, y:%u\n", srcfilename, line, record, sheet, *x, *y);
                        fclose(fp);
                        return PDFSYNCERR_SUCCESS;
                    }
                }
    #ifndef NDEBUG
                fpos_t linepos;
                fgetpos(fp, &linepos);
                assert(feof(fp) || (linepos-1==this->pline_sections[this->pdfsheet_index[sheet]].endpos));
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
    this->scanner = synctex_scanner_new_with_output_file(this->syncfilename, NULL, 1);
    if (scanner)
        return Synchronizer::rebuild_index();
    else
        return 1; // cannot rebuild the index
}

UINT SyncTex::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR srcfilepath, UINT cchFilepath, UINT *line, UINT *col)
{
    if (this->is_index_discarded())
        if (rebuild_index())
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    if (synctex_edit_query(this->scanner,sheet,x,y)>0) {
        synctex_node_t node;
        char srcfilename[_MAX_PATH];
        while (node = synctex_next_result(this->scanner)) {
            *line = synctex_node_line(node);
            *col = synctex_node_column(node);
            str_copy( srcfilename, dimof(srcfilename), synctex_scanner_get_name(this->scanner,synctex_node_tag(node)));

            // Convert the source filepath to an absolute path
            if (PathIsRelative(srcfilename))
                _snprintf(srcfilepath, cchFilepath, "%s\\%s", this->dir, srcfilename, dimof(srcfilename));
            else
                str_copy(srcfilepath, cchFilepath, srcfilename);

            return PDFSYNCERR_SUCCESS;
        }
    }
    return PDFSYNCERR_NO_SYNC_AT_LOCATION;
//    return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
}

UINT SyncTex::source_to_pdf(LPCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y)
{
    if (this->is_index_discarded())
        if (rebuild_index())
            return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // convert the source file to an absolute path
    char srcfilepath[_MAX_PATH];
    if (PathIsRelative(srcfilename))
        _snprintf(srcfilepath, dimof(srcfilepath), "%s\\%s", this->dir, srcfilename, dimof(srcfilename));
    else
        str_copy(srcfilepath, dimof(srcfilepath), srcfilename);

    switch (synctex_display_query(this->scanner,srcfilepath,line,col)) {
        case -1:
            return PDFSYNCERR_UNKNOWN_SOURCEFILE;    
        case 0:
            return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
        default:
            synctex_node_t node;
            while (node = synctex_next_result(this->scanner)) {
                *page = synctex_node_page(node);
                *x = SYNCCOORDINATE_TO_PDFCOORDINATE(synctex_node_box_h(node));
                *y = SYNCCOORDINATE_TO_PDFCOORDINATE(synctex_node_box_v(node));
                return PDFSYNCERR_SUCCESS;
            }
            return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }
//#else
    //return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
//#endif
}
#endif


// DDE commands handling

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("received WM_DDE_INITIATE from %p with %08lx\n", (HWND)wparam, lparam);

    ATOM aServer = GlobalAddAtomW(PDFSYNC_DDE_SERVICE_W);
    ATOM aTopic = GlobalAddAtomW(PDFSYNC_DDE_TOPIC_W);

    if (LOWORD(lparam) == aServer && HIWORD(lparam) == aTopic) {
        if (IsWindowUnicode((HWND)wparam))
            DBG_OUT("The client window is ANSI!\n");
        DBG_OUT("Sending WM_DDE_ACK to %p\n", (HWND)wparam);
        SendMessageW((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, MAKELPARAM(aServer, 0));
    }
    else {
        GlobalDeleteAtom(aServer);
        GlobalDeleteAtom(aTopic);
    }
    return 0;
}

// DDE commands

LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("Received WM_DDE_EXECUTE from %p with %08lx\n", (HWND)wparam, lparam);

    UINT_PTR lo, hi;
    UnpackDDElParam(WM_DDE_EXECUTE, lparam, &lo, &hi);
    DBG_OUT("%08lx => lo %04x hi %04x\n", lparam, lo, hi);

    DDEACK ack;
    ack.bAppReturnCode = 0;
    ack.reserved = 0;
    ack.fBusy = 0;

    PCSTR command = (LPCSTR)GlobalLock((HGLOBAL)hi);
    ack.fAck = 0;
    if (!command)
        DBG_OUT("WM_DDE_EXECUTE: No command specified\n");
    else {
        // Parse the command
        char pdffile[_MAX_PATH];
        char srcfile[_MAX_PATH];
        char destname[_MAX_PATH];
        char dump[_MAX_PATH];
        UINT line,col, newwindow = 0, setfocus = 0, forcerefresh = 0;
        const char *pos;
        
        pos = command;
        while (pos < (command + strlen(command))) {
        // Synchronization command.
        // format is [<DDECOMMAND_SYNC_A>("<pdffile>","<srcfile>",<line>,<col>[,<newwindow>,<setfocus>])]
        if ( (pos = command) &&
            str_skip(&pos, "[" DDECOMMAND_SYNC_A "(\"") &&
            str_copy_skip_until(&pos, pdffile, dimof(pdffile), '"') &&
            str_skip(&pos, "\",\"") &&
            str_copy_skip_until(&pos, srcfile, dimof(srcfile), '"') &&
            (4 == sscanf(pos, "\",%u,%u,%u,%u)]", &line, &col, &newwindow, &setfocus)
            || 2 == sscanf(pos, "\",%u,%u)]", &line, &col))
            )
        {
            // Execute the command.

            // check if the PDF is already opened
            WindowInfo *win = WindowInfoList_Find(pdffile);
            
            // if not then open it
            if (newwindow || !win || WS_SHOWING_PDF != win->state)
                win = LoadPdf(pdffile);
            
            if (win && WS_SHOWING_PDF == win->state ) {
                if (!win->pdfsync)
                    DBG_OUT("PdfSync: No sync file loaded!\n");
                else {
                    ack.fAck = 1;
                    assert(win->dm);
                    UINT page, x, y;
                    UINT ret = win->pdfsync->source_to_pdf(srcfile, line, col, &page, &x, &y);
                    WindowInfo_ShowForwardSearchResult(win, srcfile, line, col, ret, page, x, y);
                    if (setfocus)
                        SetFocus(win->hwndFrame);

                }
            }
        }
        // Open file DDE command.
        // format is [<DDECOMMAND_OPEN_A>("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
        else if ( (pos = command) &&
            str_skip(&pos, "[" DDECOMMAND_OPEN_A "(\"") &&
            str_copy_skip_until(&pos, pdffile, dimof(pdffile), '"') &&
            (3 == sscanf(pos, "\",%u,%u,%u)]", &newwindow, &setfocus, &forcerefresh) || str_skip(&pos, "\")"))
            )
        {
            // check if the PDF is already opened
            WindowInfo *win = WindowInfoList_Find(pdffile);
            
            // if not then open it
            if ( newwindow || !win || WS_SHOWING_PDF != win->state)
                win = LoadPdf(pdffile);
            
            if (win && WS_SHOWING_PDF == win->state ) {
                ack.fAck = 1;
                if (forcerefresh)
                    PostMessage(win->hwndFrame, WM_CHAR, 'r', 0);
                if (setfocus)
                    SetFocus(win->hwndFrame);
            }
            
        }
        // Jump to named destination DDE command.
        // format is [<DDECOMMAND_GOTO_A>("<pdffilepath>", "<destination name>")]
        else if ( (pos = command) &&
            str_skip(&pos, "[" DDECOMMAND_GOTO_A "(\"") &&
            str_copy_skip_until(&pos, pdffile, dimof(pdffile), '"') &&
            str_skip(&pos, "\",\"") &&
            str_copy_skip_until(&pos, destname, dimof(destname), '"')
            )
        {
            // check if the PDF is already opened
            WindowInfo *win = WindowInfoList_Find(pdffile);
            
            if (win && WS_SHOWING_PDF == win->state) {
                win->dm->goToNamedDest(destname);
                ack.fAck = 1;
                SetFocus(win->hwndFrame);
            }
            
        }
        else
            DBG_OUT("WM_DDE_EXECUTE: unknown DDE command or bad command format\n");

        // next command
        str_copy_skip_until(&pos, dump, dimof(dump), ']');
        str_skip(&pos, "]");
        command = pos;
        }
    }
    GlobalUnlock((HGLOBAL)hi);

    DBG_OUT("Posting %s WM_DDE_ACK to %p\n", ack.fAck ? "ACCEPT" : "REJECT", (HWND)wparam);
    WORD status = * (WORD *) & ack;
    lparam = ReuseDDElParam(lparam, WM_DDE_EXECUTE, WM_DDE_ACK, status, hi);
    PostMessageW((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, lparam);
    return 0;
}

LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("Received WM_DDE_TERMINATE from %p with %08lx\n", (HWND)wparam, lparam);

    // Respond with another WM_DDE_TERMINATE message
    PostMessage((HWND)wparam, WM_DDE_TERMINATE, (WPARAM)hwnd, 0L);
    return 0;
}
