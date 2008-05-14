// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source syncronizer based on .pdfsync file
// License: GPLv2


#include "PdfSync.h"


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



int Pdfsync::scan_and_build_index(FILE *fp)
{
    TCHAR jobname[_MAX_PATH];
    _ftscanf_s(fp, "%s\n", jobname, _countof(jobname)); // ignore the first line
    UINT versionNumber = 0;
    TCHAR version[10];
    _ftscanf_s(fp, "%s %u\n", version, _countof(version), &versionNumber);
    if (versionNumber != 1)
        return 1;

    srcfiles.clear();

    // add the initial tex file to the file stack
    src_scope s;
    fgetpos(fp, &s.openline_pos);
    s.closeline_pos = -1;

    stack<int> incstack; // stack of included files

    _tcscpy_s(s.filename, jobname);
    srcfiles.push_back(s);
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
            this->record_sections[cur_recordsec].endpos = linepos;
            cur_recordsec = -1;
        }
        if (c!='p' && cur_plinesec!=-1) { // if a section of contiguous 'p' lines finished then update the size of the corresponding p-line section
            this->pline_sections[cur_plinesec].endpos = linepos;
            cur_plinesec = -1;
        }
        switch (c) {
        case '(': 
            {
                fscanf_s(fp, "%s\n", filename, _countof(filename));

                src_scope s;
                s.openline_pos = linepos;
                s.closeline_pos = -1;
                _tcscpy_s(s.filename, filename);
                this->srcfiles.push_back(s);
                incstack.push(this->srcfiles.size()-1);
            }
            break;

        case ')':
            if (incstack.top()!=-1)
                this->srcfiles[incstack.top()].closeline_pos = linepos;
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
                    sec.endpos = -1;
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
                if(cur_sheetNumber>=pdfsheet_index.size())
                    pdfsheet_index.resize(cur_sheetNumber+1);
                break;
            }
        default:
            DBG_OUT("Malformed pdfsync file: unknown command '%c'\n",c);;
            break;
        }
        fgetpos(fp, &linepos);
    }
    if (cur_recordsec!=-1)
        this->record_sections[cur_recordsec].endpos = linepos;
    if (cur_plinesec!=-1)
        this->pline_sections[cur_plinesec].endpos = linepos;

    _ASSERT(incstack.size()==1);

    return 0;
}



int Pdfsync::rebuild_index()
{
    FILE *fp;
    errno_t err;

    err = fopen_s(&fp, syncfilename, "r");
    if(err!=0) {
        DBG_OUT("The file %s cannot be opened\n", syncfilename);
        return 1;
    }
    scan_and_build_index(fp);
    fclose(fp);
    this->index_discarded = false;
    return 0;
}

void skipline(FILE *f)
{
    char c;
    while ('\n' != (c = fgetc(f)) && c != EOF)
        ;
}

UINT Pdfsync::pdf_to_source(UINT sheet, UINT x, UINT y, PTSTR filename, UINT cchFilename, UINT *line, UINT *col)
{
    if( this->index_discarded )
        rebuild_index();

    FILE *fp;
    errno_t err;

    err = fopen_s(&fp, syncfilename, "r");
    if(err!=0) {
        DBG_OUT("The file %s cannot be opened\n", syncfilename);
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }

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
        while ((c = fgetc(fp)) && !feof(fp) && linepos<this->pline_sections[cur_psection].endpos) {
            _ASSERT(c=='p'); // it's a pdf location
            // skip the optional star
            if (fgetc(fp)=='*')
                fgetc(fp);
            // read the location
            UINT recordNumber = 0, xPosition = 0, yPosition = 0;
            fscanf_s(fp, "%u %u %u\n", &recordNumber, &xPosition, &yPosition);
            // check whether it is closer that the closest point found so far
            UINT dx = abs((int)x - (int)(xPosition/65781.76));
            UINT dy = abs((int)y - (int)(yPosition/65781.76));
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
        fgetpos(fp, &linepos);
        char c = fgetc(fp);
        _ASSERT(c=='l'); // the section contains only record declaration lines
        UINT columnNumber = 0, lineNumber = 0, recordNumber = 0;
        if (fscanf_s(fp, " %u %u %u\n", &recordNumber, &lineNumber, &columnNumber) <2)
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

// return the first record found corresponding to the given source file, line number (and optionally column number)
UINT Pdfsync::source_to_record(PCTSTR srcfilename, UINT line, UINT col)
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

    src_scope scope=this->srcfiles[isrc];

    // Find the first section that is declared within the scope of the file
    int leftsection = 0,
        rightsection = record_sections.size()-1;
    int n = rightsection-leftsection+1;
    _ASSERT(rightsection>0);
    while(n>0){
        int split = leftsection + (n>>1);
        if (record_sections[split].endpos < scope.openline_pos)
            leftsection = split+1;
        else
            rightsection = split-1;
        n = rightsection-leftsection+1;
    }
    if( scope.openline_pos > this->record_sections[leftsection].endpos
        || this->record_sections[leftsection].startpos < scope.closeline_pos)
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file


    // look for sections belonging to the specified file
    size_t isec=leftsection;
    while( isec<this->record_sections.size() ) {
        record_section &sec = this->record_sections[isec];

        if(sec.startpos > scope.closeline_pos)
            break; // we have passed the last section in scope

        // does this section belong to the desired file?
        if( sec.srcfile == isrc ) {            
            // scan the 'l' declarations of the section to find the specified line and column
            // TODO

        }
        isec++;
    }

    return PDFSYNCERR_NORECORD_FOR_THATLINE;
}

UINT Pdfsync::source_to_pdf(PCTSTR srcfilename, UINT line, UINT col, UINT *page, UINT *x, UINT *y)
{
    // TODO
    return PDFSYNCERR_SUCCESS;
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
            if( _tcsrchr(filename, '.') == NULL) // if the file name has no extension then add .tex at the end
                _tcscat_s(cmdline, cchCmdline, _T(".tex"));
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

