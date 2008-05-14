// Copyright William Blum 2008 http://william.famille-blum.org/
// PDF-source syncronizer based on .pdfsync file
// License: GPLv2


#include "PdfSync.h"


record2srcfile_node *Pdfsync::build_decision_tree(int leftrecord, int rightrecord)
{
    int n = rightrecord-leftrecord+1;
    // a single record?
    if (n == 1) {
        record2srcfile_leaf *leaf = new record2srcfile_leaf;
        leaf->header.type = Leaf;
        leaf->section = leftrecord;
        return (record2srcfile_node *)leaf;
    }
    else {
        int split = leftrecord + n / 2;
        record2srcfile_internal *node = new record2srcfile_internal;
        node->header.type = Internal;
        node->left = build_decision_tree(leftrecord,split-1);
        node->right = build_decision_tree(split,rightrecord);
        node->splitvalue = record_sections[split].firstrecord;
        return (record2srcfile_node *)node;
    }
}

void Pdfsync::delete_decision_tree(record2srcfile_node *root)
{
    _ASSERT(root);
    if (root->type == Leaf)
        delete (record2srcfile_leaf *)root;
    else {
        record2srcfile_internal *node = (record2srcfile_internal *)root;
        delete_decision_tree(node->left);
        delete_decision_tree(node->right);
        delete node;
    }
}


// get the index of the record section (in the array record_sections[]) containing a given record index
int Pdfsync::get_record_section(int record_index)
{
    _ASSERT( record2src_decitree );

    record2srcfile_node *cur = record2src_decitree;
    _ASSERT(cur);
    while (cur->type != Leaf) {
        record2srcfile_internal *node = (record2srcfile_internal *)cur;
        if (record_index >= node->splitvalue)
            cur = node->right;
        else
            cur = node->left;
    }
    return ((record2srcfile_leaf *)cur)->section;
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

    int_stack incstack; // stack of included files

    _tcscpy_s(s.filename, jobname);
    srcfiles.push_back(s);
    incstack.push(srcfiles.size()-1);

    pdfsheet_indexentry *cursheet = NULL; // pointer to the indexentry of the current pdf sheet being read
    record_section cursec; // section currenlty created
    cursec.srcfile = -1; // section not initiated
    record_sections.clear();
    pdfsheet_index.clear();

    CHAR filename[_MAX_PATH];
    fpos_t linepos;
    while (!feof(fp)) {
        fgetpos(fp, &linepos);
        char c = fgetc(fp);
        if (c!='l' && cursec.srcfile!=-1) { // if a section of contiguous 'l' lines finished then create the corresponding section
            record_sections.push_back(cursec);
            cursec.srcfile = -1;
        }
#if _DEBUG
        if (c!='p' && cursheet) { // if a section of contiguous 'p' lines finished then update the size of the corresponding sheet index entry
            cursheet->endpos = linepos;
            cursheet = NULL;
        }
#endif
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
                    if (cursec.srcfile == -1){ // section not initiated yet?
                        cursec.srcfile = incstack.top();
                        cursec.startpos = linepos;
                        cursec.firstrecord = recordNumber;
                    }
#if _DEBUG
                    cursec.highestrecord = recordNumber;
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
            }
            break;
        case 's':
            {
                UINT sheetNumber = 0;
                fscanf_s(fp, " %u\n", &sheetNumber);
                pdfsheet_indexentry entry;
                fgetpos(fp, &entry.startpos);
#if _DEBUG
                entry.endpos = -1;
#endif
                if(sheetNumber>=pdfsheet_index.size())
                    pdfsheet_index.resize(sheetNumber+1);

                pdfsheet_index[sheetNumber] = entry;
                cursheet = &pdfsheet_index[sheetNumber];
                break;
            }
        default:
            DBG_OUT("Malformed pdfsync file: unknown command '%c'\n",c);;
            break;
        }
    }
    if (cursec.srcfile != -1) {
        record_sections.push_back(cursec);
        cursec.srcfile = -1;
    }
    _ASSERT(incstack.size()==1);

    // build the decision tree for the function mapping record number to filenames
    int n = record_sections.size();
    if(record2src_decitree) {
        delete_decision_tree(record2src_decitree);
        record2src_decitree = NULL;
    }
    if (n>0)
        record2src_decitree = build_decision_tree(0, n-1);

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

    // vertical distance and 
    // this record will be used instead 

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

    // read the section of 'p' declarations (pdf locations)
    fsetpos(fp, &pdfsheet_index[sheet].startpos);
    fpos_t linepos;
    while (!feof(fp)) {
        fgetpos(fp, &linepos);
        char c = fgetc(fp);
        if (c=='(' || c==')' || c=='l' || c=='s') {
            _ASSERT(linepos == pdfsheet_index[sheet].endpos);
            break;
        }
        _ASSERT(c=='p'); // it's a pdf location
        // skip the optional *
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

