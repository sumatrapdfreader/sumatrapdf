// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source synchronizer based on .pdfsync file
// License: GPLv2


#include "PdfSync.h"
#include "SumatraPDF.h"

// convert a coordinate from the sync file into a PDF coordinate
#define SYNCCOORDINATE_TO_PDFCOORDINATE(c)          (c/65781.76)


int Pdfsync::get_record_section(int record_index)
{
    int leftsection = 0,
        rightsection = record_sections.size()-1;
    if(rightsection < 0)
        return -1; // no section in the table
    while(1){
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
    _ASSERT(0);
    return -1;
}

FILE *Pdfsync::opensyncfile()
{
    FILE *fp;
    errno_t err = fopen_s(&fp, syncfilename, "r");
    if(err!=0) {
        DBG_OUT("The file %s cannot be opened\n", syncfilename);
        return NULL;
    }
    return fp;
}

int Pdfsync::scan_and_build_index(FILE *fp)
{
    TCHAR jobname[_MAX_PATH];
    _ftscanf_s(fp, "%s\n", jobname, _countof(jobname)); // ignore the first line
    _tcscat(jobname, _T(".tex"));

    UINT versionNumber = 0;
    int ret = _ftscanf_s(fp, "version %u\n", &versionNumber);
    if (ret==EOF)
        return 1; // bad line format
    else if (versionNumber != 1)
        return 2; // unknown version

    srcfiles.clear();

    // add the initial tex file to the file stack
    src_file s;
    s.first_recordsection = -1;
    s.last_recordsection = -1;
    _tcscpy_s(s.filename, jobname);
#ifdef _DEBUG
    s.closeline_pos = -1;
    fgetpos(fp, &s.openline_pos);
#endif
    srcfiles.push_back(s);

    stack<int> incstack; // stack of included files
    incstack.push(srcfiles.size()-1);

    UINT cur_sheetNumber = -1;
    int cur_plinesec = -1; // index of the p-line-section currently being created.
    int cur_recordsec=-1; // l-line-section currenlty created
    record_sections.clear();
    pdfsheet_index.clear();


    CHAR filename[_MAX_PATH];
    fpos_t linepos;
    fgetpos(fp, &linepos);
    char c;
    while ((c = fgetc(fp)) && !feof(fp)) {
        if (c!='l' && cur_recordsec!=-1) { // if a section of contiguous 'l' lines finished then create the corresponding section
#if _DEBUG
            this->record_sections[cur_recordsec].endpos = linepos;
#endif
            cur_recordsec = -1;
        }
        if (c!='p' && cur_plinesec!=-1) { // if a section of contiguous 'p' lines finished then update the size of the corresponding p-line section
#if _DEBUG
            this->pline_sections[cur_plinesec].endpos = linepos;
#endif
            cur_plinesec = -1;
        }
        switch (c) {
        case '(': 
            {
                fscanf_s(fp, "%s\n", filename, _countof(filename));
                if( _tcsrchr(filename, '.') == NULL) // if the file name has no extension then add .tex at the end
                    _tcscat_s(filename, _T(".tex"));

                src_file s;
                s.first_recordsection = -1;
                s.last_recordsection = -1;
                _tcscpy_s(s.filename, filename);
#ifdef _DEBUG
                s.openline_pos = linepos;
                s.closeline_pos = -1;
#endif
                this->srcfiles.push_back(s);
                incstack.push(this->srcfiles.size()-1);
            }
            break;

        case ')':
#if _DEBUG
            if (incstack.top()!=-1)
                this->srcfiles[incstack.top()].closeline_pos = linepos;
#endif
            incstack.pop();
            fscanf_s(fp, "\n");
            break;
        case 'l':
            {
                UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
                if (fscanf_s(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber) <2)
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
#if _DEBUG
                    record_sections[cur_recordsec].highestrecord = recordNumber;
#endif
                    _ASSERT(incstack.top()!=-1);
                    if( this->srcfiles[incstack.top()].first_recordsection == -1 )
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
                fscanf_s(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);

                if (cur_plinesec==-1){ // section not initiated yet?
                    plines_section sec;
                    sec.startpos = linepos;
#if _DEBUG
                    sec.endpos = -1;
#endif
                    pline_sections.push_back(sec);
                    cur_plinesec = pline_sections.size()-1;

                    _ASSERT(cur_sheetNumber!=-1);
                    pdfsheet_index[cur_sheetNumber] = cur_plinesec;
                }
            }
            break;
        case 's':
            {
                fscanf_s(fp, " %u\n", &cur_sheetNumber);
                size_t nsheet = pdfsheet_index.size();
                if(cur_sheetNumber>=nsheet) {
                    pdfsheet_index.resize(cur_sheetNumber+1);
                    for(size_t s=nsheet;s<cur_sheetNumber;s++)
                        pdfsheet_index[s] = -1;
                }
                break;
            }
        default:
            DBG_OUT("Malformed pdfsync file: unknown command '%c'\n",c);;
            break;
        }
        fgetpos(fp, &linepos);
    }
#if _DEBUG
    if (cur_recordsec!=-1)
        this->record_sections[cur_recordsec].endpos = linepos;
    if (cur_plinesec!=-1)
        this->pline_sections[cur_plinesec].endpos = linepos;
#endif

    _ASSERT(incstack.size()==1);

    return 0;
}



int Pdfsync::rebuild_index()
{
    FILE *fp = opensyncfile();
    if(!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    scan_and_build_index(fp);
    fclose(fp);
    this->index_discarded = false;
    return 0;
}

UINT Pdfsync::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col)
{
    if( this->index_discarded )
        rebuild_index();

    FILE *fp = opensyncfile();
    if(!fp)
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;

    // distance to the closest pdf location (in the range <PDFSYNC_EPSILON_SQUARE)
    UINT closest_xydist=-1, 
         closest_xydist_record=-1;

    // If no record is found within a distance^2 of PDFSYNC_EPSILON_SQUARE
    // (closest_xydist_record==-1) then we pick up the record that is closest 
    // vertically to the hit-point.
    UINT closest_ydist=-1, // vertical distance between the hit point and the vertically-closest record
        closest_xdist=-1, // horizontal distance between the hit point and the vertically-closest record
        closest_ydist_record=-1; // vertically-closest record

    // find the entry in the index corresponding to this page
    if(sheet>=pdfsheet_index.size()) {
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
            fscanf_s(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
            // check whether it is closer that the closest point found so far
            UINT dx = abs((int)x - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition));
            UINT dy = abs((int)y - (int)SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition));
            UINT dist = dx*dx + dy*dy;
            if (dist<PDFSYNC_EPSILON_SQUARE && dist<closest_xydist) {
                closest_xydist_record = recordNumber;
                closest_xydist = dist;
            }
            else if ((closest_xydist == -1 )&& ( dy < PDFSYNC_EPSILON_Y ) && (dy < closest_ydist || (dy==closest_ydist && dx<closest_xdist))) {
                closest_ydist_record = recordNumber;
                closest_ydist = dy;
                closest_xdist = dx;
            }
            fgetpos(fp, &linepos);
        }
        _ASSERT(linepos == this->pline_sections[cur_psection].endpos);
    }

    int selected_record = closest_xydist_record!=-1 ? closest_xydist_record : closest_ydist_record;
    if (selected_record==-1) {
        fclose(fp);
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point
    }

    // We have a record number, we need to find its declaration ('l ...') in the syncfile

    // get the record section containing the record declaration
    int sec = this->get_record_section(selected_record);

    // get the file name from the record section
    _tcscpy_s(filename, cchFilename, this->srcfiles[record_sections[sec].srcfile].filename);

    // find the record declaration in the section
    fsetpos(fp, &record_sections[sec].startpos);
    bool found = false;
    while (!feof(fp) && !found) {
        UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
        int ret = fscanf_s(fp, "l %u %u %u\n", &recordNumber, &lineNumber, &columnNumber);
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
    _ASSERT(found);

    
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
UINT Pdfsync::source_to_record(FILE *fp, PCTSTR srcfilename, UINT line, UINT col, vector<size_t> &records)
{
    // find the source file entry
    size_t isrc=-1;
    for(size_t i=0; i<this->srcfiles.size();i++) {
        if (0==_tcsicmp(srcfilename, this->srcfiles[i].filename)) {
            isrc = i;
            break;
        }
    }
    if (isrc==-1)
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;

    src_file srcfile=this->srcfiles[isrc];

    if( srcfile.first_recordsection == -1 )
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = -1, // distance to the closest record
         closestrec = -1, // closest record
         closestrecline = -1; // closest record-line
    int c;
    for(size_t isec=srcfile.first_recordsection; isec<=srcfile.last_recordsection; isec++ ) {
        record_section &sec = this->record_sections[isec];
        // does this section belong to the desired file?
        if (sec.srcfile == isrc) {
            // scan the 'l' declarations of the section to find the specified line and column
            fsetpos(fp, &sec.startpos);
            while ((c = fgetc(fp))=='l' && !feof(fp)) {
                UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
                fscanf_s(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber);
                UINT d = abs((int)lineNumber-(int)line);
                if (d<EPSILON_LINE && d<min_distance) {
                    min_distance = d;
                    closestrec = recordNumber;
                    closestrecline = lineNumber;
                    if (d==0)
                        goto read_linerecords; // We have found a record for the requested line!
                }
            }
#if _DEBUG
            fpos_t linepos;
            fgetpos(fp, &linepos);
            _ASSERT(feof(fp) || (linepos-1 == sec.endpos));
#endif
        }
    }
    if (closestrec ==-1)
        return PDFSYNCERR_NORECORD_FOR_THATLINE;

read_linerecords:
    // we read all the consecutive records until we reach a record belonging to another line
    UINT recordNumber = closestrec, columnNumber, lineNumber;
    do {
        records.push_back(recordNumber);
        columnNumber = 0;
        lineNumber = 0;
        recordNumber = 0;
        fscanf_s(fp, "%c %u %u %u\n", &c, 1, &recordNumber, &lineNumber, &columnNumber);
    } while (c =='l' && !feof(fp) && (lineNumber==closestrecline) );
    return PDFSYNCERR_SUCCESS;

}

UINT Pdfsync::source_to_pdf(PCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y)
{
    if( this->index_discarded )
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
            if (this->pdfsheet_index[sheet]!=-1) {
                fsetpos(fp, &this->pline_sections[this->pdfsheet_index[sheet]].startpos);
                int c;
                while ((c = fgetc(fp))=='p' && !feof(fp)) {
                    // skip the optional star
                    if (fgetc(fp)=='*')
                        fgetc(fp);
                    // read the location
                    UINT recordNumber = 0, xPosition = 0, yPosition = 0;
                    fscanf_s(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
                    if (recordNumber == record) {
                        *page = sheet;
                        *x = SYNCCOORDINATE_TO_PDFCOORDINATE(xPosition);
                        *y = SYNCCOORDINATE_TO_PDFCOORDINATE(yPosition);
                        DBG_OUT("source->pdf: %s:%u -> record:%u -> page:%u, x:%u, y:%u\n", srcfilename, line, record, sheet, *x, *y);
                        fclose(fp);
                        return PDFSYNCERR_SUCCESS;
                    }
                }
    #if _DEBUG
                fpos_t linepos;
                fgetpos(fp, &linepos);
                _ASSERT(feof(fp) || (linepos-1==this->pline_sections[this->pdfsheet_index[sheet]].endpos));
    #endif

            }
        }
    }

    // the record does not correspond to any point in the PDF: this is possible...  
    fclose(fp);
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}


// Replace in 'pattern' the macros %f %l %c by 'filename', 'line' and 'col'
// the result is stored in cmdline
UINT Pdfsync::prepare_commandline(PCTSTR pattern, PCTSTR filename, UINT line, UINT col, PTSTR cmdline, UINT cchCmdline)
{
    PCTSTR perc;
    TCHAR buff[12];
    cmdline[0] = '\0';
    while (perc = _tcschr(pattern, '%')) {
        int u = perc-pattern;
        _tcsncat_s(cmdline, cchCmdline, pattern, u);
        perc++;
        if (*perc == 'f') {
            _tcscat_s(cmdline, cchCmdline, filename);
        }
        else if (*perc == 'l') {
            _itot_s(line, buff, _countof(buff), 10);
            _tcscat_s(cmdline, cchCmdline, buff);
        }
        else if (*perc == 'c') {
            _itot_s(col, buff, _countof(buff), 10);
            _tcscat_s(cmdline, cchCmdline, buff);
        }
        else
            _tcsncat_s(cmdline, cchCmdline, perc-1, 2);

        pattern = perc+1;
    }
    _tcscat_s(cmdline, cchCmdline, pattern);
    return 1;
}

///// DDE commands handling

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    DBG_OUT("received WM_DDE_INITIATE from %p with %08lx\n", (HWND)wparam, lparam);

    ATOM aServer = GlobalAddAtomW(PDFSYNC_DDE_SERVICE_W);
    ATOM aTopic = GlobalAddAtomW(PDFSYNC_DDE_TOPIC_W);

    if (LOWORD(lparam) == aServer && HIWORD(lparam) == aTopic) {
        if(IsWindowUnicode((HWND)wparam))
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

// DDE command for forward-search
// The command must be of the form:
//   [ForwardSearch("c:\file.pdf","c:\folder\source.tex",298)]
#define DDECOMMAND_SYNC_A         "ForwardSearch"
#define DDECOMMAND_SYNC_W         L"ForwardSearch"

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
        UINT line,col;
        int ret = sscanf_s(command, "[" DDECOMMAND_SYNC_A "(\"%[^\"]\",\"%[^\"]\",%u,%u)]", pdffile, _countof(pdffile), srcfile, _countof(srcfile), &line, &col);
        if (ret==EOF||ret<4)
            DBG_OUT("WM_DDE_EXECUTE: unknown DDE command or bad command format\n");
        else {
            // Execute the command.

            // check if the PDF is already opened
            WindowInfo *win = WindowInfoList_Find(pdffile);
            
            // if not then open it
            if (!win || WS_SHOWING_PDF != win->state)
                win = LoadPdf(pdffile);
            
            if (win && WS_SHOWING_PDF == win->state) {
                _ASSERT(win->dm);
                UINT page, x, y;
                UINT ret = win->pdfsync->source_to_pdf(srcfile, line, col, &page, &x, &y);
                WindowInfo_ShowForwardSearchResult(win, srcfile, line, col, ret, page, x, y);
            }
            ack.fAck = 1;
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